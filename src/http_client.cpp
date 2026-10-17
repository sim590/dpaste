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
#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <thread>
#include <sys/select.h>

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Multi.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <nlohmann/json.hpp>
#include <b64/decode.h>
#include <b64/encode.h>

#include "http_client.h"
#include "log.h"
#include "node.h"

namespace dpaste {

using json = nlohmann::json;

namespace {

std::string urlHost(const std::string& host) {
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
        return host;
    if (host.find(':') != std::string::npos)
        return "[" + host + "]";
    return host;
}

/**
 * Initialize libcurl once for the lifetime of the process. It must not be
 * initialized or cleaned up while requests are in flight, which is what the
 * previous per-request curlpp::Cleanup did (see #43, #48).
 */
void ensureCurlInit() {
    static const curlpp::Cleanup cleanup;
}

/**
 * Parse the JSON Values of a proxy response body. DhtProxyServer returns one
 * JSON Value per line; do not parse the whole response as a single JSON
 * document.
 */
std::vector<std::string> parseValues(const std::string& body) {
    std::vector<std::string> values;
    std::istringstream lines(body);
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
                values.emplace_back(decoded.str());
            }
        } catch (const std::exception&) {
            /* Ignore malformed or incompatible Value objects. */
        }
    }
    return values;
}

/* Build the JSON Value body of a PUT request. */
std::string makeRequestBody(const std::string& data) {
    std::istringstream input(data);
    std::ostringstream encoded;
    base64::encoder encoder;
    encoder.encode(input, encoded);

    /* libb64 wraps long output lines; the proxy expects one compact
     * standard-base64 string in the JSON Value. */
    auto encoded_data = encoded.str();
    encoded_data.erase(std::remove_if(encoded_data.begin(), encoded_data.end(),
            [](char c) { return c == '\r' || c == '\n'; }), encoded_data.end());

    return json {
        {"data", encoded_data},
        {"utype", Node::DPASTE_USER_TYPE}
    }.dump();
}

/* Maximum number of transfers kept in flight at once by the batch operations. */
static const constexpr size_t MAX_INFLIGHT_REQUESTS {8};
/* Timeouts (in seconds) so that a stuck transfer cannot stall a whole batch. */
static const constexpr long CONNECT_TIMEOUT_S {5};
static const constexpr long REQUEST_TIMEOUT_S {120};

/* Wait until one of the multi transfers makes progress: select() on curl's
 * file descriptors, or a short sleep when it only waits on a timer (DNS,
 * timeouts). */
void waitTransferActivity(curlpp::Multi& multi) {
    fd_set fdread, fdwrite, fdexcep;
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);

    int maxfd = -1;
    multi.fdset(&fdread, &fdwrite, &fdexcep, &maxfd);
    if (maxfd == -1)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    else
        ::select(maxfd + 1, &fdread, &fdwrite, &fdexcep, nullptr);
}

/**
 * Run `count` requests concurrently on one curl multi handle, with at most
 * MAX_INFLIGHT_REQUESTS transfers in flight. Blocks until every request has
 * completed.
 *
 * @param makeRequest  Called to prepare and register the request for one item.
 * @param collect      Called (with the transfer's result) once the request for
 *                     an item has completed.
 */
void runBatch(size_t count,
              const std::function<std::unique_ptr<curlpp::Easy>(size_t)>& makeRequest,
              const std::function<void(size_t, const curlpp::Easy&, CURLcode)>& collect)
{
    if (count == 0)
        return;

    ensureCurlInit();

    curlpp::Multi multi;
    std::vector<std::unique_ptr<curlpp::Easy>> handles;
    std::map<const curlpp::Easy*, size_t> attached;

    auto add = [&](size_t i) {
        auto req = makeRequest(i);
        attached.emplace(req.get(), i);
        multi.add(req.get());
        handles.emplace_back(std::move(req));
    };

    size_t next   = 0;
    size_t active = 0;
    try {
        while (next < count and active < MAX_INFLIGHT_REQUESTS) {
            add(next++);
            ++active;
        }

        int nbLeft = 0;
        multi.perform(&nbLeft);
        while (active > 0) {
            /* Collect the transfers that completed and start the next ones. */
            for (auto& [handle, info] : multi.info()) {
                if (info.msg != CURLMSG_DONE)
                    continue;
                const auto it = attached.find(handle);
                if (it == attached.end())
                    continue;
                const size_t i = it->second;
                attached.erase(it);

                collect(i, *handle, info.code);
                multi.remove(handle);
                --active;
                if (next < count) {
                    add(next++);
                    ++active;
                }
            }
            if (active == 0)
                break;
            waitTransferActivity(multi);
            multi.perform(&nbLeft);
        }
    } catch (curlpp::RuntimeError & e) {
        DPASTE_MSG("%s", e.what());
    } catch (curlpp::LogicError & e) {
        DPASTE_MSG("%s", e.what());
    }

    /* Never leave the multi handle with dangling easy handles. */
    for (const auto& [handle, i] : attached)
        multi.remove(handle);
}

} /* anonymous namespace */

