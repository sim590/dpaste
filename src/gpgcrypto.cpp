/*
 * Copyright © 2017 Simon Désaulniers
 * Author: Simon Désaulniers <sim.desaulniers@gmail.com>
 *
 * This file is part of dpaste.
 *
 * dpaste is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dpaste is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dpaste.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <array>
#include <sstream>

#include <gpgme++/key.h>
#include <gpgme++/data.h>
#include <gpgme.h>

#include "gpgcrypto.h"

static constexpr const size_t BUFLEN = 1024;

namespace dpaste {

std::vector<uint8_t> dataToVector(GpgME::Data& d) {
    d.seek(0, SEEK_SET);
    std::array<uint8_t, BUFLEN> buf;
    std::vector<uint8_t> v;

    ssize_t bytes = 0;
    while ((bytes = d.read(buf.data(), BUFLEN)) > 0)
        v.insert(v.end(), buf.begin(), buf.begin()+bytes);
    return v;
}

GPGCrypto::GPGCrypto(std::string signer) : ctx(GpgME::Context::createForProtocol(GpgME::Protocol::OpenPGP)) {
    GpgME::initializeLibrary();
    err = GpgME::checkEngine(GpgME::Protocol::OpenPGP);
    if (err.code() != GPG_ERR_NO_ERROR)
        throw GpgME::Exception(err, "Failed to initialize OpenPGP engine");
    ctx = std::unique_ptr<GpgME::Context>(GpgME::Context::createForProtocol(GpgME::Protocol::OpenPGP));
    ctx->setArmor(1);
    if (not signer.empty())
        ctx->addSigningKey(getKey(signer));
}

std::tuple<std::vector<uint8_t>,
    GpgME::EncryptionResult,
    GpgME::SigningResult>
GPGCrypto::encrypt(std::string recipient, std::vector<uint8_t> plain_text, bool sign) {
    if (not ctx or (sign and ctx->signingKeys().empty()))
        return {};

    /* Adding final null char delimiter to data */
    plain_text.push_back('\0');
    GpgME::Data pt {reinterpret_cast<const char*>(plain_text.data()), plain_text.size()};
    GpgME::Data cipher {};

    auto k = getKey(recipient);
    GpgME::EncryptionResult enc_res;
    GpgME::SigningResult sign_res;
    if (sign) {
        auto res = ctx->signAndEncrypt({k}, pt, cipher, GpgME::Context::EncryptionFlags::None);
        res.first.swap(sign_res);
        res.second.swap(enc_res);
    } else {
        auto res = ctx->encrypt({k}, pt, cipher, GpgME::Context::EncryptionFlags::None);
        res.swap(enc_res);
    }
    /* Adding final null char delimiter to cipher */
    cipher.write("\0", 1);

    if (enc_res.error())
        throw GpgME::Exception(
                enc_res.error(), "Failed to encrypt with key of ID "+recipient);

    if (not sign_res.isNull() and sign_res.error())
        throw GpgME::Exception(
                sign_res.error(),
                "Failed to sign with key of ID "+std::string{ctx->signingKey(0).primaryFingerprint()});

    return std::make_tuple(dataToVector(cipher), std::move(enc_res), std::move(sign_res));
}

std::tuple<std::vector<uint8_t>,
    GpgME::DecryptionResult,
    GpgME::VerificationResult>
GPGCrypto::decryptAndVerify(const std::vector<uint8_t>& cipher) {
    if (not ctx)
        return {};

    GpgME::Data c {reinterpret_cast<const char*>(cipher.data()), cipher.size()};
    GpgME::Data pt {};
    auto res = ctx->decryptAndVerify(c, pt);
    auto& dec_res = res.first;
    auto& sign_res = res.second;

    if (dec_res.error())
        throw GpgME::Exception(dec_res.error(), "Failed to decrypt cipher");
    if (sign_res.error())
        throw GpgME::Exception(sign_res.error(), "Failed to verify signature");

    return std::make_tuple(dataToVector(pt), std::move(res.first), std::move(res.second));
}

std::pair<std::vector<uint8_t>,
    GpgME::SigningResult>
GPGCrypto::sign(const std::vector<uint8_t>& plain_text) {
    if (not ctx or ctx->signingKeys().empty())
        return {};

    GpgME::Data pt {reinterpret_cast<const char*>(plain_text.data()), plain_text.size()};
    GpgME::Data signature;
    auto res = ctx->sign(pt, signature, GpgME::SignatureMode::NormalSignatureMode);

    if (res.error())
        throw GpgME::Exception(
                res.error(),
                "Failed to sign with key of ID "+std::string{ctx->signingKey(0).primaryFingerprint()});

    return std::make_pair(dataToVector(signature), std::move(res));
}

GpgME::VerificationResult
GPGCrypto::verify(const std::vector<uint8_t>& signature, const std::vector<uint8_t>& plain_text) {
    if (not ctx)
        return {};

    GpgME::Data pt {reinterpret_cast<const char*>(plain_text.data()), plain_text.size()};
    GpgME::Data sig {reinterpret_cast<const char*>(signature.data()), signature.size()};
    auto res = ctx->verifyOpaqueSignature(sig, pt);

    /* TODO: pas suffisant pour vérifier... VerificationResult.error() ne détecte pas que la verif a manqué. Par
     * exemple, il ne détectait pas quand je faisais verifyDetatchedSignature. Je reçevait un mauvais résultat, mais
     * sans "erreur". */
    /* res.signature(0) */
    if (res.error())
        throw GpgME::Exception(res.error(), "Failed to verify signature");

    return res;
}

GpgME::Key GPGCrypto::getKey(const std::string& key_id) {
    if (not ctx)
        return {};

    auto key = ctx->key(key_id.c_str(), err, false);
    if (err)
        throw GpgME::Exception(err, "Failed to retrieve key with ID "+key_id);

    return key;
}

bool GPGCrypto::isGPGencrypted(const std::vector<uint8_t>& data) const {
    GpgME::Data d {reinterpret_cast<const char*>(data.data()), data.size()};
    return d.type() == GpgME::Data::Type::PGPEncrypted;
}

} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

