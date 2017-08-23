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
#include <getopt.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bin.h"


/* Command line parsing */
struct ParsedArgs {
    bool fail {false};
    bool help {false};
    bool version {false};
    bool sign {false};
    bool no_decrypt {false};
    std::string code;
    std::string recipient;
};

static const constexpr struct option long_options[] = {
   {"help",       no_argument      , nullptr, 'h'},
   {"version",    no_argument      , nullptr, 'v'},
   {"get",        required_argument, nullptr, 'g'},
   {"encrypt",    required_argument, nullptr, 'e'},
   {"sign",       no_argument      , nullptr, 's'},
   {"no-decrypt", no_argument      , nullptr, 'x'},
   {nullptr,      0                , nullptr,  0 }
};

ParsedArgs parseArgs(int argc, char *argv[]) {
    ParsedArgs pa;
    int opt;
    while ((opt = getopt_long(argc, argv, "hvg:e:s", long_options, nullptr)) != -1) {
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
        case 'e':
            pa.recipient = std::string(optarg);
            break;
        case 's':
            pa.sign = true;
            break;
        case 'x':
            pa.no_decrypt = true;
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

    std::cout << "OPTIONS" << std::endl;
    std::cout << "    -h|--help" << std::endl
              << "        Prints this help text." << std::endl;

    std::cout << "    -v|--version" << std::endl
              << "        Prints the program's version number." << std::endl;

    std::cout << "    -g|--get {code}" << std::endl
              << "        Get the pasted file under the code {code}." << std::endl;

    std::cout << "    -e|--encrypt {recipient}" << std::endl
              << "        GPG encrypt for recipient {recipient}." << std::endl;

    std::cout << "    -s|--sign" << std::endl
              << "        Sign with configured GPG key." << std::endl;

    std::cout << "    --no-decrypt" << std::endl
              << "        Tells dpaste not to decrypt PGP data and rather output it on stdout." << std::endl;

    std::cout << std::endl;
    std::cout << "When -g option is ommited, " << PACKAGE_NAME << " will read its standard input for a file to paste."
              << std::endl;
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
    std::stringstream ss;
    if (parsed_args.code.empty())
        ss << std::cin.rdbuf();

    dpaste::Bin dpastebin {
        std::move(parsed_args.code),
        std::move(ss),
        std::move(parsed_args.recipient),
        parsed_args.sign,
        parsed_args.no_decrypt
    };
    return dpastebin.execute();
}

