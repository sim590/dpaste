/*
 * Copyright © 2020 Simon Désaulniers
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

#include <cstdint>

namespace dpaste {
namespace code {

/**
 * Number of bytes in a dpaste code accounting for the number of packets the
 * file was split into
 */
static const constexpr unsigned int DPASTE_NPACKETS_LEN {2};
/**
 * Number of bytes in a dpaste code accounting for the location PIN. */
static const constexpr unsigned int DPASTE_PIN_LEN {8};

/**
 * Number of bytes used to encode the AES password.
 */
static const constexpr size_t PASSWORD_LEN {8};

/**
 * Length of the PIN if it contains the password for encrypted pasted data.
 * It consists of 18 hexadecimal characters: 18*4 = 72 bits. Last 32 bits
 * encode the password. Otherwise, PIN should be 40 bits.
 */
static const constexpr size_t PIN_WITH_PASS_LEN {DPASTE_PIN_LEN + DPASTE_NPACKETS_LEN + PASSWORD_LEN};

}
} /* dpaste */

/* vim: set sts=4 ts=4 sw=4 tw=120 et :*/

