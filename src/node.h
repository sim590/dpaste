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

#pragma once

#include <cstdint>
#include <string>

#include <opendht/dhtrunner.h>
#include <opendht/value.h>
#include <opendht/callbacks.h>

namespace dpaste {

class Node {
	static const constexpr char* DEFAULT_BOOTSTRAP_NODE = "bootstrap.ring.cx";
	static const constexpr char* DEFAULT_BOOTSTRAP_PORT = "4222";
	static const constexpr char* CONNECTION_FAILURE_MSG = "err.. Failed to connect to the DHT.";

public:
    using PastedCallback = std::function<void(std::vector<dht::Blob>)>;

	Node () {}
	virtual ~Node () {}

	void run(uint16_t port = 0, std::string bootstrap_hostname = DEFAULT_BOOTSTRAP_NODE, std::string bootstrap_port = DEFAULT_BOOTSTRAP_PORT) {
		if (running)
			return;
		node.run(port, dht::crypto::generateIdentity(), true);
		node.bootstrap(bootstrap_hostname, bootstrap_port);
		running = true;
	};

    void stop() { node.join(); }

	/**
	 * Pastes a blob on the DHT under a random hash.
	 *
	 * @param blob  The blob to paste.
	 * @param cb	A function to execute when paste is done. If empty, the
	 *				function will block until paste is done.
	 *
	 * @return the hash under which the blob is pasted.
	 */
	std::string paste(dht::Blob&& blob, dht::DoneCallback&& cb = {});

	/**
	 * Pastes a blob on the DHT under a given hash.
	 *
	 * @param blob  The blob to paste.
	 * @param cb	A function to execute when paste is done. If empty, the
	 *				function will block until done.
	 *
	 * @return the hash under which the blob is pasted.
	 */
	void paste(std::string hash, dht::Blob&& blob, dht::DoneCallback&& cb = {}) {
        paste(dht::InfoHash(hash), std::forward<dht::Blob>(blob), std::forward<dht::DoneCallback>(cb));
    }

	/**
	 * Recover a blob under a given hash.
	 *
	 * @param hash  The hash to lookup.
	 * @param cb	A function to execute when the pasted blob is retrieved.
	 */
	void get(std::string hash, PastedCallback&& cb);

	/**
	 * Recover blob values under a given hash.
	 *
	 * @param hash  The hash to lookup.
     *
     * @return the blobs.
	 */
    std::vector<dht::Blob> get(std::string hash);

private:
    static const constexpr char* DPASTE_USER_TYPE = "dpaste";

	void paste(dht::InfoHash hash, dht::Blob&& blob, dht::DoneCallback&& cb = {});

    dht::DhtRunner node;
	bool running {false};
};

} /* dpaste */

