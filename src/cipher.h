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

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <exception>
#include <variant>

namespace dpaste {
namespace crypto {

struct GPGParameters;
using Parameters = std::variant<GPGParameters>;

class Cipher {
public:
    enum class Scheme : int { NONE=0, GPG };

    virtual ~Cipher () {}

    static void init();

    /**
     * Process the plain text according to the cipher used.
     *
     * @param plain_text  The plain text.
     * @param params      The parameters needed to process the plain text by the cipher.
     *
     * @return the resulting cipher_text
     */
    virtual std::vector<uint8_t>
        processPlainText(std::vector<uint8_t> plain_text, std::shared_ptr<Parameters>&& params) = 0;

    /**
     * Process the cipher text according to the cipher used.
     *
     * @param plain_text  The cipher text.
     * @param params      The parameters needed to process the cipher text by the cipher.
     *
     * @return the resulting plain text.
     */
    virtual std::vector<uint8_t>
        processCipherText(std::vector<uint8_t> cipher_text, std::shared_ptr<Parameters>&& params) = 0;

    /**
     * Get a cipher by specifying the scheme to use (either GPG).
     *
     * @param scheme       The scheme to use (GPG).
     * @param init_params  The initialization parameters if needed.
     *
     * @return A cipher
     */
    static std::shared_ptr<Cipher> get(Scheme scheme, std::shared_ptr<Parameters>&& init_params={});

    /**
     * Returns a cipher according to the cipher text put in parameters.
     *
     * @param cipher_text  The cipher text to guess the cipher from.
     *
     * @return A cipher
     */
    static std::shared_ptr<Cipher> get(const std::vector<uint8_t>& cipher_text);
};

struct GPGParameters {
    const static Cipher::Scheme scheme = Cipher::Scheme::GPG;

    std::string key_id;
    std::vector<std::string> recipients;
    bool self_recipient;
    bool sign;

    GPGParameters() {}
    GPGParameters(std::string key_id) : key_id(key_id) {}
    GPGParameters(std::vector<std::string> recipients, bool self_recipient, bool sign)
        : recipients(recipients), self_recipient(self_recipient), sign(sign) {}
};

} /* crypto */
} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

