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

#include <istream>
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <map>
#include <utility>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
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

    /**
     * Maximal size to be read from the input stream and pasted on the DHT
     * (possibly split in several packets).
     */
    static const constexpr size_t DPASTE_MAX_SIZE {2 * 1024 * 1024};

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
    std::string paste(std::vector<uint8_t>&& data, std::unique_ptr<crypto::Parameters>&& params) {
        std::string is;
        std::move(data.begin(), data.end(), std::back_inserter(is));
        std::stringstream ss(std::move(is));
        return paste(std::move(ss), std::forward<std::unique_ptr<crypto::Parameters>>(params));
    }
    std::string paste(std::stringstream&& input_stream, std::unique_ptr<crypto::Parameters>&& params);

private:
    /* constants */
    static const constexpr char* DPASTE_URI_PREFIX = "dpaste:";
    static const constexpr uint8_t PROTO_VERSION = 0;

    struct Packet {
        /**
         * Bytes needed for the two layers of serialization. This accounts for
         * dpaste's usage of msgpack as well as OpenDHT's and GPG
         * encryption/signature.
         */
        static const constexpr size_t EXTRA_SERIALIZATION_BYTES {12 * 1024};

        std::vector<uint8_t> data {};
        std::vector<uint8_t> signature {};

        std::vector<uint8_t> serialize() const;
        void deserialize(const std::vector<uint8_t>& pbuffer);
    };

    /*!
     * @class   Random
     * @brief   Random number generator state encapsulator.
     * @details
     * The structure contains the the state of the random number generator used
     * by the Bin class to perform its random code generation. The structure
     * is embeded inside the Bin class as a private member.
     */
    struct Random {
        Random() {
            std::random_device rdev;
            std::seed_seq seed {rdev(), rdev()};
            gen.seed(seed);
        }

        inline uint32_t integer() { return dist(gen); }

        std::uniform_int_distribution<uint32_t> dist;
        std::mt19937_64 gen;
    };

    /**
     * Create a Packet from input data and crypto parameters.
     *
     * @param input_stream  input data stream of bytes.
     * @param params        cryptographic parameters (either GPGParameters or
     *                      AESParameters).
     *
     * @return an ordered pair of a Packet and associated password.
     */
    std::pair<std::vector<Bin::Packet>, std::string>
    prepare_data(std::stringstream&& input_stream, std::unique_ptr<crypto::Parameters>&& params);

    /**
     * Parses the DHT values recovered from the DHT get operation and returns
     * the first value found the the given code identifier.
     *
     * @param code        String of bytes consisting in the location code
     *                    information.
     * @param values      The values to parse.
     * @param pwd         The password used to decrypt the AES encrypted data
     *                    (if it was present in the input code).
     * @param no_decrypt  A bool indicating if the data should be decrypted
     *                    before being returned.
     *
     * @return The value (or part of the value) recovered from the DHT.
     */
    std::vector<uint8_t> parse_data(const std::string& code,
                                     std::vector<std::vector<uint8_t>>&& values,
                                     std::string pwd,
                                     bool no_decrypt) const;

    /**
     * Get data from input stream.
     *
     * @param input_stream  The stream to read to get the data.
     * @param count         Number of bytes to be read from the stream.
     *
     * @return the data
     */
    static std::vector<uint8_t> data_from_stream(std::stringstream& input_stream, const size_t& count);

    /**
     * Parse dpaste uri for code.
     *
     * @param uri  The dpaste uri
     *
     * @return hexadecimal code from the uri
     */
    static std::string code_from_dpaste_uri(const std::string& uri);

    /**
     * Parses the input code into three separate parts consisting respectivly in
     * the location code information, the number of packets the file is split
     * into and the password used to decrypt AES encrypted data.
     *
     * @param code  The input code.
     *
     * @return The tuple described above.
     */
    static std::tuple<std::string, uint32_t, std::string> parse_code_info(const std::string& code);

    /**
     * Converts a number into its hexadecimal `len` bytes long string value
     * with zeroes padding in prefix when necessary.
     *
     * @param i    The integer.
     * @param len  The length of the resulting string.
     *
     * @return the resulting hexadecimal string representation of the integer.
     */
    static std::string hexStrFromInt(uint32_t i, size_t len);

    /**
     * Converts a PIN number into a string code, i.e. an hexadecimal string
     * representation of the PIN in upper case.
     *
     * @param i  The integer.
     *
     * @return the resulting hexadecimal string representation of the integer.
     */
    static std::string code_from_pin(uint32_t i);

    /**
     * Generates a random PIN using a 32-bit random number generator. The
     * resulting string describes a 32-bit value.
     */
    inline std::string random_pin() { return code_from_pin(random_.integer()); }

    /* A map containing the configuration read from the config file on disk. */
    std::map<std::string, std::string> conf_;

    /* transport */
    std::unique_ptr<HttpClient> http_client_ {};
    Node node {};

    Random random_ {};
};

} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