bool HttpClient::isAvailable() const {
    try {
        ensureCurlInit();
        curlpp::Easy req;
        std::stringstream response;
        req.setOpt<curlpp::options::Port>(port);
        req.setOpt<curlpp::options::Url>(HTTP_PROTO + urlHost(host) + "/node/stats");
        req.setOpt(curlpp::Options::WriteStream(&response));
        /* Bound the probe so that an unreachable proxy cannot block for the
         * default curl timeouts. */
        req.setOpt(curlpp::Options::Timeout(5));

        try {
            req.perform();
            /* server gives code 200 when everything is fine. */
            if (curlpp::Infos::ResponseCode::get(req) == 200) {
                const auto stats = json::parse(response.str(), nullptr, false);
                /* serverStartTime only exists since OpenDHT 3.5, but
                 * listenCount is present in every version exposing this
                 * endpoint. */
                return stats.is_object()
                    && (stats.contains("serverStartTime") || stats.contains("listenCount"));
            }
        } catch (curlpp::RuntimeError &e) {
            DPASTE_MSG("%s", e.what());
        }

        return false;
    } catch (curlpp::LogicError &e) {
        DPASTE_MSG("%s", e.what());
        return false;
    }
}

std::vector<std::vector<std::string>> HttpClient::getAll(const std::vector<std::string>& codes) const {
    std::vector<std::vector<std::string>> results(codes.size());
    std::vector<std::stringstream> responses(codes.size());

    runBatch(codes.size(),
        [&](size_t i) {
            auto req = std::make_unique<curlpp::Easy>();
            req->setOpt<curlpp::options::Port>(port);
            req->setOpt<curlpp::options::Url>(HTTP_PROTO + urlHost(host) + "/key/" +
                    dht::InfoHash::get(codes[i]).toString());
            req->setOpt(curlpp::Options::WriteStream(&responses[i]));
            req->setOpt(curlpp::Options::ConnectTimeout(CONNECT_TIMEOUT_S));
            req->setOpt(curlpp::Options::Timeout(REQUEST_TIMEOUT_S));
            return req;
        },
        [&](size_t i, const curlpp::Easy& req, CURLcode code) {
            if (code != CURLE_OK) {
                DPASTE_MSG("%s", curl_easy_strerror(code));
                return;
            }
            /* server gives code 200 when everything is fine. */
            if (curlpp::Infos::ResponseCode::get(req) == 200)
                results[i] = parseValues(responses[i].str());
        });

    return results;
}

std::vector<std::string> HttpClient::get(const std::string& code) const {
    return getAll({code}).front();
}

std::vector<bool> HttpClient::putAll(const std::vector<std::pair<std::string, std::string>>& items) const {
    std::vector<bool> results(items.size(), false);
    std::vector<std::stringstream> responses(items.size());

    runBatch(items.size(),
        [&](size_t i) {
            auto req = std::make_unique<curlpp::Easy>();
            req->setOpt<curlpp::options::Port>(port);
            req->setOpt<curlpp::options::Url>(HTTP_PROTO + urlHost(host) + "/key/" +
                    dht::InfoHash::get(items[i].first).toString());
            req->setOpt(curlpp::Options::PostFields(makeRequestBody(items[i].second)));
            req->setOpt(new curlpp::options::HttpHeader(
                    std::list<std::string> {"Content-Type: application/json"}));
            req->setOpt(curlpp::Options::WriteStream(&responses[i]));
            req->setOpt(curlpp::Options::ConnectTimeout(CONNECT_TIMEOUT_S));
            req->setOpt(curlpp::Options::Timeout(REQUEST_TIMEOUT_S));
            return req;
        },
        [&](size_t i, const curlpp::Easy& req, CURLcode code) {
            if (code != CURLE_OK) {
                DPASTE_MSG("%s", curl_easy_strerror(code));
                return;
            }
            results[i] = curlpp::Infos::ResponseCode::get(req) == 200;
        });

    return results;
}

bool HttpClient::put(const std::string& code, const std::string& data) const {
    return putAll({{code, data}}).front();
}

} /* dpaste */

/* vim:set et sw=4 ts=4 tw=120: */

