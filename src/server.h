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
 * along with dpaste. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "node.h"
#include "ipc.h"

#include <cstdint>
#include <string>
#include <utility>
#include <sstream>
#include <list>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <atomic>
#include <array>

namespace dpaste {
namespace ipc {

class ServerInitException : public std::runtime_error {
public:
    explicit ServerInitException(const std::string& what) : std::runtime_error(what) {}
};

using SockAddr = std::pair<sockaddr_storage, socklen_t>;
struct SockInfo {
    int socket;
    SockAddr bound;
};

class Server {
public:
    static const constexpr uint16_t DEFAULT_SERVER_PORT {6489};

    Server() {}
    virtual ~Server() {}

    /**
     * Run the server
     *
     * @param port  Socket port.
     */
    void run(uint16_t port = DEFAULT_SERVER_PORT);

    /**
     * Stop the server.
     */
    void stop();

private:
    static const constexpr std::chrono::minutes CONNECTION_TIMEOUT {1};

    using SendResponseCb = std::function<void(bool, dht::Blob&&)>;

    /**
     * Handle client connection. A socket on which we don't receive any packet
     * for at least 1 minute is considered expired.
     *
     * @param socket  The socket associated to the client.
     */
    void handleConnection(int socket);

    /**
     * Handle a packet received from a client.
     *
     * @param blob  The unserialized packet.
     *
     * @return false if the connection can be closed.
     */
    void handlePacket(dht::Blob&& blob, SendResponseCb&& cb);


    /* response packet serialization */
    dht::Blob makeErrorResponse();
    dht::Blob makePasteResponse(const std::string& code);
    dht::Blob makeGetResponse(const std::string& code, std::vector<dht::Blob>&& pasted_content);

    std::atomic<bool> running {false};
    SockInfo sockInfo;                        /* reception socket info */
    std::thread recvThread;                   /* reception thread */
    std::list<std::thread> connectionThreads; /* per client connection thread */

    dpaste::Node node;
};

} /* ipc */
} /* dpaste */

