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

#include <istream>
#include <string>
#include <memory>
#include <map>

#include "http_client.h"
#include "node.h"

namespace dpaste {

class Bin {
public:
    Bin (std::string&& code);
    virtual ~Bin () {}

    /**
     * Execute the program main functionnality.
     *
     * @return return code (0: success, 1 fail)
     */
    int execute() {
        if (not code_.empty())
            return get();
        else
            return paste();
    }

private:
    static const constexpr char* DPASTE_CODE_PREFIX = "dpaste:";

    int get();
    int paste();

    std::string code_ {};
    std::string buffer_  {};
    std::unique_ptr<HttpClient> http_client_ {};
    std::map<std::string, std::string> conf;
    Node node {};
};

} /* dpaste */

