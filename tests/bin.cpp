/*
 * Copyright © 2018-2020 Simon Désaulniers
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

#include "code.h"
#include "tests.h"
#include "bin.h"

#include "catch.h"

namespace dpaste {
namespace tests {

class PirateBinTester {
    #define YARRRRR
public:

    PirateBinTester () {}
    virtual ~PirateBinTester () {}

    static std::string dpaste_uri_prefix() { return Bin::DPASTE_URI_PREFIX; }

    static std::string hexStrFromInt(uint32_t i, size_t len) {
        return Bin::hexStrFromInt(i, len);
    }

    static std::tuple<std::string, uint32_t, std::string> parse_code_info(const std::string& uri) {
        return Bin::parse_code_info(uri);
    }

    static std::string code_from_dpaste_uri(const std::string& uri) {
        return Bin::code_from_dpaste_uri(uri);
    }

    std::vector<uint8_t> data_from_stream(std::stringstream& input_stream, size_t count) const {
        return Bin::data_from_stream(input_stream, count);
    }
};

void test_paste(Bin& bin, const std::vector<uint8_t>& data) {
    using pbt = PirateBinTester;
    auto code = bin.paste(std::vector<uint8_t> {data}, {});
    CATCH_REQUIRE ( code.size() == pbt::dpaste_uri_prefix().size()+code::DPASTE_PIN_LEN+code::DPASTE_NPACKETS_LEN );

    CATCH_SECTION ( "getting pasted blob back from the DHT" ) {
        auto rd = bin.get(std::move(code)).second;
        std::vector<uint8_t> rdv {rd.begin(), rd.end()};
        CATCH_REQUIRE ( data == rdv );
    }
}

void test_paste_encrypted(Bin& bin, const std::vector<uint8_t>& data) {
    using pbt = PirateBinTester;
    auto p = std::make_unique<dpaste::crypto::Parameters>();
    p->emplace<crypto::AESParameters>();
    auto code = bin.paste(std::vector<uint8_t> {data}, std::move(p));
    CATCH_REQUIRE ( code.size() == pbt::dpaste_uri_prefix().size()+code::DPASTE_PIN_LEN+code::DPASTE_NPACKETS_LEN+code::PASSWORD_LEN );

    CATCH_SECTION ( "getting pasted AES encrypted blob back from the DHT" ) {
        auto rd = bin.get(std::move(code)).second;
        std::vector<uint8_t> rdv {rd.begin(), rd.end()};
        CATCH_REQUIRE ( data == rdv );
    }
}

CATCH_TEST_CASE("Bin get/paste on DHT", "[Bin][get][paste]") {
    std::vector<uint8_t> data = {0, 1, 2, 3, 4};
    Bin bin {};
    crypto::Cipher::init();
    CATCH_SECTION ( "pasting data {0,1,2,3,4}" ) {
        test_paste(bin, data);
    }
    CATCH_SECTION ( "pasting AES encrypted {0,1,2,3,4}" ) {
        test_paste_encrypted(bin, data);
    }
    CATCH_SECTION ( "pasting data (size over 64KB)" ) {
        std::array<size_t, 4> sizes = {32 * 1024, 64 * 1024, 512 * 1024};
        for (const auto& s : sizes) {
            std::stringstream ss;
            ss << s;
            CATCH_SECTION ( "trying for size of " + ss.str() + "B" ) {
                std::vector<uint8_t> buffer(s);
                CATCH_SECTION ( "plain data" ) {
                    test_paste(bin, buffer);
                }
                CATCH_SECTION ( "AES encrypted data" ) {
                    test_paste_encrypted(bin, buffer);
                }
            }
        }
    }
}

CATCH_TEST_CASE("Bin parsing of uri code ([dpaste:]XXXXXXXX)", "[Bin][code_from_dpaste_uri][parse_code_info]") {
    using pbt = PirateBinTester;
    const uint32_t NPACKETS     = (uint32_t)random_number() % 256;
    const std::string NPACKETSS = pbt::hexStrFromInt(NPACKETS, 2);
    const std::string LCODE     = random_code();
    const std::string CODE      = LCODE + NPACKETSS;
    const std::string PWD       = random_code();
    std::string pin_upper {CODE.begin(), CODE.end()};
    std::transform(pin_upper.begin(), pin_upper.end(), pin_upper.begin(), ::toupper);

    auto test_code_parsing = [&LCODE,&NPACKETS](const std::string& unparsed_code, const std::string& pwd) {
        const auto code_tuple1 = pbt::parse_code_info(unparsed_code);

        CATCH_REQUIRE ( std::get<0>(code_tuple1) == LCODE );
        CATCH_REQUIRE ( std::get<1>(code_tuple1) == NPACKETS );
        CATCH_REQUIRE ( std::get<2>(code_tuple1) == pwd );
    };

    CATCH_SECTION ( "good pins" ) {
        auto gc1 = "dpaste:"+CODE;
        auto gc2 = CODE;
        auto gc3 = "dpaste:"+CODE+PWD;
        CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc1) == CODE );
        CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc2) == CODE );
        CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc3) == CODE+PWD );

        CATCH_SECTION ( "good pin (upper case)" ) {
            auto gc4 = "dpaste:"+pin_upper;
            auto gc5 = pin_upper;
            auto gc6 = "dpaste:"+pin_upper+PWD;

            CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc4) == pin_upper );
            CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc5) == pin_upper );
            CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc6) == pin_upper+PWD );
        }
        CATCH_SECTION ( "testing Bin::parse_code_info" ) {
            test_code_parsing(CODE, "");
        }
        CATCH_SECTION ( "testing Bin::parse_code_info with password" ) {
            test_code_parsing(CODE+PWD, PWD);
        }
    }
    CATCH_SECTION ( "bad pins" ) {
        std::string bc1 = "DPASTE:"+CODE;
        std::string bc2 = "DPaste:"+CODE;
        CATCH_REQUIRE ( pbt::code_from_dpaste_uri(bc1) != CODE );
        CATCH_REQUIRE ( pbt::code_from_dpaste_uri(bc2) != CODE );

        CATCH_SECTION ( "bad pins (upper case)" ) {
            auto bc3 = "DPASTE:"+pin_upper;
            auto bc4 = "DPaste:"+pin_upper;

            CATCH_REQUIRE ( pbt::code_from_dpaste_uri(bc3) != pin_upper );
            CATCH_REQUIRE ( pbt::code_from_dpaste_uri(bc4) != pin_upper );
        }
    }
}

CATCH_TEST_CASE("Bin conversion to hex string from int", "[Bin][hexStrFromInt]") {
    using pbt = PirateBinTester;
    CATCH_REQUIRE ( pbt::hexStrFromInt(255, 2) == "ff" );
    CATCH_REQUIRE ( pbt::hexStrFromInt(121, 2) == "79" );
    CATCH_REQUIRE ( pbt::hexStrFromInt(0, 2) == "00" );
}


CATCH_TEST_CASE("Bin conversion of stringstream to vector", "[Bin][data_from_stream]") {
    PirateBinTester pt;

    const std::string DATA = "SOME DATA";
    std::stringstream ss(DATA);
    std::vector<uint8_t> d {DATA.begin(), DATA.end()};
    CATCH_REQUIRE ( pt.data_from_stream(ss, DATA.size()) == d );
}

} /* tests */
} /* dpaste */

/* vim: set ts=4 sw=4 tw=120 et :*/

