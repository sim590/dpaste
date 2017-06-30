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
#include <fstream>
#include <string>
#include <sstream>
#include <utility>
#include <getopt.h>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <json.hpp>
#include <b64/decode.h>

#include "node.h"

using json = nlohmann::json;

static const constexpr char* DPASTE_CODE_PREFIX = "dpaste:";
static const constexpr char* HTTP_PROTO = "http://";
static std::ofstream null("/dev/null");

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

    try {
        bool curl_success = false;
        curlpp::Cleanup mycleanup;
        curlpp::Easy req;
        long port;
        {
            std::istringstream conv(conf.at("port"));
            conv >> port;
        }
        req.setOpt<curlpp::options::Port>(port);
        if (not parsed_args.code.empty()) {
            std::stringstream response, oss;
            req.setOpt<curlpp::options::Url>(HTTP_PROTO+
                    conf.at("host")+"/"+dht::InfoHash::get(parsed_args.code).toString()
                    +"?user_type="+dpaste::Node::DPASTE_USER_TYPE
            );
            req.setOpt(curlpp::Options::WriteStream(&response));

            try {
                req.perform();
                /* server gives code 200 when everything is fine. */
                curl_success = curlpp::Infos::ResponseCode::get(req) == 200 ? true : false;
            } catch (curlpp::RuntimeError & e) { }

            if (curl_success) {
                auto pr = json::parse(response.str());
                if (not pr.empty()) {
                    std::istringstream iss((*pr.begin())["base64"].dump());
                    base64::decoder d;
                    d.decode(iss, oss);
                }
            } else {
                dpaste::Node node;
                node.run();

                /* get a pasted blob */
                auto values = node.get(parsed_args.code);
                if (not values.empty()) {
                    auto& b = values.front();
                    oss << std::string(b.begin(), b.end());
                }
                node.stop();
            }
            if (not oss.str().empty()) {
                std::cout << oss.str() << std::endl;
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

            auto pin = codeDist_(rand_);
            std::stringstream ss;
            ss << std::hex << pin;
            auto pin_str = ss.str();
            std::transform(pin_str.begin(), pin_str.end(), pin_str.begin(), ::toupper);

            req.setOpt<curlpp::options::Url>(HTTP_PROTO+conf.at("host")+"/"+dht::InfoHash::get(pin_str).toString());
            req.setOpt(curlpp::Options::WriteStream(&null));
            {
                curlpp::Forms form_parts;
                form_parts.push_back(new curlpp::FormParts::Content("user_type", dpaste::Node::DPASTE_USER_TYPE));
                form_parts.push_back(new curlpp::FormParts::Content("data", in_str));
                req.setOpt(new curlpp::options::HttpPost(form_parts));
            }

            try {
                req.perform();
                curl_success = curlpp::Infos::ResponseCode::get(req) == 200 ? true : false;
            } catch (curlpp::RuntimeError & e) { }

            if (not curl_success) {
                dpaste::Node node;
                node.run();

                dht::Blob blob {in_str.begin(), in_str.end()};
                node.paste(pin_str, std::move(blob));
                node.stop();
            }
            std::cout << DPASTE_CODE_PREFIX << pin_str << std::endl;
        }
    } catch (curlpp::LogicError & e) { }

    return 0;
}

