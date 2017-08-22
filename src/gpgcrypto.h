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

namespace dpaste {

class GPGCrypto {
public:
    GPGCrypto(std::string signer);
    virtual ~GPGCrypto () {}


    std::tuple<std::vector<uint8_t>,
        GpgME::EncryptionResult,
        GpgME::SigningResult>
            encrypt(std::string recipient, std::vector<uint8_t> plain_text, bool sign=false);

    std::tuple<std::vector<uint8_t>,
        GpgME::DecryptionResult,
        GpgME::VerificationResult>
            decryptAndVerify(const std::vector<uint8_t>& cipher);

    std::pair<std::vector<uint8_t>,
        GpgME::SigningResult>
            sign(const std::vector<uint8_t>& plain_text);

    GpgME::VerificationResult verify(const std::vector<uint8_t>& signature, const std::vector<uint8_t>& plain_text);

    bool isGPGencrypted(const std::vector<uint8_t>& d) const;

private:
    GpgME::Key getKey(const std::string& key_id);
    GpgME::Error err;
    std::unique_ptr<GpgME::Context> ctx;
};

} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

