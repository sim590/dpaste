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

#include "conf.h"

namespace dpaste {
namespace conf {

void trim_str(std::string& str) {
    auto first = std::min(str.size(), str.find_first_not_of(" "));
    auto last = std::min(str.size(), str.find_last_not_of(" "));
    str = str.substr(first, last - first + 1);
}

int ConfigurationFile::load() {
    std::ifstream config_file(filePath_, std::ios::in);
    if (not config_file.is_open())
        return 1;

    std::string line;
    while (std::getline(config_file, line)) {
        std::string arg_name;
        std::istringstream ss(line);
        while (std::getline(ss, arg_name, '=')) {
            std::string arg;
            trim_str(arg_name);
            auto ait = config_.find(arg_name);
            if (ait != config_.end()) {
                std::getline(ss, arg);
                trim_str(arg);
                if (not arg.empty())
                    ait->second = arg;
            }
        }
    }
    return 0;
}

} /* conf  */
} /* dpaste */

/* vim: set ts=4 sw=4 tw=120 et :*/

