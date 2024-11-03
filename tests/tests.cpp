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

#include <random>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

namespace dpaste {
namespace tests {

static std::uniform_int_distribution<uint32_t> dist;
static std::mt19937_64 rand_;

void init() {
    static bool initialized = false;
    if (initialized)
        return;

    rand_.seed(Catch::rngSeed());

    initialized = true;
}

int random_number() {
    init();
    return dist(rand_);
}

std::string random_pin() {
    const auto i = random_number();
    std::stringstream ss;
    ss << std::hex << i;
    return ss.str();
}

} /* tests */
} /* dpaste */

/* vim: set ts=4 sw=4 tw=120 et :*/

