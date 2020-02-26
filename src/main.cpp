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

#include <memory>
#include <string>

#include <vector>

extern "C" {
#include <getopt.h>
}

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bin.h"
#include "cipher.h"

/* Command line parsing */
struct ParsedArgs {
    bool fail {false};
    bool help {false};
    bool version {false};
    bool sign {false};
    bool aes_encrypt {false};
    bool gpg_encrypt {false};
    bool no_decrypt {false};
    bool self_recipient {false};
    std::string code;
    std::vector<std::string> recipients;
};

static const constexpr struct option long_options[] = {
   {"help",           no_argument,       nullptr, 'h'},
   {"version",        no_argument,       nullptr, 'v'},
   {"get",            required_argument, nullptr, 'g'},
   {"aes-encrypt",    no_argument,       nullptr, '3'},
   {"gpg-encrypt",    no_argument,       nullptr, '4'},
   {"recipients",     required_argument, nullptr, 'r'},
   {"sign",           no_argument,       nullptr, 's'},
   {"no-decrypt",     no_argument,       nullptr, '1'},
   {"self-recipient", no_argument,       nullptr, '2'},
   {nullptr,          0,                 nullptr,  0 }
};

ParsedArgs parseArgs(int argc, char *argv[]) {
    ParsedArgs pa;
    int opt;
    while ((opt = getopt_long(argc, argv, "hvg:r:s", long_options, nullptr)) != -1) {
        switch (opt) {
        case 'h':
            pa.help = true;
            break;
        case 'v':
            pa.version = true;
            break;
        case 'g':
            pa.code = std::string(optarg);
            break;
        case '3':
            pa.aes_encrypt = true;
            break;
        case '4':
            pa.gpg_encrypt = true;
            break;
        case 'r':
            pa.recipients.emplace_back(std::string(optarg));
            break;
        case 's':
            pa.sign = true;
            break;
        case '1':
            pa.no_decrypt = true;
            break;
        case '2':
            pa.self_recipient = true;
            break;
        default:
            pa.fail = true;
            return pa;
        }
    }
    return pa;
}

void print_help() {
    std::cout << PACKAGE_NAME << " -- A simple pastebin for light values (max 64KB)"
                              << " using OpenDHT distributed hash table." << std::endl << std::endl;

    std::cout << "SYNOPSIS" << std::endl
              << "    " << PACKAGE_NAME << " [-h]" << std::endl
              << "    " << PACKAGE_NAME << " [-v]" << std::endl
              << "    " << PACKAGE_NAME << " [-g code]" << std::endl;

    std::cout << "OPTIONS"
              << std::endl;
    std::cout << "    -h|--help"
              << std::endl;
    std::cout << "        Prints this help text."
              << std::endl;

    std::cout << "    -v|--version" << std::endl
              << "        Prints the program's version number."
              << std::endl;

    std::cout << "    -g|--get {code}" << std::endl
              << "        Get the pasted file under the code {code}."
              << std::endl;

    std::cout << "    --aes-encrypt" << std::endl
              << "        Use AES scheme for encryption. Password is automatically saved in " << std::endl;
    std::cout << "        the returned code (\"dpaste:XXXXXX\")." << std::endl;

    std::cout << "    --gpg-encrypt" << std::endl
              << "        Use GPG scheme for encryption/signing." << std::endl;

    std::cout << "    -r|--recipients {recipient}" << std::endl
              << "        Specify the list of recipients to use for GPG encryption (--gpg--encrypt). " << std::endl
              << "        Use '-r' multiple times to specify a list of recipients" << std::endl;

    std::cout << "    -s|--sign" << std::endl
              << "        Tells wether message should be signed using the user's GPG key. The key has to be configured" << std::endl;
    std::cout << "        through the configuration file ($XDG_CONFIG_DIR/dpaste.conf, keyword: pgp_key_id)." << std::endl;

    std::cout << "    --no-decrypt" << std::endl
              << "        Tells dpaste not to decrypt PGP data and rather output it on stdout."
              << std::endl;

    std::cout << "    --self-recipient" << std::endl
              << "        Include self as recipient. Self refers to the key id configured for signing" << std::endl;
    std::cout << "        (see --sign description). This only takes effect if option \"-e\" is also used." << std::endl;

    std::cout << std::endl;
    std::cout << "When -g option is ommited, " << PACKAGE_NAME << " will read its standard input for a file to paste."
              << std::endl;
}

std::unique_ptr<dpaste::crypto::Parameters> params_from_args(const ParsedArgs& pa) {
    auto params = std::make_unique<dpaste::crypto::Parameters>();
    if (pa.aes_encrypt) {
        params->emplace<dpaste::crypto::AESParameters>();
    } else if (pa.gpg_encrypt) {
        params->emplace<dpaste::crypto::GPGParameters>(pa.recipients, pa.self_recipient, pa.sign);
    } else if (pa.sign) {
        params->emplace<dpaste::crypto::GPGParameters>();
        auto& p = std::get<dpaste::crypto::GPGParameters>(*params);
        p.sign = pa.sign;
    } else
        return {};
    return params;
}

int main(int argc, char *argv[]) {
    auto parsed_args = parseArgs(argc, argv);
    if (parsed_args.fail) {
        return 1;
    } else if (parsed_args.help) {
        print_help();
        return 0;
    } else if (parsed_args.version) {
        std::cout << VERSION << std::endl;
        return 0;
    }

    dpaste::Bin dpastebin {};
    dpaste::crypto::Cipher::init();
    int rc;
    if (not parsed_args.code.empty()) {
        auto r = dpastebin.get(std::move(parsed_args.code), parsed_args.no_decrypt);
        if (r.first) {
            rc = 0;
            std::cout << r.second;
        } else
            rc = 1;
    } else {
        std::stringstream ss;
        ss << std::cin.rdbuf();
        auto uri = dpastebin.paste(std::move(ss), params_from_args(parsed_args));
        std::cout << uri << std::endl;
        rc = uri.empty() ? 1 : 0;
    }

    return rc;
}

/* vim:set et sw=4 ts=4 tw=120: */

