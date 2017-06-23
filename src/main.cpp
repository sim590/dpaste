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

#include <json.hpp>
#include <cpr/cpr.h>
#include <b64/decode.h>

#include "node.h"

using json = nlohmann::json;

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
            pa.code = optarg;
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
              << "    dpaste [-v]" << std::endl
              << "    dpaste [-g code]" << std::endl;

    std::cout << "OPTIONS" << std::endl;
    std::cout << "    -h|--help" << std::endl
              << "        Prints this help text." << std::endl;

    std::cout << "    -v|--version" << std::endl
              << "        Prints the program's version number." << std::endl;

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
        std::cout << VERSION << std::endl;
        return 0;
    }
    auto config_file = dpaste::conf::ConfigurationFile();
    config_file.load();
    const auto conf = config_file.getConfiguration();

    dpaste::Node node;
    node.run();

    if (not parsed_args.code.empty()) {
        auto r = cpr::Get(cpr::Url{conf.at("host")+":"+conf.at("port")+"/"+dht::InfoHash::get(parsed_args.code).toString()},
                cpr::Parameters{{"user_type", dpaste::Node::DPASTE_USER_TYPE}});
        /* server gives code 200 when everything is fine. */
        std::ostringstream oss;
        if (r.status_code == 200) {
            std::istringstream iss((*json::parse(r.text).begin())["base64"].dump());
            base64::decoder d;
            d.decode(iss, oss);
        } else {
            /* get a pasted blob */
            auto values = node.get(parsed_args.code);
            auto& b = values.front();
            oss << std::string(b.begin(), b.end());
        }
        std::cout << oss.str() << std::endl;
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

        auto pin = codeDist_(rand_);
        std::stringstream ss;
        ss << std::hex << pin;
        auto pin_str = ss.str();
        std::transform(pin_str.begin(), pin_str.end(), pin_str.begin(), ::toupper);

        auto r = cpr::Post(
                cpr::Url{conf.at("host")+":"+conf.at("port")+"/"+dht::InfoHash::get(pin_str).toString()},
                cpr::Payload{{"data", in_str}, {"user_type", dpaste::Node::DPASTE_USER_TYPE}});
        if (r.status_code != 200) {
            dht::Blob blob {in_str.begin(), in_str.end()};
            node.paste(pin_str, std::move(blob));
        }
        std::cout << pin_str << std::endl;
    }

    node.stop();
    return 0;
}

