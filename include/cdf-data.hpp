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
#include "cdf-endianness.hpp"
#include <algorithm>
#include <cstdint>
#include <variant>
#include <vector>

namespace cdf
{

struct tt2000_t
{
    int64_t value;
};

struct epoch
{
    double value;
};

enum class CDF_Types : uint32_t
{
    CDF_INT1 = 1,
    CDF_INT2 = 2,
    CDF_INT4 = 4,
    CDF_INT8 = 8,
    CDF_UINT1 = 11,
    CDF_UINT2 = 12,
    CDF_UINT4 = 14,
    CDF_BYTE = 41,
    CDF_REAL4 = 21,
    CDF_REAL8 = 22,
    CDF_FLOAT = 44,
    CDF_DOUBLE = 45,
    CDF_EPOCH = 31,
    CDF_EPOCH16 = 32,
    CDF_TIME_TT2000 = 33,
    CDF_CHAR = 51,
    CDF_UCHAR = 52
};

template <CDF_Types type>
constexpr auto from_cdf_type()
{
    if constexpr (type == CDF_Types::CDF_CHAR)
        return char {};
    if constexpr (type == CDF_Types::CDF_INT1)
        return int8_t {};
    if constexpr (type == CDF_Types::CDF_INT2)
        return int16_t {};
    if constexpr (type == CDF_Types::CDF_INT4)
        return int32_t {};
    if constexpr (type == CDF_Types::CDF_INT8)
        return int64_t {};
    if constexpr (type == CDF_Types::CDF_UINT1)
        return uint8_t {};
    if constexpr (type == CDF_Types::CDF_UINT2)
        return uint16_t {};
    if constexpr (type == CDF_Types::CDF_UINT4)
        return uint32_t {};
    if constexpr (type == CDF_Types::CDF_FLOAT)
        return float {};
    if constexpr (type == CDF_Types::CDF_DOUBLE)
        return double {};
}

template <typename type>
constexpr CDF_Types to_cdf_type()
{
    if constexpr (std::is_same_v<type, float>)
        return CDF_Types::CDF_FLOAT;
    if constexpr (std::is_same_v<type, double>)
        return CDF_Types::CDF_DOUBLE;
    if constexpr (std::is_same_v<type, uint8_t>)
        return CDF_Types::CDF_UINT1;
    if constexpr (std::is_same_v<type, uint16_t>)
        return CDF_Types::CDF_UINT2;
    if constexpr (std::is_same_v<type, uint32_t>)
        return CDF_Types::CDF_UINT4;
    if constexpr (std::is_same_v<type, int8_t>)
        return CDF_Types::CDF_INT1;
    if constexpr (std::is_same_v<type, int16_t>)
        return CDF_Types::CDF_INT2;
    if constexpr (std::is_same_v<type, int32_t>)
        return CDF_Types::CDF_INT4;
    if constexpr (std::is_same_v<type, int64_t>)
        return CDF_Types::CDF_INT8;
    if constexpr (std::is_same_v<type, epoch>)
        return CDF_Types::CDF_EPOCH;
    if constexpr (std::is_same_v<type, tt2000_t>)
        return CDF_Types::CDF_TIME_TT2000;
}


template <CDF_Types type>
using from_cdf_type_t = decltype(to_cdf_type<type>());

template <CDF_Types type, typename endianness_t>
auto load(const char* buffer, std::size_t buffer_size)
{
    constexpr std::size_t size = buffer_size / sizeof(from_cdf_type_t<type>);
    std::vector<from_cdf_type_t<type>> result { size };
    endianness::decode_v<endianness_t>(buffer, buffer_size, std::begin(result));
    return result;
}

struct data_t
{

    template <CDF_Types type>
    decltype(auto) get()
    {
        return std::get<std::vector<from_cdf_type_t<type>>>(p_values);
    }

    template <CDF_Types type>
    decltype(auto) get() const
    {
        return std::get<std::vector<from_cdf_type_t<type>>>(p_values);
    }

    template <typename T>
    data_t(const T& values) : p_values { values }, p_type { to_cdf_type<typename T::value_type>() }
    {
    }

private:
    std::variant<std::vector<uint8_t>, std::vector<uint16_t>, std::vector<uint32_t>,
        std::vector<int8_t>, std::vector<int16_t>, std::vector<int32_t>, std::vector<int64_t>,
        std::vector<float>, std::vector<double>, std::vector<tt2000_t>, std::vector<epoch>>
        p_values;
    CDF_Types p_type;
};
}
