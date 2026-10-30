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

#include <msgpack.hpp>

#include "code.h"
#include "tests.h"
#include "bin.h"

/* Included after the project headers: OpenDHT's log_enable.h defines a `WARN`
 * member that collides with Catch2's WARN() macro. */
#include <catch2/catch_test_macros.hpp>

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

    static std::optional<code::Info> parse_code_info(const std::string& uri) {
        return Bin::parse_code_info(uri);
    }

    static std::string code_from_dpaste_uri(const std::string& uri) {
        return Bin::code_from_dpaste_uri(uri);
    }

    std::vector<uint8_t> data_from_stream(std::stringstream& input_stream, size_t count) const {
        return Bin::data_from_stream(input_stream, count);
    }

    static std::pair<std::vector<Bin::Packet>, std::string>
    prepare_data(Bin& bin, std::stringstream&& input_stream, std::unique_ptr<crypto::Parameters>&& params) {
        return bin.prepare_data(std::move(input_stream), std::move(params));
    }

    static std::optional<Bin::ParsedPacket>
    parse_data(Bin& bin, std::vector<uint8_t>&& value, std::string pwd, bool no_decrypt) {
        return bin.parse_data(std::move(value), std::move(pwd), no_decrypt);
    }
};

void test_paste(Bin& bin, const std::vector<uint8_t>& data, bool no_proxy) {
    using pbt = PirateBinTester;
    auto code = bin.paste(std::vector<uint8_t> {data}, {}, no_proxy);
    CATCH_REQUIRE ( code.size() == pbt::dpaste_uri_prefix().size()+code::DPASTE_PIN_LEN+code::DPASTE_NPACKETS_LEN+code::DPASTE_VPK_LEN );

    CATCH_SECTION ( "getting pasted blob back from the DHT" ) {
        auto rd = bin.get(std::move(code), false, no_proxy).second;
        std::vector<uint8_t> rdv {rd.begin(), rd.end()};
        CATCH_REQUIRE ( data == rdv );
    }
}

void test_paste_encrypted(Bin& bin, const std::vector<uint8_t>& data, bool no_proxy) {
    using pbt = PirateBinTester;
    auto p = std::make_unique<dpaste::crypto::Parameters>();
    p->emplace<crypto::AESParameters>();
    auto code = bin.paste(std::vector<uint8_t> {data}, std::move(p), no_proxy);
    CATCH_REQUIRE ( code.size() == pbt::dpaste_uri_prefix().size()+code::DPASTE_PIN_LEN+code::DPASTE_NPACKETS_LEN+code::DPASTE_VPK_LEN+code::PASSWORD_LEN );

    CATCH_SECTION ( "getting pasted AES encrypted blob back from the DHT" ) {
        auto rd = bin.get(std::move(code), false, no_proxy).second;
        std::vector<uint8_t> rdv {rd.begin(), rd.end()};
        CATCH_REQUIRE ( data == rdv );
    }
}

CATCH_TEST_CASE("Bin get/paste on DHT", "[Bin][get][paste]") {
    std::vector<uint8_t> data = {0, 1, 2, 3, 4};
    Bin bin {};
    crypto::Cipher::init();
    CATCH_SECTION ( "pasting data {0,1,2,3,4}" ) {
        test_paste(bin, data, true);
    }
    CATCH_SECTION ( "pasting AES encrypted {0,1,2,3,4}" ) {
        test_paste_encrypted(bin, data, true);
    }
    CATCH_SECTION ( "pasting data (size over 64KB)" ) {
        std::array<size_t, 3> sizes = {32 * 1024, 64 * 1024, 512 * 1024};
        for (const auto& s : sizes) {
            std::stringstream ss;
            ss << s;
            CATCH_SECTION ( "trying for size of " + ss.str() + "B" ) {
                std::vector<uint8_t> buffer(s);
                CATCH_SECTION ( "plain data" ) {
                    test_paste(bin, buffer, true);
                }
                CATCH_SECTION ( "AES encrypted data" ) {
                    test_paste_encrypted(bin, buffer, true);
                }
            }
        }
    }

    CATCH_SECTION ( "pasting empty input" ) {
        std::stringstream empty;
        CATCH_REQUIRE ( bin.paste(std::move(empty), {}, true).empty() );
    }

    CATCH_SECTION ( "preparing data over the maximum paste size" ) {
        std::stringstream ss(std::string(Bin::DPASTE_MAX_SIZE + 4096, 'x'));
        auto pp = PirateBinTester::prepare_data(bin, std::move(ss), {});
        size_t total = 0;
        for (const auto& p : pp.first)
            total += p.data.size();
        CATCH_REQUIRE ( total == Bin::DPASTE_MAX_SIZE );
    }
}

