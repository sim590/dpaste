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

#include <algorithm>
#include <cstdint>
#include <algorithm>
#include <list>
#include <sstream>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <nlohmann/json.hpp>
#include <b64/decode.h>
#include <b64/encode.h>

#include "http_client.h"
#include "node.h"

namespace dpaste {

using json = nlohmann::json;

std::string HttpClient::get(const std::string& code) const {
    try {
        curlpp::Cleanup mycleanup;
        curlpp::Easy req;
        std::stringstream response;
        req.setOpt<curlpp::options::Url>(HTTP_PROTO + host + ":" +
                std::to_string(port) + "/key/" +
                dht::InfoHash::get(code).toString());
        req.setOpt(curlpp::Options::WriteStream(&response));

        try {
            req.perform();
            /* server gives code 200 when everything is fine. */
            if (curlpp::Infos::ResponseCode::get(req) == 200) {
                std::istringstream lines(response.str());
                std::string line;
                while (std::getline(lines, line)) {
                    try {
                        const auto value = json::parse(line);
                        if (value.is_object() &&
                            value.value("utype", std::string {}) == Node::DPASTE_USER_TYPE &&
                            value.contains("data") && value["data"].is_string()) {
                            std::istringstream encoded(value["data"].get<std::string>());
                            std::ostringstream decoded;
                            base64::decoder decoder;
                            decoder.decode(encoded, decoded);
                            return decoded.str();
                        }
                    } catch (const std::exception&) {
                        /* Ignore malformed or incompatible Value objects. */
                    }
                }
            }
        } catch (curlpp::RuntimeError & e) { }

        return {};
    } catch (curlpp::LogicError & e) { return {}; }
}

bool HttpClient::put(const std::string& code, const std::string& data) const {
    try {
        curlpp::Cleanup mycleanup;
        curlpp::Easy req;
        req.setOpt<curlpp::options::Url>(HTTP_PROTO + host + ":" +
                std::to_string(port) + "/key/" +
                dht::InfoHash::get(code).toString());

        std::istringstream input(data);
        std::ostringstream encoded;
        base64::encoder encoder;
        encoder.encode(input, encoded);

        /* libb64 wraps long output lines; the proxy expects one compact
         * standard-base64 string in the JSON Value. */
        auto encoded_data = encoded.str();
        encoded_data.erase(std::remove_if(encoded_data.begin(), encoded_data.end(),
                [](char c) { return c == '\r' || c == '\n'; }), encoded_data.end());

        const auto body = json {
            {"id", "0"},
            {"type", 0},
            {"data", encoded_data},
            {"utype", Node::DPASTE_USER_TYPE}
        }.dump();
        req.setOpt(curlpp::Options::PostFields(body));
        req.setOpt(new curlpp::options::HttpHeader(
                std::list<std::string> {"Content-Type: application/json"}));
        std::stringstream response;
        req.setOpt(curlpp::Options::WriteStream(&response));

        try {
            req.perform();
            return curlpp::Infos::ResponseCode::get(req) == 200;
        } catch (curlpp::RuntimeError & e) {
            return false;
        }
    } catch (curlpp::LogicError & e) { return false; }
}

} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */
