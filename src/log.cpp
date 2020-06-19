/*
 * Copyright © 2017-2020 Simon Désaulniers
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

#include <array>
#include <iostream>
#include <stdio.h>
#include <stdarg.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "log.h"

static constexpr const char* DPASTE_MSG_PREFIX = "DPASTE: ";

void print_log(char const *m, va_list args) {
    std::array<char, 8192> buffer;
    int ret = vsnprintf(buffer.data(), buffer.size(), m, args);
    if (ret < 0)
        return;

    std::cerr << DPASTE_MSG_PREFIX;

    // write log
    std::cerr.write(buffer.data(), std::min((size_t) ret, buffer.size()));
    if ((size_t) ret >= buffer.size())
        std::cerr << "[[TRUNCATED]]";
    std::cerr << std::endl;
}

void DPASTE_MSG(char const* format, ...) {
#ifndef DPASTE_TEST
    va_list args;
    va_start(args, format);
    print_log(format, args);
    va_end(args);
#endif
}

/* vim:set et sw=4 ts=4 tw=120: */

