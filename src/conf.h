/*
 * Copyright © 2016 Simon Désaulniers
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
#include <fstream>
#include <sstream>
#include <map>
#include <iostream>

#include <glibmm.h>

namespace dpaste {
namespace conf {

static const constexpr char* CONFIG_FILE_NAME = "dpaste.conf";

class ConfigurationFile final {
public:
    ConfigurationFile(std::string file_path="") : filePath_(file_path),
        config_({{"host", "127.0.0.1"},
                {"port", "6509"}})
    {
        if (file_path.empty()) {
            const auto default_file_path = Glib::get_user_config_dir() + '/' + CONFIG_FILE_NAME;
            filePath_ = default_file_path;
        }
    }
    ~ConfigurationFile() {}

    void load();
    const std::map<std::string, std::string>& getConfiguration() { return config_; }

private:
    std::map<std::string, std::string> config_;
    std::string filePath_;
};

} /* conf  */
} /* dpaste */

