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

#include "conf.h"

namespace dpaste {
namespace tests {

const std::string CONFIG_FILE_PATH_PREFIX = "./tests/misc/dpaste.conf";

TEST_CASE("ConfigurationFile loading/parsing (./misc/dpaste.conf)", "[ConfigurationFile][get][load]") {
    SECTION ( "Well-formed file" ) {
        conf::ConfigurationFile f(CONFIG_FILE_PATH_PREFIX+".1");
        REQUIRE ( f.load() == 0 );

        auto c = f.getConfiguration();
        REQUIRE ( c.at("host") ==  "192.168.1.100" );
        REQUIRE ( c.at("port") ==  "6500" );
    }
    SECTION ( "Malformed file (bad host)" ) {
        conf::ConfigurationFile f(CONFIG_FILE_PATH_PREFIX+".2");
        REQUIRE ( f.load() == 0 );

        auto c = f.getConfiguration();
        REQUIRE ( c.at("host") ==  "127.0.0.1" ); /* the default         */
        REQUIRE ( c.at("port") ==  "6500" );      /* recovered from file */
    }
    SECTION ( "File not found" ) {
        conf::ConfigurationFile f("/dpaste.conf"); /* This should not work on
                                                      any computer owned by a
                                                      sane person */
        REQUIRE ( f.load() == 1 );
    }
}

} /* tests */
} /* dpaste */

/* vim: set ts=4 sw=4 tw=120 et :*/

