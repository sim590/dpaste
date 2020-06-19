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

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <utility>

#include <gpgme++/context.h>
#include <gpgme++/exception.h>
#include <gpgme++/encryptionresult.h>
#include <gpgme++/decryptionresult.h>
#include <gpgme++/signingresult.h>
#include <gpgme++/verificationresult.h>

#include "cipher.h"

namespace dpaste {
namespace crypto {

class GPG : public Cipher {
public:
    GPG(std::string signer="");
    virtual ~GPG () {}

    static void init();

    std::vector<uint8_t>
        processPlainText(std::vector<uint8_t> plain_text, std::shared_ptr<Parameters>&& params) override;

    std::vector<uint8_t>
        processCipherText(std::vector<uint8_t> cipher_text, std::shared_ptr<Parameters>&& params) override;

    std::tuple<std::vector<uint8_t>,
        GpgME::EncryptionResult,
        GpgME::SigningResult>
            encrypt(const std::vector<std::string>& recipient, std::vector<uint8_t> plain_text, bool sign=false) const;

    std::tuple<std::vector<uint8_t>,
        GpgME::DecryptionResult,
        GpgME::VerificationResult>
            decryptAndVerify(const std::vector<uint8_t>& cipher) const;

    std::pair<std::vector<uint8_t>,
        GpgME::SigningResult>
            sign(const std::vector<uint8_t>& plain_text) const;

    GpgME::VerificationResult verify(const std::vector<uint8_t>& signature, const std::vector<uint8_t>& plain_text) const;

    void comment_on_signature(const GpgME::Signature& sig);

    static bool isGPGencrypted(const std::vector<uint8_t>& d);

private:
    GpgME::Key getKey(const std::string& key_id) const;

    std::unique_ptr<GpgME::Context> ctx;
    std::string signerKey_;
};


} /* crypto */
} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

