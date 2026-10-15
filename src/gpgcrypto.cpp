/*
 * Copyright © 2017-2020 Simon Désaulniers
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

#include "log.h"
#include "gpgcrypto.h"

static constexpr const size_t BUFLEN = 1024;

namespace dpaste {
namespace crypto {

std::vector<uint8_t> dataToVector(GpgME::Data& d) {
    d.seek(0, SEEK_SET);
    std::array<uint8_t, BUFLEN> buf;
    std::vector<uint8_t> v;

    ssize_t bytes = 0;
    while ((bytes = d.read(buf.data(), BUFLEN)) > 0)
        v.insert(v.end(), buf.begin(), buf.begin()+bytes);
    return v;
}

GPG::GPG(std::string signer) : ctx(GpgME::Context::createForProtocol(GpgME::Protocol::OpenPGP)), signerKey_(signer) {
    ctx = std::unique_ptr<GpgME::Context>(GpgME::Context::createForProtocol(GpgME::Protocol::OpenPGP));
    ctx->setArmor(1);
    if (not signer.empty())
        ctx->addSigningKey(getKey(signer));
}

void GPG::init() {
    static bool initialized = false;
    if (initialized) return;

    GpgME::initializeLibrary();
    auto err = GpgME::checkEngine(GpgME::Protocol::OpenPGP);
    if (err.code() != GPG_ERR_NO_ERROR)
        throw GpgME::Exception(err, "Failed to initialize OpenPGP engine");

    initialized = true;
}

std::vector<uint8_t> GPG::processPlainText(std::vector<uint8_t> plain_text, std::shared_ptr<Parameters>&& params)
{
    if (not params)
        return {};

    auto gparams = std::get<GPGParameters>(*params);
    /* we include self as recipient if there's at least one other recipient */
    if (not gparams.recipients.empty() and gparams.self_recipient and not signerKey_.empty())
        gparams.recipients.emplace_back(signerKey_);

    auto to_sign = gparams.sign and not signerKey_.empty();
    if (not gparams.recipients.empty()) {
        DPASTE_MSG("Encrypting (gpg)%s...", to_sign ? " and signing " : "");
        auto res = encrypt(gparams.recipients, plain_text, to_sign);
        return std::get<0>(res);
    }

    return {};
}

std::vector<uint8_t>
GPG::processCipherText(std::vector<uint8_t> cipher_text, std::shared_ptr<Parameters>&& params)
{
    auto gparams = params ? std::get<GPGParameters>(*params) : GPGParameters {};

    DPASTE_MSG("Decrypting (gpg)...");
    auto res = decryptAndVerify(cipher_text);
    DPASTE_MSG("Success!");

    auto data = std::move(std::get<0>(res));
    auto& verif_res = std::get<2>(res);
    if (verif_res.numSignatures() > 0)
        comment_on_signature(verif_res.signature(0));
    return data;
}

std::tuple<std::vector<uint8_t>,
    GpgME::EncryptionResult,
    GpgME::SigningResult>
GPG::encrypt(const std::vector<std::string>& recipients, std::vector<uint8_t> plain_text, bool sign) const {
    if (not ctx or (sign and ctx->signingKeys().empty()))
        return {};

    /* Adding final null char delimiter to data */
    plain_text.push_back('\0');
    GpgME::Data pt {reinterpret_cast<const char*>(plain_text.data()), plain_text.size()};
    GpgME::Data cipher_text {};

    std::vector<GpgME::Key> keys;
    for (const auto& r : recipients)
        keys.emplace_back(getKey(r));
    GpgME::EncryptionResult enc_res;
    GpgME::SigningResult sign_res;
    if (sign) {
        auto res = ctx->signAndEncrypt(keys, pt, cipher_text, GpgME::Context::EncryptionFlags::None);
        res.first.swap(sign_res);
        res.second.swap(enc_res);
    } else {
        auto res = ctx->encrypt(keys, pt, cipher_text, GpgME::Context::EncryptionFlags::None);
        res.swap(enc_res);
    }
    /* Adding final null char delimiter to cipher_text */
    cipher_text.write("\0", 1);

    if (enc_res.error())
        throw GpgME::Exception(enc_res.error(), "Failed to encrypt with key of ID "+recipients.front());

    if (not sign_res.isNull() and sign_res.error())
        throw GpgME::Exception(
                sign_res.error(),
                "Failed to sign with key of ID "+std::string{ctx->signingKey(0).primaryFingerprint()});

    return std::make_tuple(dataToVector(cipher_text), std::move(enc_res), std::move(sign_res));
}

std::tuple<std::vector<uint8_t>,
    GpgME::DecryptionResult,
    GpgME::VerificationResult>
GPG::decryptAndVerify(const std::vector<uint8_t>& cipher_text) const {
    if (not ctx)
        return {};

    GpgME::Data ct {reinterpret_cast<const char*>(cipher_text.data()), cipher_text.size()};
    GpgME::Data pt {};
    auto res = ctx->decryptAndVerify(ct, pt);
    auto& dec_res = res.first;
    auto& sign_res = res.second;

    if (dec_res.error())
        throw GpgME::Exception(dec_res.error());
    if (sign_res.error())
        throw GpgME::Exception(sign_res.error());

    return std::make_tuple(dataToVector(pt), std::move(res.first), std::move(res.second));
}

std::pair<std::vector<uint8_t>,
    GpgME::SigningResult>
GPG::sign(const std::vector<uint8_t>& plain_text) const {
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
GPG::verify(const std::vector<uint8_t>& signature, const std::vector<uint8_t>& plain_text) const {
    if (not ctx)
        return {};

    GpgME::Data pt {reinterpret_cast<const char*>(plain_text.data()), plain_text.size()};
    GpgME::Data sig {reinterpret_cast<const char*>(signature.data()), signature.size()};
    auto res = ctx->verifyOpaqueSignature(sig, pt);

    if (res.error())
        throw GpgME::Exception(res.error());

    return res;
}

void GPG::comment_on_signature(const GpgME::Signature& sig) {
    const auto& s = sig.summary();
    if (s & GpgME::Signature::Valid)
        DPASTE_MSG("Valid signature from key with ID %s", sig.fingerprint());
}

GpgME::Key GPG::getKey(const std::string& key_id) const {
    if (not ctx)
        return {};

    GpgME::Error err;
    auto key = ctx->key(key_id.c_str(), err, false);
    if (err)
        throw GpgME::Exception(err, "Failed to retrieve key with ID "+key_id);

    return key;
}

bool GPG::isGPGencrypted(const std::vector<uint8_t>& data) {
    GpgME::Data d {reinterpret_cast<const char*>(data.data()), data.size()};
    return d.type() == GpgME::Data::Type::PGPEncrypted;
}

} /* crypto */
} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

