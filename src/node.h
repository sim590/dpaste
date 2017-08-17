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

#pragma once

#include <cstdint>
#include <string>
#include <memory>

#include <opendht/dhtrunner.h>
#include <opendht/value.h>
#include <opendht/infohash.h>
#include <opendht/rng.h>
#include <opendht/callbacks.h>

namespace dpaste {

class Node {
    static const constexpr char* DEFAULT_BOOTSTRAP_NODE = "bootstrap.ring.cx";
    static const constexpr char* DEFAULT_BOOTSTRAP_PORT = "4222";
    static const constexpr char* CONNECTION_FAILURE_MSG = "err.. Failed to connect to the DHT.";
    static const constexpr char* OPERATION_FAILURE_MSG = "err.. DHT operation failed.";

public:
    using PastedCallback = std::function<void(std::vector<dht::Blob>)>;

    static const constexpr char* DPASTE_USER_TYPE = "dpaste";

    Node() {}
    virtual ~Node () {}

    void run(uint16_t port = 0, std::string bootstrap_hostname = DEFAULT_BOOTSTRAP_NODE, std::string bootstrap_port = DEFAULT_BOOTSTRAP_PORT) {
        if (running_)
            return;
        node_.run(port, dht::crypto::generateIdentity(), true);
        node_.bootstrap(bootstrap_hostname, bootstrap_port);
        running_ = true;
    };

    void stop() {
        std::condition_variable cv;
        std::mutex m;
        std::atomic_bool done {false};

        node_.shutdown([&]()
        {
            std::lock_guard<std::mutex> lk(m);
            done = true;
            cv.notify_all();
        });

        // wait for shutdown
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&](){ return done.load(); });

        node_.join();
    }

    /**
     * Pastes a blob on the DHT under a given code. If no callback, the function
     * blocks until pasting on the DHT is done.
     *
     * @param blob  The blob to paste.
     * @param cb    A function to execute when paste is done. If empty, the
     *              function will block until done.
     *
     * @return true if success, else false.
     */
    bool paste(const std::string& code, dht::Blob&& blob, dht::DoneCallbackSimple&& cb = {});

    /**
     * Recover a blob under a given code.
     *
     * @param code  The code to lookup.
     * @param cb    A function to execute when the pasted blob is retrieved.
     */
    void get(const std::string& code, PastedCallback&& cb);

    /**
     * Recover blob values under a given code. This function blocks until the
     * DHT has satisfied the request.
     *
     * @param code  The code to lookup.
     *
     * @return the blobs.
     */
    std::vector<dht::Blob> get(const std::string& code);

private:

    dht::DhtRunner node_;
    bool running_ {false};

    std::uniform_int_distribution<uint32_t> codeDist_;
    std::mt19937_64 rand_;
};

} /* dpaste */

