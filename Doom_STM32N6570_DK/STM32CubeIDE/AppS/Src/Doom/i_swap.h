//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Endianess handling, swapping 16bit and 32bit.
//


#ifndef __I_SWAP__
#define __I_SWAP__

// Endianess handling.
// WAD files are stored little endian.

// These are deliberately cast to signed values; this is the behaviour
// of the macros in the original source and some code relies on it.

#include <stdint.h>

// Always interpret input as little-endian, convert to host-endian, then cast to signed
static inline int16_t SHORT(uint16_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define SYS_LITTLE_ENDIAN
    return (int16_t)x;
#else
    #define SYS_BIG_ENDIAN
    return (int16_t)((x << 8) | (x >> 8));
#endif
}

static inline int32_t LONG(uint32_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (int32_t)x;
#else
    return (int32_t)(
        ((x & 0x000000FFU) << 24) |
        ((x & 0x0000FF00U) << 8)  |
        ((x & 0x00FF0000U) >> 8)  |
        ((x & 0xFF000000U) >> 24)
    );
#endif
}

#endif

