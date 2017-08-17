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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "node.h"
#include "conf.h"

#include <iostream>
#include <string>
#include <sstream>
#include <utility>
#include <getopt.h>


#include "node.h"
#include "http_client.h"

static const constexpr char* DPASTE_CODE_PREFIX = "dpaste:";

/* Command line parsing */
struct ParsedArgs {
    bool fail {false};
    bool help {false};
    bool version {false};
    std::string code;
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
            const auto cs = std::string(optarg);
            const auto p = cs.find_first_of(DPASTE_CODE_PREFIX);
            pa.code = cs.substr(p != std::string::npos ? sizeof(DPASTE_CODE_PREFIX)-1 : p);
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

    std::cout << std::endl;
    std::cout << "When -g option is ommited, " << PACKAGE_NAME << " will read its standard input for a file to paste."
              << std::endl;
}

int main(int argc, char *argv[]) {
    auto parsed_args = parseArgs(argc, argv);
    if (parsed_args.fail) {
        return -1;
    } else if (parsed_args.help) {
        print_help();
        return 0;
    } else if (parsed_args.version) {
        std::cout << VERSION << std::endl;
        return 0;
    }
    auto config_file = dpaste::conf::ConfigurationFile();
    config_file.load();
    const auto conf = config_file.getConfiguration();

    long port;
    {
        std::istringstream conv(conf.at("port"));
        conv >> port;
    }
    dpaste::HttpClient cc {conf.at("host"), port};
    auto success {false};
    if (not parsed_args.code.empty()) {
        /* first try http server */
        auto data = cc.get(parsed_args.code);

        /* if fail, then perform request from local node */
        if (data.empty()) {
            dpaste::Node node;
            node.run();

            /* get a pasted blob */
            auto values = node.get(parsed_args.code);
            if (not values.empty()) {
                auto& b = values.front();
                data = std::string(b.begin(), b.end());
            }
            node.stop();
        }
        if (not data.empty()) {
            std::cout << data << std::endl;
        }
    } else {
        /* paste a blob on the DHT */
        char in[dht::MAX_VALUE_SIZE];
        std::cin.read(in, dht::MAX_VALUE_SIZE);
        std::string in_str(in, in+std::cin.gcount()-1);

        std::uniform_int_distribution<uint32_t> codeDist_;
        std::mt19937_64 rand_;
        dht::crypto::random_device rdev;
        std::seed_seq seed {rdev(), rdev()};
        rand_.seed(seed);

        auto code_n = codeDist_(rand_);
        std::stringstream ss;
        ss << std::hex << code_n;
        auto code = ss.str();
        std::transform(code.begin(), code.end(), code.begin(), ::toupper);

        success = cc.put(code, in_str);
        if (not success) {
            dpaste::Node node;
            node.run();

            dht::Blob blob {in_str.begin(), in_str.end()};
            success = node.paste(code, std::move(blob));
            node.stop();
        }
        if (success)
            std::cout << DPASTE_CODE_PREFIX << code << std::endl;
    }

    return 0;
}

