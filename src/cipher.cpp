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

#include "cipher.h"

#include "code.h"
#include "gpgcrypto.h"
#include "aescrypto.h"

namespace dpaste {
namespace crypto {

void Cipher::init() {
    static bool initialized = false;
    if (initialized) return;

    GPG::init();

    initialized = true;
}

std::shared_ptr<Cipher> Cipher::get(const std::vector<uint8_t>& cipher_text, const std::string& pin="") {
    if (GPG::isGPGencrypted(cipher_text))
        return std::make_shared<GPG>();
    else if (pin.size() == code::PIN_WITH_PASS_LEN)
        return std::make_shared<AES>();
    return {};
}

std::shared_ptr<Cipher> Cipher::get(Scheme scheme, std::shared_ptr<Parameters>&& params) {
    switch (scheme) {
        case Scheme::AES:
            return std::make_shared<AES>();
        case Scheme::GPG:
            if (auto gpg_params = std::get_if<GPGParameters>(params.get()))
                return std::static_pointer_cast<Cipher>(std::make_shared<GPG>(gpg_params->key_id));
            else
                return std::make_shared<GPG>();
        default:
            break;
    }
    return {};
}

} /* crypto */
} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

