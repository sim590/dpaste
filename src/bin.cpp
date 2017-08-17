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

#include <sstream>
#include <iostream>

#include "bin.h"
#include "conf.h"

namespace dpaste {

Bin::Bin(std::string&& code) : code_(code) {
    /* load dpaste config */
    auto config_file = conf::ConfigurationFile();
    config_file.load();
    conf = config_file.getConfiguration();

    long port;
    {
        std::istringstream conv(conf.at("port"));
        conv >> port;
    }

    node.run();
    http_client_ = std::make_unique<HttpClient>(conf.at("host"), port);

    /* operation is put */
    if (code.empty()) {
        char buf[dht::MAX_VALUE_SIZE];
        buffer_.reserve(dht::MAX_VALUE_SIZE);
        std::cin.read(buf, dht::MAX_VALUE_SIZE);
        buffer_ = std::string(buf, buf+std::cin.gcount()-1);
    } else { /* otherwise, it's a get */
        const auto p = code_.find_first_of(DPASTE_CODE_PREFIX);
        code_ = code_.substr(p != std::string::npos ? sizeof(DPASTE_CODE_PREFIX)-1 : p);
    }
}

int Bin::get() {
    /* first try http server */
    auto data = http_client_->get(code_);

    /* if fail, then perform request from local node */
    if (data.empty()) {

        /* get a pasted blob */
        auto values = node.get(code_);
        if (not values.empty()) {
            auto& b = values.front();
            data = std::string(b.begin(), b.end());
        }
        node.stop();
    }
    if (not data.empty())
        std::cout << data << std::endl;
    return 0;
}

int Bin::paste() {
    /* paste a blob on the DHT */
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

    auto success = http_client_->put(code, buffer_);
    if (not success) {
        Node node;
        node.run();

        dht::Blob blob {buffer_.begin(), buffer_.end()};
        success = node.paste(code, std::move(blob));
        node.stop();
    }

    if (success) {
        std::cout << DPASTE_CODE_PREFIX << code << std::endl;
        return 0;
    } else
        return 1;
}

} /* dpaste  */