CATCH_TEST_CASE("Bin parsing of uri code ([dpaste:]XXXXXXXX)", "[Bin][code_from_dpaste_uri][parse_code_info]") {
    using pbt = PirateBinTester;
    const uint32_t NPACKETS     = (uint32_t)random_number() % 255 + 1;
    const std::string NPACKETSS = pbt::hexStrFromInt(NPACKETS, 2);
    const std::string LCODE     = random_code();
    const std::string CODE      = LCODE + NPACKETSS;
    const std::string VPKSS     = "10";
    const std::string CODE_VPK  = CODE + VPKSS;
    const std::string PWD       = random_code();
    std::string pin_upper {CODE.begin(), CODE.end()};
    std::transform(pin_upper.begin(), pin_upper.end(), pin_upper.begin(), ::toupper);

    auto test_code_parsing = [&LCODE,&NPACKETS](const std::string& unparsed_code, uint8_t vpk, const std::string& pwd) {
        const auto code_info = pbt::parse_code_info(unparsed_code);
        CATCH_REQUIRE ( code_info.has_value() );

        CATCH_REQUIRE ( code_info->lcode    == LCODE    );
        CATCH_REQUIRE ( code_info->npackets == NPACKETS );
        CATCH_REQUIRE ( code_info->vpk      == vpk      );
        CATCH_REQUIRE ( code_info->pwd      == pwd      );
    };

    CATCH_SECTION ( "good pins" ) {
        auto gc1 = "dpaste:"+CODE;
        auto gc2 = CODE;
        auto gc3 = "dpaste:"+CODE+PWD;
        auto gc4 = "dpaste:"+CODE_VPK;
        auto gc5 = "dpaste:"+CODE_VPK+PWD;
        CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc1) == CODE );
        CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc2) == CODE );
        CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc3) == CODE+PWD );
        CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc4) == CODE_VPK );
        CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc5) == CODE_VPK+PWD );

        CATCH_SECTION ( "good pin (upper case)" ) {
            auto gc6 = "dpaste:"+pin_upper;
            auto gc7 = pin_upper;
            auto gc8 = "dpaste:"+pin_upper+PWD;

            CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc6) == pin_upper );
            CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc7) == pin_upper );
            CATCH_REQUIRE ( pbt::code_from_dpaste_uri(gc8) == pin_upper+PWD );
        }
        CATCH_SECTION ( "trailing garbage is trimmed" ) {
            CATCH_REQUIRE ( pbt::code_from_dpaste_uri("dpaste:"+CODE_VPK+PWD+"JUNK") == CODE_VPK+PWD );
        }
        CATCH_SECTION ( "testing Bin::parse_code_info" ) {
            test_code_parsing(CODE, 1, "");
            test_code_parsing(CODE+PWD, 1, PWD);
        }
        CATCH_SECTION ( "testing Bin::parse_code_info with values per key" ) {
            test_code_parsing(CODE_VPK, 16, "");
            test_code_parsing(CODE_VPK+PWD, 16, PWD);
            test_code_parsing(CODE+"20", 32, ""); /* two characters allow more than 15 */
        }
        CATCH_SECTION ( "testing legacy codes (releases up to 0.5.0)" ) {
            const auto legacy = pbt::parse_code_info(LCODE);
            CATCH_REQUIRE ( legacy.has_value() );
            CATCH_REQUIRE ( legacy->lcode    == LCODE );
            CATCH_REQUIRE ( legacy->npackets == 1 );
            CATCH_REQUIRE ( legacy->vpk      == 1 );
            CATCH_REQUIRE ( legacy->pwd      == "" );
            CATCH_REQUIRE ( legacy->format   == code::Format::LEGACY_PIN );

            const auto legacy_pwd = pbt::parse_code_info(LCODE + PWD);
            CATCH_REQUIRE ( legacy_pwd.has_value() );
            CATCH_REQUIRE ( legacy_pwd->lcode    == LCODE );
            CATCH_REQUIRE ( legacy_pwd->npackets == 1 );
            CATCH_REQUIRE ( legacy_pwd->vpk      == 1 );
            CATCH_REQUIRE ( legacy_pwd->pwd      == PWD );
            CATCH_REQUIRE ( legacy_pwd->format   == code::Format::LEGACY_PIN_PWD );
        }
        CATCH_SECTION ( "testing invalid codes" ) {
            CATCH_REQUIRE_FALSE ( pbt::parse_code_info("").has_value() );
            CATCH_REQUIRE_FALSE ( pbt::parse_code_info(LCODE+"00").has_value() );
            CATCH_REQUIRE_FALSE ( pbt::parse_code_info(CODE+"00").has_value() );
            CATCH_REQUIRE_FALSE ( pbt::parse_code_info(CODE+"zz").has_value() );
            CATCH_REQUIRE_FALSE ( pbt::parse_code_info(CODE+"0").has_value() );
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

CATCH_TEST_CASE("Bin parsing of legacy raw values", "[Bin][parse_data]") {
    Bin bin {};
    CATCH_SECTION ( "raw value which is not a MessagePack map" ) {
        std::vector<uint8_t> raw = {'r', 'a', 'w'};
        auto parsed = PirateBinTester::parse_data(bin, std::move(raw), "", false);
        CATCH_REQUIRE ( parsed.has_value() );
        CATCH_REQUIRE_FALSE ( parsed->index.has_value() );
        CATCH_REQUIRE ( parsed->data == std::vector<uint8_t>({'r', 'a', 'w'}) );
    }
    CATCH_SECTION ( "raw value starting like an incomplete MessagePack header" ) {
        /* A lone 0xd9 byte announces a MessagePack string whose length byte
         * is missing. Releases up to 0.3.3 stored raw values, so it must be
         * returned unchanged instead of being dropped. */
        const std::vector<std::vector<uint8_t>> raw_values = {
            {0xd9},
            {0x84},
            {0xc5, 0x00},
        };
        for (const auto& raw : raw_values) {
            auto parsed = PirateBinTester::parse_data(bin, std::vector<uint8_t>(raw), "", false);
            CATCH_REQUIRE ( parsed.has_value() );
            CATCH_REQUIRE_FALSE ( parsed->index.has_value() );
            CATCH_REQUIRE ( parsed->data == raw );
        }
    }
    CATCH_SECTION ( "MessagePack map without a data scheme" ) {
        /* Releases up to 0.5.0 stored a MessagePack map with the data and an
         * optional signature, but no scheme nor index. */
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> pk(&buffer);
        pk.pack_map(2);
        pk.pack("data");      pk.pack(std::vector<uint8_t> {'o', 'l', 'd'});
        pk.pack("signature"); pk.pack(std::vector<uint8_t> {});
        std::vector<uint8_t> value {buffer.data(), buffer.data()+buffer.size()};

        auto parsed = PirateBinTester::parse_data(bin, std::move(value), "", false);
        CATCH_REQUIRE ( parsed.has_value() );
        CATCH_REQUIRE_FALSE ( parsed->index.has_value() );
        CATCH_REQUIRE ( parsed->data == std::vector<uint8_t>({'o', 'l', 'd'}) );
    }
}

} /* tests */
} /* dpaste */

/* vim: set ts=4 sw=4 tw=120 et :*/

