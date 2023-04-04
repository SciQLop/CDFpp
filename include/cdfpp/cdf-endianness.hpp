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
#include "cdfpp_config.h"
#ifdef CDFpp_BIG_ENDIAN
inline const bool host_is_big_endian = true;
inline const bool host_is_little_endian = false;
#else
#ifdef CDFpp_LITTLE_ENDIAN
inline const bool host_is_big_endian = false;
inline const bool host_is_little_endian = true;
#else
#error "Can't find if platform is either big or little endian"
#endif
#endif

#ifdef __GNUC__
#define bswap16 __builtin_bswap16
#define bswap32 __builtin_bswap32
#define bswap64 __builtin_bswap64
#endif

#include "cdf-debug.hpp"
#include "cdf-enums.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <iostream>

namespace cdf::endianness
{

bool is_big_endian_encoding(cdf_encoding encoding)
{
    return encoding == cdf_encoding::network || encoding == cdf_encoding::SUN
        || encoding == cdf_encoding::NeXT || encoding == cdf_encoding::PPC
        || encoding == cdf_encoding::SGi || encoding == cdf_encoding::IBMRS
        || encoding == cdf_encoding::ARM_BIG;
}
bool is_little_endian_encoding(cdf_encoding encoding)
{
    return !is_big_endian_encoding(encoding);
}

struct big_endian_t
{
};
struct little_endian_t
{
};

using host_endianness_t = std::conditional_t<host_is_little_endian, little_endian_t, big_endian_t>;

template <typename endianness_t>
inline constexpr bool is_little_endian_v = std::is_same_v<little_endian_t, endianness_t>;

template <typename endianness_t>
inline constexpr bool is_big_endian_v = std::is_same_v<big_endian_t, endianness_t>;

namespace
{
    template <std::size_t s>
    struct uint;

    template <>
    struct uint<1>
    {
        using type = uint8_t;
    };
    template <>
    struct uint<2>
    {
        using type = uint16_t;
    };
    template <>
    struct uint<4>
    {
        using type = uint32_t;
    };
    template <>
    struct uint<8>
    {
        using type = uint64_t;
    };

    template <std::size_t s>
    using uint_t = typename uint<s>::type;


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


template <typename src_endianess_t, typename T, typename U>
CDFPP_NON_NULL(1)
T decode(const U* input)
{
    T result;
    CDFPP_ASSERT(input != nullptr);
    std::memcpy(&result, input, sizeof(T));
    if constexpr (!std::is_same_v<host_endianness_t, src_endianess_t>)
    {
        return byte_swap<T>(result);
    }
    return result;
}

template <typename src_endianess_t, typename value_t>
CDFPP_NON_NULL(1, 3)
inline void decode_v(const char* input, std::size_t size, value_t* output)
{
    using casted_buffer_t = uint_t<sizeof(value_t)>;
    CDFPP_ASSERT(input != nullptr);
    CDFPP_ASSERT(output != nullptr);
    std::memcpy(output, input, size);
    if constexpr (not std::is_same_v<host_endianness_t, src_endianess_t>)
    {
        std::size_t buffer_size = size / sizeof(value_t);
        casted_buffer_t* casted_buffer = reinterpret_cast<casted_buffer_t*>(output);
        std::transform(casted_buffer, casted_buffer + buffer_size, casted_buffer,
            [](const auto& v) { return byte_swap(v); });
    }
}

template <typename src_endianess_t>
CDFPP_NON_NULL(1, 3)
inline void decode_v(const char* input, std::size_t size, epoch16* output)
{
    decode_v<src_endianess_t>(input, size, reinterpret_cast<double*>(output));
}

}
