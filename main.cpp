/*
 * dpaste -- a simple pastebin over the OpenDHT distributed hash table.
 *
 * Copyright © 2016 Simon Désaulniers
 * Author: Simon Désaulniers <sim.desaulniers@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <opendht.h>
#include <iostream>
#include <string>
#include <random>
#include <memory>
#include <future>
#include <getopt.h>

/* random */
std::uniform_int_distribution<dht::Value::Id> udist;
std::mt19937_64 rand_;

/* DHT */
const std::string DEFAULT_BOOTSTRAP_NODE = "bootstrap.ring.cx";
const std::string DEFAULT_BOOTSTRAP_PORT = "4222";
const std::string CONNECTION_FAILURE_MSG = "err.. Failed to connect to the DHT.";

/* Command line parsing */
struct ParsedArgs {
    bool fail {false};
    bool help {false};
    dht::InfoHash get_hash {dht::zeroes};
};

static const constexpr struct option long_options[] = {
   {"help",       no_argument      , nullptr, 'h'},
   {"get",        required_argument, nullptr, 'g'},
   {nullptr,      0                , nullptr,  0}
};

ParsedArgs parseArgs(int argc, char *argv[]) {
    ParsedArgs pa;
    int opt;
    while ((opt = getopt_long(argc, argv, "hg:", long_options, nullptr)) != -1) {
        switch (opt) {
        case 'h':
            pa.help = true;
            break;
        case 'g': {
            std::stringstream ss;
            ss << optarg;
            pa.get_hash = dht::InfoHash(ss.str());
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
    }

    if (parsed_args.help) {
        print_help();
        return 0;
    }

    dht::DhtRunner node;
    node.run(0, dht::crypto::generateIdentity(), true);
    node.bootstrap(DEFAULT_BOOTSTRAP_NODE, DEFAULT_BOOTSTRAP_PORT);

    if (parsed_args.get_hash != dht::zeroes) {
        auto values = node.get(parsed_args.get_hash).get();
        if (values.size() == 1) {
            auto& b = values.front()->data;
            std::string s {b.begin(), b.end()};
            std::cout << s << std::endl;
        } else if (values.size() > 1) {
            std::cout << "Multiple candidates..." << std::endl;
            size_t n {0};
            for (auto& v : values) {
                auto& b = v->data;
                std::string s {b.begin(), b.end()};

                std::cout << "Candidate #" << n++ << std::endl;
                std::istringstream iss {s};
                for (std::string line; std::getline(iss, line);) {
                    std::cout << "\t" << line << std::endl;
                }
            }
        }
    } else {
        auto mtx = std::make_shared<std::mutex>();
        auto cv = std::make_shared<std::condition_variable>();
        std::unique_lock<std::mutex> lk(*mtx);

        char in[dht::MAX_VALUE_SIZE];
        std::cin.read(in, dht::MAX_VALUE_SIZE);
        dht::Blob blob {in, in+std::cin.gcount()-1};
        auto v = std::make_shared<dht::Value>(dht::ValueType::USER_DATA.id, blob, udist(rand_));
        auto hash = dht::InfoHash::getRandom();

        node.put(hash, v, [&hash,&mtx,&cv](bool success) {
            std::unique_lock<std::mutex> lk(*mtx);
            if (success)
                std::cout << hash << std::endl;
            else
                std::cerr << CONNECTION_FAILURE_MSG << std::endl;
            cv->notify_all();
        });
        cv->wait(lk);
    }

    node.join();
    return 0;
}
