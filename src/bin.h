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
    int get(std::string&& code, bool no_decrypt=false);

    /**
     * Execute procedure to publish content and generate the associated code.
     *
     * @param data            Data to be pasted.
     * @param recipient       The recipient of the pasted data.
     * @param sign            Whether to sign the data or not.
     * @param self_recipient  Whether including self in recipient list.
     *
     * @return return code (0: success, 1 fail)
     */
    int paste(std::vector<uint8_t>&& data,
            std::string&& recipient={},
            bool sign=false,
            bool self_recipient=false) const;
    int paste(std::stringstream&& input_stream,
            std::string&& recipient={},
            bool sign=false,
            bool self_recipient=false) const
    {
        return paste(data_from_stream(std::move(input_stream)),
                std::forward<std::string>(recipient),
                sign,
                self_recipient);
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

    /* crypto */
    std::unique_ptr<GPGCrypto> gpg {};
    std::string keyid_ {};

    /* transport */
    std::unique_ptr<HttpClient> http_client_ {};
    std::map<std::string, std::string> conf;
    Node node {};
};

} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

