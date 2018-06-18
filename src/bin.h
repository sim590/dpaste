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
#include <utility>

#include "config.h"
#include "node.h"
#include "http_client.h"
#include "cipher.h"

namespace dpaste {
#ifdef DPASTE_TEST
namespace tests { class PirateBinTester; } /* tests */
#endif

class Bin {
#ifdef DPASTE_TEST
    friend class tests::PirateBinTester;
#endif
public:

    static const constexpr unsigned int DPASTE_PIN_LEN {8};

    Bin();
    virtual ~Bin () {}

    /**
     * Execute procedure to get the content stored for a given code.
     *
     * @param code        The PIN for finding data in DHT.
     * @param no_decrypt  Whether to decrypt the recovered data or not.
     *
     * @return return code (0: success, 1 fail)
     */
    std::pair<bool, std::string> get(std::string&& code, bool no_decrypt=false);

    /**
     * Execute procedure to publish content and generate the associated code.
     *
     * @param data    Data to be pasted.
     * @param params  Cryptographic parameters.
     *
     * @return the code (key) to the pasted data. If empty, then process failed.
     */
    std::string paste(std::vector<uint8_t>&& data, std::unique_ptr<crypto::Parameters>&& params);
    std::string paste(std::stringstream&& input_stream, std::unique_ptr<crypto::Parameters>&& params) {
        return paste(data_from_stream(std::move(input_stream)),
                std::forward<std::unique_ptr<crypto::Parameters>>(params));
    }

private:
    /* constants */
    static const constexpr char* DPASTE_URI_PREFIX = "dpaste:";
    static const constexpr uint8_t PROTO_VERSION = 0;

    struct Packet {
        std::vector<uint8_t> data {};
        std::vector<uint8_t> signature {};

        std::vector<uint8_t> serialize() const;
        void deserialize(const std::vector<uint8_t>& pbuffer);
    };

    /**
     * Get data from input stream.
     *
     * @param input_stream  The stream to read to get the data.
     *
     * @return the data
     */
    static std::vector<uint8_t> data_from_stream(std::stringstream&& input_stream);

    /**
     * Parse dpaste uri for code.
     *
     * @param uri  The dpaste uri
     *
     * @return hexadecimal code from the uri
     */
    static std::string code_from_dpaste_uri(const std::string& uri);

    static std::string random_pin();

    std::map<std::string, std::string> conf_;

    /* transport */
    std::unique_ptr<HttpClient> http_client_ {};
    Node node {};
};

} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

