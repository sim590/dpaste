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

std::uniform_int_distribution<dht::Value::Id> vid_dist;
std::mt19937_64 rand_;

std::string Node::paste(dht::Blob&& blob, dht::DoneCallback&& cb) {
	auto h = dht::InfoHash::getRandom();
	paste(h, std::forward<dht::Blob>(blob), std::forward<dht::DoneCallback>(cb));
	return h.toString();
}

void Node::paste(dht::InfoHash hash, dht::Blob&& blob, dht::DoneCallback&& cb) {
	auto v = std::make_shared<dht::Value>(std::forward<dht::Blob>(blob));
	v->id = vid_dist(rand_);
	v->type = dht::ValueType::USER_DATA.id;
    v->user_type = DPASTE_USER_TYPE;

	if (cb)
		node.put(hash, v, cb);
	else {
		auto mtx = std::make_shared<std::mutex>();
		auto cv = std::make_shared<std::condition_variable>();
		std::unique_lock<std::mutex> lk(*mtx);
		node.put(hash, v, [&hash,&mtx,&cv] (bool success) {
			std::unique_lock<std::mutex> lk(*mtx);
			if (not success)
				/* TODO: use better logging methods */
				std::cerr << CONNECTION_FAILURE_MSG << std::endl;
			cv->notify_all();
		});
		cv->wait(lk);
	}
}

void Node::get(std::string hash, PastedCallback&& pcb) {
    auto blobs = std::make_shared<std::vector<dht::Blob>>();
    node.get(dht::InfoHash(hash),
        [blobs](std::shared_ptr<dht::Value> value) {
            blobs->emplace_back(value->data);
            return true;
        },
        [pcb,blobs] (bool success) {
            if (not success)
				std::cerr << CONNECTION_FAILURE_MSG << std::endl;
            else if (pcb)
                pcb(*blobs);
        }, dht::Value::AllFilter(), dht::Where {}.userType(std::string(DPASTE_USER_TYPE))
    );
}

std::vector<dht::Blob> Node::get(std::string hash) {
    auto values = node.get(dht::InfoHash(hash), dht::Value::AllFilter(), dht::Where {}.userType(DPASTE_USER_TYPE)).get();
    std::vector<dht::Blob> blobs (values.size());
    std::transform(values.begin(), values.end(), blobs.begin(), [] (const decltype(values)::value_type& value) {
        return value->data;
    });
    return blobs;
}

} /* dpaste */

