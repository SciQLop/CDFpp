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
#include "cdf-enums.hpp"
#include <algorithm>
#include <cstdint>
#include <variant>
#include <vector>

namespace cdf
{

struct cdf_none
{
};

using cdf_values_t = std::variant<cdf_none, std::vector<char>, std::vector<uint8_t>,
    std::vector<uint16_t>, std::vector<uint32_t>, std::vector<int8_t>, std::vector<int16_t>,
    std::vector<int32_t>, std::vector<int64_t>, std::vector<float>, std::vector<double>,
    std::vector<tt2000_t>, std::vector<epoch>>;


template <CDF_Types type, typename endianness_t>
auto load_values(const char* buffer, std::size_t buffer_size)
{
    std::size_t size = buffer_size / sizeof(from_cdf_type_t<type>);
    std::vector<from_cdf_type_t<type>> result(size);
    endianness::decode_v<endianness_t>(buffer, buffer_size, result.data());
    return cdf_values_t { std::move(result) };
}

cdf_values_t load_values(
    const char* buffer, std::size_t buffer_size, CDF_Types type, cdf_encoding encoding)
{
    switch (type)
    {
        case CDF_Types::CDF_FLOAT:
            return load_values<CDF_Types::CDF_FLOAT, endianness::little_endian_t>(
                buffer, buffer_size);
        case CDF_Types::CDF_CHAR:
            return load_values<CDF_Types::CDF_CHAR, endianness::little_endian_t>(
                buffer, buffer_size);
        case CDF_Types::CDF_INT1:
            return load_values<CDF_Types::CDF_INT1, endianness::little_endian_t>(
                buffer, buffer_size);
        case CDF_Types::CDF_INT2:
            return load_values<CDF_Types::CDF_INT2, endianness::little_endian_t>(
                buffer, buffer_size);
        case CDF_Types::CDF_INT4:
            return load_values<CDF_Types::CDF_INT4, endianness::little_endian_t>(
                buffer, buffer_size);
        case CDF_Types::CDF_INT8:
            return load_values<CDF_Types::CDF_INT8, endianness::little_endian_t>(
                buffer, buffer_size);
        case CDF_Types::CDF_UINT1:
            return load_values<CDF_Types::CDF_UINT1, endianness::little_endian_t>(
                buffer, buffer_size);
        case CDF_Types::CDF_UINT2:
            return load_values<CDF_Types::CDF_UINT2, endianness::little_endian_t>(
                buffer, buffer_size);
        case CDF_Types::CDF_UINT4:
            return load_values<CDF_Types::CDF_UINT4, endianness::little_endian_t>(
                buffer, buffer_size);
        case CDF_Types::CDF_BYTE:
            return load_values<CDF_Types::CDF_BYTE, endianness::little_endian_t>(
                buffer, buffer_size);
    }
    return {};
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

    data_t() : p_values {}, p_type { CDF_Types::CDF_NONE } {}

    template <typename T>
    data_t(const T& values) : p_values { values }, p_type { to_cdf_type<typename T::value_type>() }
    {
    }

    data_t(const cdf_values_t& values) : p_values { values } {}
    data_t(cdf_values_t&& values) { this->p_values = std::move(values); }


private:
    cdf_values_t p_values;
    CDF_Types p_type;
};
}
