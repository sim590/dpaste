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
 * along with dpaste. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <array>
#include <iostream>
#include <msgpack.hpp>

#include <opendht/value.h>
#include <opendht/infohash.h>

namespace dpaste {
namespace ipc {

const constexpr size_t MAX_PACKET_SIZE {dht::MAX_VALUE_SIZE + 1024};

enum MessageType {
    Error = 0,
    Reply,
    Paste,
    Get,
};

struct ParsedMessage {
    MessageType type;
    std::string code;
    dht::Blob paste_value;

    void msgpack_unpack(msgpack::object msg);
};

} /* ipc */
} /* dpaste */

