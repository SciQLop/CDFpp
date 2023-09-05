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

#ifdef _MSC_VER
#include <stdlib.h>
#define bswap16(x) _byteswap_ushort(x)
#define bswap32(x) _byteswap_ulong(x)
#define bswap64(x) _byteswap_uint64(x)
#endif

#include "../cdf-debug.hpp"
#include "../cdf-enums.hpp"
#include <algorithm>
#include <stdint.h>
#include <cstring>
#include <type_traits>

#include <iostream>

namespace cdf::endianness
{

[[nodiscard]] constexpr bool is_big_endian_encoding(cdf_encoding encoding)
{
    return encoding == cdf_encoding::network || encoding == cdf_encoding::SUN
        || encoding == cdf_encoding::NeXT || encoding == cdf_encoding::PPC
        || encoding == cdf_encoding::SGi || encoding == cdf_encoding::IBMRS
        || encoding == cdf_encoding::ARM_BIG;
}

[[nodiscard]] bool is_little_endian_encoding(cdf_encoding encoding)
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

    template <typename T>
    union swapable_t
    {
        static inline constexpr bool is_int = std::is_same_v<T, uint_t<sizeof(T)>>;
        uint_t<sizeof(T)> int_view;
        T value;
        template <bool disable = is_int>
        swapable_t(T v, typename std::enable_if<!disable>::type* = 0) : value { v }
        {
        }
        swapable_t(uint_t<sizeof(T)> v) : int_view { v } { }
    };


    [[nodiscard]] inline uint8_t bswap(uint8_t v) noexcept
    {
        return v;
    }
    [[nodiscard]] inline uint16_t bswap(uint16_t v) noexcept
    {
        return bswap16(v);
    }
    [[nodiscard]] inline uint32_t bswap(uint32_t v) noexcept
    {
        return bswap32(v);
    }
    [[nodiscard]] inline uint64_t bswap(uint64_t v) noexcept
    {
        return bswap64(v);
    }

    template <typename T, std::size_t s = sizeof(T)>
    [[nodiscard]] inline T byte_swap(T value) noexcept
    {
        return swapable_t<T> { bswap(swapable_t<T> { value }.int_view) }.value;
    }
}


template <typename src_endianess_t, typename T, typename U>
[[nodiscard]] CDFPP_NON_NULL(1)
inline T decode(const U* input)
{
    if constexpr (sizeof(T) > 1)
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
    else
    {
        return static_cast<T>(*input);
    }
}

template <typename src_endianess_t, typename value_t>
CDFPP_NON_NULL(1)
inline void _impl_decode_v(value_t* data, std::size_t size)
{
    if constexpr (sizeof(value_t) > 1 and not std::is_same_v<host_endianness_t, src_endianess_t>)
    {
        CDFPP_ASSERT(data != nullptr);
        if (size > 0)
        {
            for (auto i = 0UL; i < size; i++)
            {
                data[i] = byte_swap(data[i]);
            }
        }
    }
}


template <typename src_endianess_t, typename value_t>
CDFPP_NON_NULL(1)
    inline void decode_v(value_t* data, std::size_t size)
{
    _impl_decode_v<src_endianess_t>(reinterpret_cast<uint_t<sizeof(value_t)>*>(data),size);
}

template <typename src_endianess_t>
CDFPP_NON_NULL(1)
inline void decode_v(epoch16* data, std::size_t size)
{
    decode_v<src_endianess_t>(reinterpret_cast<uint64_t*>(data), size * 2);
}
}
