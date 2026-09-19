/*
 * Copyright © 2018 Simon Désaulniers
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

#include <catch2/catch.hpp>

#include <cstdlib>
#include <stdexcept>
#include <unistd.h>

#include "tests.h"
#include "node.h"

namespace dpaste {
namespace tests {

class PirateNodeTester {
    #define YARRRRR
public:
    PirateNodeTester () {}
    virtual ~PirateNodeTester () {}

    bool is_running(const dpaste::Node& n) const { return n.running_; }
};

class CacheFileGuard {
    int fd_ {-1};
    std::string path_;
    bool had_previous_value_ {false};
    std::string previous_value_;

public:
    CacheFileGuard() {
        const char* previous = std::getenv("DPASTE_CACHE_DIR");
        had_previous_value_ = previous != nullptr;
        if (had_previous_value_)
            previous_value_ = previous;

        char path[] = "/tmp/dpaste-cache-XXXXXX";
        fd_ = mkstemp(path);
        if (fd_ == -1)
            throw std::runtime_error("could not create temporary cache file");
        path_ = path;

        if (setenv("DPASTE_CACHE_DIR", path_.c_str(), 1) != 0) {
            close(fd_);
            unlink(path_.c_str());
            throw std::runtime_error("could not set DPASTE_CACHE_DIR");
        }
    }

    ~CacheFileGuard() {
        if (had_previous_value_)
            setenv("DPASTE_CACHE_DIR", previous_value_.c_str(), 1);
        else
            unsetenv("DPASTE_CACHE_DIR");
        close(fd_);
        unlink(path_.c_str());
    }
};

TEST_CASE("Node starts even when the cache path is unusable", "[Node][cache]") {
    PirateNodeTester pt;
    CacheFileGuard cache;

    dpaste::Node node {};
    node.run();
    REQUIRE(pt.is_running(node));

    node.stop();
    REQUIRE_FALSE(pt.is_running(node));
}

TEST_CASE("Node get/paste on DHT", "[Node][get][paste]") {
    PirateNodeTester pt;

    const std::string PIN = random_pin();
    std::vector<uint8_t> data = {0, 1, 2, 3, 4};
    dpaste::Node node {};
    node.run();

    SECTION ( "pasting data {0,1,2,3,4}" ) {
        REQUIRE ( node.paste(PIN, std::vector<uint8_t> {data}) );

        SECTION ( "getting pasted blob back from the DHT" ) {
            auto rd = node.get(PIN);
            REQUIRE ( data == rd.front() );
        }
    }

    node.stop();
    REQUIRE ( not pt.is_running(node) );
}

} /* tests */
} /* dpaste */

/* vim: set ts=4 sw=4 tw=120 et :*/

