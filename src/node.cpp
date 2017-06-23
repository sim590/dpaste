/*
 * Copyright © 2016 Simon Désaulniers
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
#include <random>
#include <future>

#include <opendht.h>

#include "node.h"

namespace dpaste {

void Node::paste(const std::string& code, dht::Blob&& blob, dht::DoneCallbackSimple&& cb) {
    auto v = std::make_shared<dht::Value>(std::forward<dht::Blob>(blob));
    v->user_type = DPASTE_USER_TYPE;

    auto hash = dht::InfoHash::get(code);

    if (cb)
        node.put(hash, v, cb);
    else {
        std::mutex mtx;
        std::condition_variable cv;
        std::unique_lock<std::mutex> lk(mtx);
        bool done {false};
        node.put(hash, v, [&](bool success) {
            if (not success)
                std::cerr << CONNECTION_FAILURE_MSG << std::endl;
            {
                std::unique_lock<std::mutex> lk(mtx);
                done = true;
            }
            cv.notify_all();
        });
        cv.wait(lk, [&](){ return done; });
    }
}

void Node::get(const std::string& code, PastedCallback&& pcb) {
    auto blobs = std::make_shared<std::vector<dht::Blob>>();
    node.get(dht::InfoHash::get(code),
        [blobs](std::shared_ptr<dht::Value> value) {
            blobs->emplace_back(value->data);
            return true;
        },
        [pcb,blobs](bool success) {
            if (not success)
                std::cerr << CONNECTION_FAILURE_MSG << std::endl;
            else if (pcb)
                pcb(*blobs);
        }, dht::Value::AllFilter(), dht::Where{}.userType(std::string(DPASTE_USER_TYPE))
    );
}

std::vector<dht::Blob> Node::get(const std::string& code) {
    auto values = node.get(dht::InfoHash::get(code), dht::Value::AllFilter(), dht::Where{}.userType(DPASTE_USER_TYPE)).get();
    std::vector<dht::Blob> blobs (values.size());
    std::transform(values.begin(), values.end(), blobs.begin(), [] (const decltype(values)::value_type& value) {
        return value->data;
    });
    return blobs;
}

} /* dpaste */

