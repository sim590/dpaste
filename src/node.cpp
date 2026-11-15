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
#include <random>
#include <future>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>

#include <opendht.h>
#include <glibmm.h>

#include "node.h"
#include "log.h"

namespace dpaste {

const constexpr char* Node::DPASTE_USER_TYPE;

namespace {

/* Upper bound on the wait for the local node's callbacks. A callback that is
 * never invoked must not hang the CLI: the stragglers stay marked as failed,
 * so Bin::paste reports the failure (or retries on the local node) instead of
 * blocking forever. A duplicated callback must not keep decrementing the
 * pending count either, since it would wrap around and never release the
 * wait. */
constexpr std::chrono::seconds BATCH_WAIT_TIMEOUT {60};

/**
 * Directory holding the on-disk OpenDHT node state.
 * Can be overridden with the DPASTE_CACHE_DIR environment variable
 * (e.g. for tests). Defaults to ${XDG_CACHE_HOME}/dpaste.
 */
std::optional<std::filesystem::path> create_cache_dir() {
    const char* env = std::getenv("DPASTE_CACHE_DIR");
    const std::filesystem::path cache_dir = env and *env ? env : Glib::get_user_cache_dir() + "/dpaste";
    std::error_code ec;
    std::filesystem::create_directories(cache_dir, ec);
    if (ec) {
        DPASTE_MSG("warning: could not create cache directory '%s': %s; the DHT state will not be persisted", cache_dir.string().c_str(), ec.message().c_str());
        return std::nullopt;
    }
    return cache_dir;
}

} /* anonymous namespace */

void Node::run(uint16_t port, std::string bootstrap_hostname, std::string bootstrap_port) {
    if (running_)
        return;

    dht::DhtRunner::Config config;
    /* Ask OpenDHT to load its state (routing table) on start and save it on
     * shutdown; this reuses known peers and improves bootstrap resilience. */
    if (const auto cache_dir = create_cache_dir())
        config.dht_config.node_config.persist_path = (*cache_dir / "nodes").string();
    config.threaded = true;
    node_.run(port, config);

    node_.bootstrap(bootstrap_hostname, bootstrap_port);
    running_ = true;
}

bool Node::paste(const std::string& code, dht::Blob&& blob, dht::DoneCallbackSimple&& cb) {
    auto v = std::make_shared<dht::Value>(std::forward<dht::Blob>(blob));
    v->user_type = DPASTE_USER_TYPE;

    auto hash = dht::InfoHash::get(code);

    if (cb) {
        node_.put(hash, v, cb);
        return true;
    } else {
        std::mutex mtx;
        std::condition_variable cv;
        bool done = false, success_ {false};
        node_.put(hash, v, [&](bool success) {
            {
                std::unique_lock<std::mutex> lk(mtx);
                if (not success)
                    std::cerr << OPERATION_FAILURE_MSG << " (put)" << std::endl;
                else
                    success_ = true;
                done = true;
            }
            cv.notify_all();
        });
        /* Submit before taking the lock: a callback invoked inline by put()
         * would otherwise deadlock on the same non-recursive mutex. */
        std::unique_lock<std::mutex> lk(mtx);
        if (not cv.wait_for(lk, BATCH_WAIT_TIMEOUT, [&](){ return done; }))
            std::cerr << OPERATION_FAILURE_MSG << " (put timeout)" << std::endl;
        return success_;
    }
}

void Node::get(const std::string& code, PastedCallback&& pcb) {
    auto blobs = std::make_shared<std::vector<dht::Blob>>();
    node_.get(dht::InfoHash::get(code),
        [blobs](std::shared_ptr<dht::Value> value) {
            blobs->emplace_back(value->data);
            return true;
        },
        [pcb,blobs](bool success) {
            if (not success)
                std::cerr << OPERATION_FAILURE_MSG << " (get)" << std::endl;
            else if (pcb)
                pcb(*blobs);
        }, dht::Value::AllFilter(), dht::Where{}.userType(std::string(DPASTE_USER_TYPE))
    );
}

std::vector<dht::Blob> Node::get(const std::string& code) {
    auto values = node_.get(dht::InfoHash::get(code),
            dht::Value::AllFilter(),
            dht::Where{}.userType(DPASTE_USER_TYPE)).get();
    std::vector<dht::Blob> blobs (values.size());
    std::transform(values.begin(), values.end(), blobs.begin(), [] (const decltype(values)::value_type& value) {
        return value->data;
    });
    return blobs;
}

std::vector<bool>
Node::pasteAll(std::vector<std::pair<std::string, dht::Blob>>&& items) {
    std::vector<bool> results(items.size(), false);
    if (items.empty())
        return results;

    std::mutex mtx;
    std::condition_variable cv;
    size_t pending = items.size();
    size_t idx     = 0;

    for (auto& [code, blob] : items) {
        auto v = std::make_shared<dht::Value>(std::move(blob));
        v->user_type = DPASTE_USER_TYPE;

        node_.put(dht::InfoHash::get(code), v, [&, i = idx++](bool success) {
            std::lock_guard<std::mutex> g(mtx);
            if (not success)
                std::cerr << OPERATION_FAILURE_MSG << " (put)" << std::endl;
            else
                results[i] = true;
            if (pending > 0)
                --pending;
            if (pending == 0)
                cv.notify_all();
        });
    }

    /* Every put has been submitted; take the lock only now so that a callback
     * invoked inline by put() cannot deadlock on the same non-recursive
     * mutex. */
    std::unique_lock<std::mutex> lk(mtx);
    cv.wait_for(lk, BATCH_WAIT_TIMEOUT, [&]{ return pending == 0; });
    if (pending > 0)
        std::cerr << OPERATION_FAILURE_MSG << " (put timeout)" << std::endl;
    return results;
}

std::vector<std::vector<dht::Blob>>
Node::getAll(const std::vector<std::string>& codes) {
    /* Submit every lookup first so that they run concurrently, then collect
     * the results. The futures are bounded by OpenDHT's own query timeout,
     * so no condition variable or callback lifetime handling is needed. */
    std::vector<std::future<std::vector<std::shared_ptr<dht::Value>>>> futures;
    futures.reserve(codes.size());
    for (const auto& code : codes)
        futures.emplace_back(node_.get(dht::InfoHash::get(code),
                                       dht::Value::AllFilter(),
                                       dht::Where{}.userType(std::string(DPASTE_USER_TYPE))));

    std::vector<std::vector<dht::Blob>> results;
    results.reserve(futures.size());
    for (auto& future : futures) {
        const auto values = future.get();
        std::vector<dht::Blob> blobs;
        blobs.reserve(values.size());
        for (const auto& value : values)
            blobs.emplace_back(value->data);
        results.emplace_back(std::move(blobs));
    }
    return results;
}

} /* dpaste */
