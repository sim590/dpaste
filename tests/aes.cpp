/*
 * Copyright © 2018 Simon Désaulniers
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

#include <memory>

#include <catch2/catch.hpp>

#include "cipher.h"
#include "aescrypto.h"

namespace dpaste {
namespace tests {

TEST_CASE("AES process plain/cipher text", "[AES][processPlainText][processCipherText]") {
    crypto::AES aes {};
    std::vector<uint8_t> data {0,1,2,3,4};
    auto pwd = "this_is_a_really_good_pwd";
    auto p = std::make_shared<crypto::Parameters>();
    auto pp = p;
    p->emplace<crypto::AESParameters>(pwd);

    auto ct = aes.processPlainText(data, std::move(p));
    REQUIRE ( not ct.empty() );

    auto pt = aes.processCipherText(ct, std::move(pp));
    REQUIRE ( pt == data );
}

} /* tests */
} /* dpaste */

/* vim: set ts=4 sw=4 tw=120 et :*/

