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

#include "server.h"

#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <functional>
#include <iostream>
#include <queue>
#include <condition_variable>
#include <mutex>

namespace dpaste {
namespace ipc {

const constexpr std::chrono::minutes Server::CONNECTION_TIMEOUT;

void Server::run(uint16_t port) {
    if (running)
        return;

    sockaddr_in sin4;
    std::fill_n((uint8_t*)&sin4, sizeof(sin4), 0);
    sin4.sin_family = AF_INET;
    sin4.sin_addr.s_addr = INADDR_ANY;
    sin4.sin_port = htons(port);

    sockInfo.socket = socket(PF_INET, SOCK_STREAM, 0);
    fcntl(sockInfo.socket, F_SETFL, O_NONBLOCK);
    if (sockInfo.socket >= 0) {
        auto rc = bind(sockInfo.socket, (sockaddr*)&sin4, sizeof(sin4));
        if(rc < 0) {
            if(errno != EINTR)
                perror("bind");
            throw ServerInitException("Failed to bind on socket.");
        }
        sockInfo.bound.second = sizeof(sockInfo.bound.first);
        getsockname(sockInfo.socket, (sockaddr*)&sockInfo.bound.first, &sockInfo.bound.second);
    } else
        throw ServerInitException("Cannot create socket.");

    running = true;

    node.run();

    recvThread = std::thread([this] {
        try {
            while (true) {
                listen(sockInfo.socket, 16);
                struct timeval tv {
                    /*.tv_sec = */0,
                    /*.tv_usec = */250000
                };
                fd_set readfds;

                FD_ZERO(&readfds);
                FD_SET(sockInfo.socket, &readfds);

                int rc = select(sockInfo.socket+1, &readfds, nullptr, nullptr, &tv);
                if(rc < 0) {
                    if(errno != EINTR) {
                        perror("select");
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }

                if(not running)
                    break;

                if (rc) {
                    auto s = accept(sockInfo.socket, nullptr, nullptr);
                    /* TODO: Remove this debug log */
                    std::cout << "Got a connection request.. Accepting." << std::endl;
                    connectionThreads.emplace_back(std::thread(std::bind(&Server::handleConnection, this, s)));
                }

            }
        } catch (const std::exception& e) {
            std::cerr << "Error in networking thread: " << e.what() << std::endl;
        }
        close(sockInfo.socket);
    });
}

void Server::stop() {
    std::cout << "Stopping server..." << std::endl;
    running = false;
    node.stop();
    for (auto& t : connectionThreads) {
        if (t.joinable())
            t.join();
    }
    if (recvThread.joinable())
        recvThread.join();
}

void Server::handleConnection(int socket) {
    using clock = std::chrono::steady_clock;
    clock::time_point last_packet {clock::now()};

    std::mutex resp_mtx;
    std::condition_variable cv;
    auto over = std::make_shared<bool>();
    auto prending_responses = std::make_shared<std::queue<dht::Blob>>();
    while (true) {
        struct timeval tv {/*.tv_sec = */0, /*.tv_usec = */250000};
        fd_set readfds;

        FD_ZERO(&readfds);
        if(socket >= 0)
            FD_SET(socket, &readfds);

        int rc = select(socket+1, &readfds, nullptr, nullptr, &tv);
        if(rc < 0) {
            if(errno != EINTR) {
                perror("select");
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        {
            /* send pending prending_responses */
            std::lock_guard<std::mutex> lk(resp_mtx);
            while (not prending_responses->empty()) {
                auto b = prending_responses->front();
                send(socket, b.data(), b.size(), 0);
                prending_responses->pop();
            }
        }

        if (not running or *over or clock::now() > last_packet + CONNECTION_TIMEOUT)
            break;

        if (rc > 0) {
            last_packet = clock::now();

            std::array<uint8_t, MAX_PACKET_SIZE> buf;
            recv(socket, buf.data(), MAX_PACKET_SIZE, 0);

            dht::Blob blob {buf.begin(), buf.end()};
            std::weak_ptr<bool> wover = over;
            std::weak_ptr<std::queue<dht::Blob>> w_pending_responses = prending_responses;

            handlePacket(std::move(blob),
                [wover,w_pending_responses,&cv,&resp_mtx] (bool stop, dht::Blob&& blob) {
                    if (auto prending_responses = w_pending_responses.lock()) {
                    {
                        std::lock_guard<std::mutex> lk(resp_mtx);
                        prending_responses->emplace(std::forward<dht::Blob>(blob));
                    }
                        if (stop)
                            *wover.lock() = true;
                    }
                }
            );
        }
    }
    close(socket);
}

void Server::handlePacket(dht::Blob&& blob, SendResponseCb&& cb) {
    ParsedMessage msg;

    msgpack::unpacked unp = msgpack::unpack((const char*)blob.data(), blob.size());
    msg.msgpack_unpack(unp.get());
    auto code = msg.code;
    switch (msg.type) {
        case MessageType::Paste: {
            node.paste(code, std::move(msg.paste_value), [this,code,cb](bool success) {
                if (success)
                    cb(true, makePasteResponse(code));
                else
                    cb(true, makeErrorResponse());
            });
            break;
        }
        case MessageType::Get: {
            node.get(code, [this,code,cb](std::vector<dht::Blob> pasted_content) {
                cb(true, makeGetResponse(code, std::move(pasted_content)));
            });
            break;
        }
        default:
            break;
    }
}

dht::Blob Server::makeErrorResponse() {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);

    pk.pack_map(1);
    pk.pack(std::string("e")); pk.pack(1);
    return dht::Blob {buffer.data(), buffer.data()+buffer.size()};
}

dht::Blob Server::makePasteResponse(const std::string& code) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);

    pk.pack_map(1);
    pk.pack(std::string("r")); pk.pack_map(1);
      pk.pack(std::string("code")); pk.pack(code);

    return dht::Blob {buffer.data(), buffer.data()+buffer.size()};
}
dht::Blob Server::makeGetResponse(const std::string& code, std::vector<dht::Blob>&& pasted_content) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> pk(&buffer);

    pk.pack_map(1);
    pk.pack(std::string("r")); pk.pack_map(2);
      pk.pack(std::string("code")); pk.pack(code);
      pk.pack(std::string("pasted")); pk.pack(pasted_content);

    return dht::Blob {buffer.data(), buffer.data()+buffer.size()};
}

} /* ipc */
} /* dpaste */

