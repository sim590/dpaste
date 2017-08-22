/*
 * Copyright © 2017 Simon Désaulniers
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

#include <string>

namespace dpaste {

class HttpClient {
public:
    HttpClient (std::string host, long port) : host(host), port(port) {}
    virtual ~HttpClient () {}

    std::string get(const std::string& code) const;
    bool put(const std::string& code, const std::string& data) const;

private:
    static const constexpr char* HTTP_PROTO = "http://";

    std::string host; /* host for the http dht service */
    long port;        /* port for the http dht service */
};

} /* dpaste */
