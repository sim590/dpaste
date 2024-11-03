/*
 * Copyright © 2017-2020 Simon Désaulniers
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

#include <fstream>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <nlohmann/json.hpp>
#include <b64/decode.h>

#include "curlpp/Options.hpp"

#include "http_client.h"
#include "node.h"

namespace dpaste {

using json = nlohmann::json;

static std::ofstream null("/dev/null");

bool HttpClient::isAvailable() const {
    return put(dht::InfoHash::getRandom().toString(), "hello world");
}

std::vector<std::string> HttpClient::get(const std::string& code) const {
    try {
        curlpp::Cleanup mycleanup;
        curlpp::Easy req;
        req.setOpt<curlpp::options::Port>(port);
        std::stringstream response;
        std::vector<std::string> data;
        req.setOpt<curlpp::options::Url>(HTTP_PROTO+
                host+"/"+dht::InfoHash::get(code).toString()
                +"?user_type="+dpaste::Node::DPASTE_USER_TYPE
        );
        req.setOpt(curlpp::Options::WriteStream(&response));

        try {
            req.perform();
            /* server gives code 200 when everything is fine. */
            if (curlpp::Infos::ResponseCode::get(req) == 200) {
                auto pr = json::parse(response.str());
                for (const auto& entry : pr) {
                    std::istringstream iss(entry["base64"].dump());
                    std::stringstream oss;
                    base64::decoder d;
                    d.decode(iss, oss);
                    data.emplace_back(oss.str());
                }
            }
        } catch (curlpp::RuntimeError & e) { }

        return data;
    } catch (curlpp::LogicError & e) { return {}; }
}

bool HttpClient::put(const std::string& code, const std::string& data) const {
    try {
        curlpp::Cleanup mycleanup;
        curlpp::Easy req;
        req.setOpt<curlpp::options::Port>(port);
        req.setOpt<curlpp::options::Url>(HTTP_PROTO+host+"/"+dht::InfoHash::get(code).toString());
        req.setOpt(curlpp::Options::WriteStream(&null));
        {
            curlpp::Forms form_parts;
            form_parts.push_back(new curlpp::FormParts::Content("user_type", dpaste::Node::DPASTE_USER_TYPE));
            form_parts.push_back(new curlpp::FormParts::Content("data", data));
            req.setOpt(new curlpp::options::HttpPost(form_parts));
        }

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

