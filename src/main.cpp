/*
 * Copyright © 2016 Simon Désaulniers
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

#include <iostream>
#include <string>
#include <utility>
#include <getopt.h>

#include "node.h"

#define DPASTE_VERSION "0.0.1"

/* Command line parsing */
struct ParsedArgs {
    bool fail {false};
    bool help {false};
    bool version {false};
    std::string get_hash;
};

static const constexpr struct option long_options[] = {
   {"help",       no_argument      , nullptr, 'h'},
   {"version",    no_argument      , nullptr, 'v'},
   {"get",        required_argument, nullptr, 'g'},
   {nullptr,      0                , nullptr,  0}
};

ParsedArgs parseArgs(int argc, char *argv[]) {
    ParsedArgs pa;
    int opt;
    while ((opt = getopt_long(argc, argv, "hvg:", long_options, nullptr)) != -1) {
        switch (opt) {
        case 'h':
            pa.help = true;
            break;
        case 'v':
            pa.version = true;
            break;
        case 'g': {
            pa.get_hash = optarg;
            break;
        }
        default:
            pa.fail = true;
            return pa;
        }
    }
    return pa;
}

void print_help() {
    std::cout << "dpaste -- A simple pastebin for light values (max 64KB) using OpenDHT distributed hash table." << std::endl << std::endl;

    std::cout << "SYNOPSIS" << std::endl
              << "    dpaste [-h]" << std::endl
              << "    dpaste [-g hash]" << std::endl;

    std::cout << "OPTIONS" << std::endl;
    std::cout << "    -h|--help" << std::endl
              << "        Prints this help text." << std::endl;

    std::cout << "    -g|--get {hash}" << std::endl
              << "        Get the pasted file under the hash {hash}." << std::endl;

    std::cout << std::endl;
    std::cout << "When -g option is ommited, dpaste will read its standard input for a file to paste." << std::endl;
}

int main(int argc, char *argv[]) {
    auto parsed_args = parseArgs(argc, argv);
    if (parsed_args.fail) {
        return -1;
    } else if (parsed_args.help) {
        print_help();
        return 0;
    } else if (parsed_args.version) {
        std::cout << DPASTE_VERSION << std::endl;
        return 0;
    }

    dpaste::Node node;
    node.run();

    if (not parsed_args.get_hash.empty()) {
        /* get a pasted blob */
        auto values = node.get(parsed_args.get_hash);
        if (values.size() == 1) {
            auto& b = values.front();
            std::string s {b.begin(), b.end()};
            std::cout << s << std::endl;
        } else if (values.size() > 1) {
            std::cout << "Multiple candidates..." << std::endl;
            size_t n {0};
            for (auto& b : values) {
                std::string s {b.begin(), b.end()};

                std::cout << "Candidate #" << n++ << std::endl;
                std::istringstream iss {s};
                for (std::string line; std::getline(iss, line);) {
                    std::cout << "\t" << line << std::endl;
                }
            }
        }
    } else {
        /* paste a blob on the DHT */
        char in[dht::MAX_VALUE_SIZE];
        std::cin.read(in, dht::MAX_VALUE_SIZE);
        dht::Blob blob {in, in+std::cin.gcount()-1};
        std::cout << node.paste(std::move(blob)) << std::endl;
    }

    node.stop();
    return 0;
}

