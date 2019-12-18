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
#include <type_traits>

namespace cdf::endianness
{

struct big_endian_t
{
};
struct little_endian_t
{
};

template <typename endianness_t>
inline constexpr bool is_little_endian_v = std::is_same_v<little_endian_t, endianness_t>;

template <typename endianness_t>
inline constexpr bool is_big_endian_v = std::is_same_v<big_endian_t, endianness_t>;

namespace
{

    template <std::size_t s>
    auto uint()
    {
        if constexpr (s == 1)
            return uint8_t {};
        if constexpr (s == 2)
            return uint16_t {};
        if constexpr (s == 4)
            return uint32_t {};
        if constexpr (s == 8)
            return uint64_t {};
    }

    template <std::size_t s>
    using uint_t = decltype(uint<s>());

    inline uint8_t bswap(uint8_t v) { return v; }
    inline uint16_t bswap(uint16_t v) { return bswap16(v); }
    inline uint32_t bswap(uint32_t v) { return bswap32(v); }
    inline uint64_t bswap(uint64_t v) { return bswap64(v); }

    template <typename T, std::size_t s = sizeof(T)>
    T byte_swap(T value)
    {
        using int_repr_t = uint_t<s>;
        int_repr_t result = bswap(*reinterpret_cast<int_repr_t*>(&value));
        return *reinterpret_cast<T*>(&result);
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

template <typename src_endianess_t, typename iterator_t>
void read_v(const char* input, iterator_t& begin, iterator_t& end)
{
    using value_t = decltype(*begin);
    auto casted_buffer = reinterpret_cast<uint_t<sizeof(value_t)>*>(input);
    // std::transform(casted_buffer, casted_buffer + size, result, [](const auto& v) { return v; });
}

}
