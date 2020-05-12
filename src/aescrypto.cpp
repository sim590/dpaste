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

#include <opendht/crypto.h>

#include "log.h"
#include "aescrypto.h"

namespace dpaste {
namespace crypto {

std::string AES::getPassword(const std::shared_ptr<Parameters>& params) const {
    if (auto p = std::get_if<AESParameters>(params.get()))
        return p->password;
    return {};
}

std::vector<uint8_t> AES::processPlainText(std::vector<uint8_t> plain_text, std::shared_ptr<Parameters>&& params) {
    DPASTE_MSG("Encrypting (aes-gcm) data...");
    return dht::crypto::aesEncrypt(plain_text, getPassword(params));
}

std::vector<uint8_t> AES::processCipherText(std::vector<uint8_t> cipher_text, std::shared_ptr<Parameters>&& params) {
    DPASTE_MSG("Decrypting (aes-gcm)...");
    return dht::crypto::aesDecrypt(cipher_text, getPassword(params));
}

} /* crypto */
} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

