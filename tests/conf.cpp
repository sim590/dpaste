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

#include "conf.h"

#include "catch.h"

namespace dpaste {
namespace tests {

const std::string CONFIG_FILE_PATH_PREFIX = "./tests/misc/dpaste.conf";

CATCH_TEST_CASE("ConfigurationFile loading/parsing (./misc/dpaste.conf)", "[ConfigurationFile][get][load]") {
    CATCH_SECTION ( "Well-formed file" ) {
        conf::ConfigurationFile f(CONFIG_FILE_PATH_PREFIX+".1");
        CATCH_REQUIRE ( f.load() == 0 );

        auto c = f.getConfiguration();
        CATCH_REQUIRE ( c.at("host") ==  "192.168.1.100" );
        CATCH_REQUIRE ( c.at("port") ==  "6500" );
    }
    CATCH_SECTION ( "Malformed file (bad host)" ) {
        conf::ConfigurationFile f(CONFIG_FILE_PATH_PREFIX+".2");
        CATCH_REQUIRE ( f.load() == 0 );

        auto c = f.getConfiguration();
        CATCH_REQUIRE ( c.at("host") ==  "127.0.0.1" ); /* the default         */
        CATCH_REQUIRE ( c.at("port") ==  "6500" );      /* recovered from file */
    }
    CATCH_SECTION ( "File not found" ) {
        conf::ConfigurationFile f("/dpaste.conf"); /* This should not work on
                                                      any computer owned by a
                                                      sane person */
        CATCH_REQUIRE ( f.load() == 1 );
    }
}

} /* tests */
} /* dpaste */

/* vim: set ts=4 sw=4 tw=120 et :*/

