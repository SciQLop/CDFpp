#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2019, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/

#ifdef CDFpp_BIG_ENDIAN
inline const bool is_big_endian = true;
inline const bool is_little_endian = false;
#else
#ifdef CDFpp_LITTLE_ENDIAN
inline const bool is_big_endian = false;
inline const bool is_little_endian = true;
#else
#error "Can't find if platform is either big or little endian"
#endif
#endif

#ifdef __GNUC__
#define bswap16 __builtin_bswap16
#define bswap32 __builtin_bswap32
#define bswap64 __builtin_bswap64
#endif

#include <cstdint>

namespace cdf::endianness
{

namespace
{
    template <typename T>
    T byte_swap(T value)
    {
        if constexpr (sizeof(T) == 1)
            return value;
        if constexpr (sizeof(T) == 2)
        {
            int16_t result = bswap16(*reinterpret_cast<int16_t*>(&value));
            return *reinterpret_cast<T*>(&result);
        }

        if constexpr (sizeof(T) == 4)
        {
            int32_t result = bswap32(*reinterpret_cast<int32_t*>(&value));
            return *reinterpret_cast<T*>(&result);
        }

        if constexpr (sizeof(T) == 8)
        {
            int64_t result = bswap64(*reinterpret_cast<int64_t*>(&value));
            return *reinterpret_cast<T*>(&result);
        }
    }
}

template <typename T>
T read(const char* input)
{
    if constexpr (is_little_endian)
    {
        return byte_swap<T>(*reinterpret_cast<const T*>(input));
    }
    return *reinterpret_cast<const T*>(input);
}
}
