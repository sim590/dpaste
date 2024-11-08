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

