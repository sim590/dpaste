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

#include <algorithm>

#include <catch2/catch.hpp>

#include "tests.h"
#include "bin.h"

namespace dpaste {
namespace tests {

class PirateBinTester {
    #define YARRRRR
public:
    static const constexpr unsigned int LOCATION_CODE_LEN = 8;
    static const constexpr char* DPASTE_URI_PREFIX = "dpaste:";

    PirateBinTester () {}
    virtual ~PirateBinTester () {}

    std::string code_from_dpaste_uri(const std::string& uri) const {
        return Bin::code_from_dpaste_uri(uri);
    }

    std::vector<uint8_t> data_from_stream(std::stringstream&& input_stream) const {
        return Bin::data_from_stream(std::forward<std::stringstream>(input_stream));
    }
};

TEST_CASE("Bin get/paste on DHT", "[Bin][get][paste]") {
    using pbt = PirateBinTester;
    std::vector<uint8_t> data = {0, 1, 2, 3, 4};
    Bin bin {};
    crypto::Cipher::init();
    SECTION ( "pasting data {0,1,2,3,4}" ) {
        auto code = bin.paste(std::vector<uint8_t> {data}, {});
        REQUIRE ( code.size() == pbt::LOCATION_CODE_LEN+sizeof(pbt::DPASTE_URI_PREFIX)-1 );

        SECTION ( "getting pasted blob back from the DHT" ) {
            auto rd = bin.get(std::move(code)).second;
            std::vector<uint8_t> rdv {rd.begin(), rd.end()};
            REQUIRE ( data == rdv );
        }
    }
    SECTION ( "pasting AES encrypted {0,1,2,3,4}" ) {
        auto p = std::make_unique<dpaste::crypto::Parameters>();
        p->emplace<crypto::AESParameters>();
        auto code = bin.paste(std::vector<uint8_t> {data}, std::move(p));
        REQUIRE ( code.size() == 2*pbt::LOCATION_CODE_LEN+sizeof(pbt::DPASTE_URI_PREFIX)-1 );

        SECTION ( "getting pasted AES encrypted blob back from the DHT" ) {
            auto rd = bin.get(std::move(code)).second;
            std::vector<uint8_t> rdv {rd.begin(), rd.end()};
            REQUIRE ( data == rdv );
        }
    }
}

TEST_CASE("Bin parsing of uri code ([dpaste:]XXXXXXXX)", "[Bin][code_from_dpaste_uri]") {
    PirateBinTester pt;
    const std::string PIN = random_pin();
    std::string pin_upper {PIN.begin(), PIN.end()};
    std::transform(pin_upper.begin(), pin_upper.end(), pin_upper.begin(), ::toupper);

    SECTION ( "good pins" ) {
        auto gc1 = "dpaste:"+PIN;
        auto gc2 = PIN;
        REQUIRE ( pt.code_from_dpaste_uri(gc1) == PIN );
        REQUIRE ( pt.code_from_dpaste_uri(gc2) == PIN );

        SECTION ( "good pin (upper case)" ) {
            auto gc3 = "dpaste:"+pin_upper;
            auto gc4 = pin_upper;

            REQUIRE ( pt.code_from_dpaste_uri(gc3) == pin_upper );
            REQUIRE ( pt.code_from_dpaste_uri(gc4) == pin_upper );
        }
    }
    SECTION ( "bad pins" ) {
        std::string bc1 = "DPASTE:"+PIN;
        std::string bc2 = "DPaste:"+PIN;
        REQUIRE ( pt.code_from_dpaste_uri(bc1) != PIN );
        REQUIRE ( pt.code_from_dpaste_uri(bc2) != PIN );

        SECTION ( "bad pins (upper case)" ) {
            auto bc3 = "DPASTE:"+pin_upper;
            auto bc4 = "DPaste:"+pin_upper;

            REQUIRE ( pt.code_from_dpaste_uri(bc3) != pin_upper );
            REQUIRE ( pt.code_from_dpaste_uri(bc4) != pin_upper );
        }
    }
}

TEST_CASE("Bin conversion of stringstream to vector", "[Bin][data_from_stream]") {
    PirateBinTester pt;

    const std::string DATA = "SOME DATA";
    std::stringstream ss(DATA);
    std::vector<uint8_t> d {DATA.begin(), DATA.end()};
    REQUIRE ( pt.data_from_stream(std::move(ss)) == d );
}

} /* tests */
} /* dpaste */

/* vim: set ts=4 sw=4 tw=120 et :*/

