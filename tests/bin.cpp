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
#include <cstdlib>
#include <fstream>
#include <stdexcept>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

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

class DirectDhtConfig final {
public:
    DirectDhtConfig() {
        const auto previous = std::getenv("XDG_CONFIG_HOME");
        if (previous) {
            previous_config_home_ = previous;
            had_previous_config_home_ = true;
        }

        char directory[] = "/tmp/dpaste-bin-test-XXXXXX";
        if (not mkdtemp(directory))
            throw std::runtime_error("could not create temporary configuration directory");
        config_home_ = directory;

        try {
            std::ofstream config(config_home_ + "/dpaste.conf");
            if (not config)
                throw std::runtime_error("could not create temporary configuration file");
            config << "host = 127.0.0.1\nport = " << unavailable_port() << '\n';
            config.close();
            if (not config)
                throw std::runtime_error("could not write temporary configuration file");
            if (setenv("XDG_CONFIG_HOME", config_home_.c_str(), 1) != 0)
                throw std::runtime_error("could not set XDG_CONFIG_HOME");
        } catch (...) {
            cleanup();
            throw;
        }
    }

    ~DirectDhtConfig() { cleanup(); }

    DirectDhtConfig(const DirectDhtConfig&) = delete;
    DirectDhtConfig& operator=(const DirectDhtConfig&) = delete;

private:
    static uint16_t unavailable_port() {
        const auto socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0)
            throw std::runtime_error("could not create loopback socket");

        sockaddr_in address {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(0);
        if (bind(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            close(socket_fd);
            throw std::runtime_error("could not bind loopback socket");
        }

        socklen_t address_length = sizeof(address);
        if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &address_length) != 0) {
            close(socket_fd);
            throw std::runtime_error("could not inspect loopback socket");
        }
        const auto port = ntohs(address.sin_port);
        close(socket_fd);
        return port;
    }

    void cleanup() noexcept {
        if (config_home_.empty())
            return;
        if (had_previous_config_home_)
            setenv("XDG_CONFIG_HOME", previous_config_home_.c_str(), 1);
        else
            unsetenv("XDG_CONFIG_HOME");
        unlink((config_home_ + "/dpaste.conf").c_str());
        rmdir(config_home_.c_str());
        config_home_.clear();
    }

    std::string config_home_;
    std::string previous_config_home_;
    bool had_previous_config_home_ = false;
};

TEST_CASE("Bin get/paste on DHT", "[Bin][get][paste]") {
    using pbt = PirateBinTester;
    // Keep a user's configured proxy from changing this direct-DHT test.
    DirectDhtConfig direct_dht_config;
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
