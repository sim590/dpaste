/*
 * Copyright © 2026 Jaël Champagne Gareau
 * Author: Jaël Champagne Gareau <gareau_jael@hotmail.com>
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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "http_client.h"

namespace dpaste {
namespace tests {

class FakeHttpServer {
public:
    explicit FakeHttpServer(const std::string& response_body) : response_body(response_body) {
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        CATCH_REQUIRE(listen_fd >= 0);

        sockaddr_in address {};
        address.sin_family      = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port        = htons(0);
        CATCH_REQUIRE(bind(listen_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        CATCH_REQUIRE(listen(listen_fd, 1) == 0);

        socklen_t length = sizeof(address);
        CATCH_REQUIRE(getsockname(listen_fd, reinterpret_cast<sockaddr*>(&address), &length) == 0);
        server_port = ntohs(address.sin_port);
        worker = std::thread(&FakeHttpServer::serve, this);
    }

    ~FakeHttpServer() {
        join();
        if (listen_fd >= 0)
            close(listen_fd);
    }

    void join() {
        if (worker.joinable())
            worker.join();
    }

    long port() const { return server_port; }
    const std::string& request() const { return request_data; }

private:
    void serve() {
        const int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0)
            return;

        char buffer[1024];
        while (request_data.find("\r\n\r\n") == std::string::npos) {
            const auto count = recv(client_fd, buffer, sizeof(buffer), 0);
            if (count <= 0)
                break;
            request_data.append(buffer, static_cast<size_t>(count));
        }

        const auto header_end = request_data.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            const auto content_length = request_data.find("Content-Length:");
            size_t body_length        = 0;
            if (content_length != std::string::npos)
                body_length = std::stoul(request_data.substr(content_length + 15));

            while (request_data.size() < header_end + 4 + body_length) {
                const auto count = recv(client_fd, buffer, sizeof(buffer), 0);
                if (count <= 0)
                    break;
                request_data.append(buffer, static_cast<size_t>(count));
            }
        }

        const std::string response = "HTTP/1.1 200 OK\r\n"
                "Content-Length: " + std::to_string(response_body.size()) + "\r\n"
                "Connection: close\r\n\r\n" + response_body;
        size_t sent = 0;
        while (sent < response.size()) {
            const auto count = send(client_fd, response.data() + sent, response.size() - sent, 0);
            if (count <= 0)
                break;
            sent += static_cast<size_t>(count);
        }
        close(client_fd);
    }

    int listen_fd    = -1;
    long server_port = 0;
    std::string response_body;
    std::string request_data;
    std::thread worker;
};

static long closed_loopback_port() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    CATCH_REQUIRE(fd >= 0);

    sockaddr_in address {};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port        = htons(0);
    CATCH_REQUIRE(bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);

    socklen_t length = sizeof(address);
    CATCH_REQUIRE(getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) == 0);
    const long port = ntohs(address.sin_port);
    close(fd);
    return port;
}

CATCH_TEST_CASE("HttpClient gets the dpaste Value from proxy NDJSON", "[HttpClient][get]") {
    FakeHttpServer server("{\"utype\":\"other\",\"data\":\"bm90IHRoaXM=\"}\n"
            "{\"utype\":\"dpaste\",\"data\":\"aGVsbG8=\"}\n");
    HttpClient client("127.0.0.1", server.port());

    const auto values = client.get("get-test");
    CATCH_REQUIRE ( values.size() == 1 );
    CATCH_REQUIRE ( values.front() == "hello" );
    server.join();
}

CATCH_TEST_CASE("HttpClient gets all dpaste Values from proxy NDJSON", "[HttpClient][get]") {
    FakeHttpServer server("{\"utype\":\"dpaste\",\"data\":\"aGVsbG8=\"}\n"
            "{\"utype\":\"dpaste\",\"data\":\"d29ybGQ=\"}\n");
    HttpClient client("127.0.0.1", server.port());

    const auto values = client.get("get-test");
    CATCH_REQUIRE ( values.size() == 2 );
    CATCH_REQUIRE ( values[0] == "hello" );
    CATCH_REQUIRE ( values[1] == "world" );
    server.join();
}

CATCH_TEST_CASE("HttpClient probes the proxy stats endpoint", "[HttpClient][isAvailable]") {
    FakeHttpServer server("{\"serverStartTime\":1700000000,\"listenCount\":0}\n");
    HttpClient client("127.0.0.1", server.port());

    CATCH_REQUIRE ( client.isAvailable() );
    server.join();
    CATCH_REQUIRE ( server.request().find("GET /node/stats") != std::string::npos );
}

CATCH_TEST_CASE("HttpClient rejects a proxy without stats", "[HttpClient][isAvailable]") {
    FakeHttpServer server("{\"foo\":42}\n");
    HttpClient client("127.0.0.1", server.port());

    CATCH_REQUIRE_FALSE ( client.isAvailable() );
    server.join();
}

CATCH_TEST_CASE("HttpClient puts a dpaste Value through the proxy", "[HttpClient][put]") {
    FakeHttpServer server("");
    HttpClient client("127.0.0.1", server.port());

    CATCH_REQUIRE(client.put("put-test", "hello"));
    server.join();
    const auto body_start = server.request().find("\r\n\r\n");
    CATCH_REQUIRE(body_start != std::string::npos);
    const auto body = server.request().substr(body_start + 4);
    CATCH_REQUIRE(body.find("\"utype\":\"dpaste\"") != std::string::npos);
    CATCH_REQUIRE(body.find("\"data\":\"aGVsbG8=\"") != std::string::npos);
}

CATCH_TEST_CASE("HttpClient handles an unavailable proxy", "[HttpClient][errors]") {
    const auto port = closed_loopback_port();
    HttpClient client("127.0.0.1", port);

    CATCH_REQUIRE ( client.get("unavailable-test").empty() );
    CATCH_REQUIRE_FALSE ( client.isAvailable() );
    CATCH_REQUIRE_FALSE ( client.put("unavailable-test", "hello") );
}

} /* tests */
} /* dpaste */

/* vim: set ts=4 sw=4 tw=120 et :*/

