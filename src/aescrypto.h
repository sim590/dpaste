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

#include <cstdint>
#include <string>
#include <vector>

#include "cipher.h"

namespace dpaste {
namespace crypto {

class AES : public Cipher {
public:
    AES() {}
    virtual ~AES () {}

    std::vector<uint8_t>
        processPlainText(std::vector<uint8_t> plain_text, std::shared_ptr<Parameters>&& params) override;
    std::vector<uint8_t>
        processCipherText(std::vector<uint8_t> cipher_text, std::shared_ptr<Parameters>&& params) override;
private:
    std::string getPassword(const std::shared_ptr<Parameters>& params) const;
};

} /* crypto */
} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

