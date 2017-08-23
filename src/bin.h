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

#include <istream>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <map>

#include "node.h"
#include "http_client.h"
#include "gpgcrypto.h"

namespace dpaste {

class Bin {
public:
    Bin(std::string&& code, std::stringstream&& data_stream, std::string&& recipient={}, bool sign=false, bool no_decrypt=false);
    virtual ~Bin () {}

    /**
     * Execute the program main functionnality.
     *
     * @return return code (0: success, 1 fail)
     */
    int execute() {
        if (not code_.empty())
            return get();
        else
            return paste();
    }

private:
    /* constants */
    static const constexpr char* DPASTE_CODE_PREFIX = "dpaste:";
    static const constexpr uint8_t PROTO_VERSION = 0;

    struct Packet {
        std::vector<uint8_t> data {};
        std::vector<uint8_t> signature {};

        std::vector<uint8_t> serialize();
        void deserialize(const std::vector<uint8_t>& pbuffer);
    };

    /**
     * Execute procedure to get the content stored for a given code.
     *
     * @return return code (0: success, 1 fail)
     */
    int get();
    /**
     * Execute procedure to publish content and generate the associated code.
     *
     * @return return code (0: success, 1 fail)
     */
    int paste();

    /* data */
    std::string code_ {};
    std::vector<uint8_t> buffer_ {};

    /* crypto */
    std::unique_ptr<GPGCrypto> gpg;
    std::string keyid_;
    std::string recipient_ {};
    bool sign_ {false};
    bool no_decrypt_ {false};

    /* transport */
    std::unique_ptr<HttpClient> http_client_ {};
    std::map<std::string, std::string> conf;
    Node node {};
};

} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

