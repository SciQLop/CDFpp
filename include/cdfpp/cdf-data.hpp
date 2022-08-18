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
#include "cdf-debug.hpp"
#include "cdf-endianness.hpp"
#include "cdf-enums.hpp"
#include "cdf-helpers.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace cdf
{

struct cdf_none
{
    bool operator==([[maybe_unused]] const cdf_none& other) const { return true; }
};

using cdf_values_t = std::variant<cdf_none, std::vector<char>, std::vector<uint8_t>,
    std::vector<uint16_t>, std::vector<uint32_t>, std::vector<int8_t>, std::vector<int16_t>,
    std::vector<int32_t>, std::vector<int64_t>, std::vector<float>, std::vector<double>,
    std::vector<tt2000_t>, std::vector<epoch>, std::vector<epoch16>>;

struct data_t
{

    template <CDF_Types type>
    decltype(auto) get();

    template <CDF_Types type>
    decltype(auto) get() const;

    template <typename type>
    decltype(auto) get();

    template <typename type>
    decltype(auto) get() const;

    CDF_Types type() const { return p_type; }

    cdf_values_t& values() { return p_values; }

    data_t& operator=(data_t&& other);
    data_t& operator=(const data_t& other);

    data_t() : p_values { cdf_none {} }, p_type { CDF_Types::CDF_NONE } { }
    data_t(const data_t& other) = default;
    data_t(data_t&& other) = default;


    template <typename T, typename Dummy = void,
        typename = std::enable_if_t<!std::is_same_v<std::remove_reference_t<T>, data_t>, Dummy>>
    data_t(T&& values)
            : p_values { std::forward<T>(values) }
            , p_type { to_cdf_type<typename std::remove_reference_t<T>::value_type>() }
    {
    }

    data_t(const cdf_values_t& values, CDF_Types type) : p_values { values }, p_type { type } { }
    data_t(cdf_values_t&& values, CDF_Types type) : p_values { std::move(values) }, p_type { type }
    {
    }

    template <typename... Ts>
    friend auto visit(data_t& data, Ts... lambdas);
    template <typename... Ts>
    friend auto visit(const data_t& data, Ts... lambdas);

    template <typename T, typename type>
    friend decltype(auto) _get_impl(T* self);

private:
    cdf_values_t p_values;
    CDF_Types p_type;
};

template <typename... Ts>
auto visit(data_t& data, Ts... lambdas)
{
    return std::visit(helpers::make_visitor(lambdas...), data.p_values);
}

template <typename... Ts>
auto visit(const data_t& data, Ts... lambdas)
{
    return std::visit(helpers::make_visitor(lambdas...), data.p_values);
}

template <CDF_Types type, typename endianness_t>
auto load_values(const char* buffer, std::size_t buffer_size);

data_t load_values(
    const char* buffer, std::size_t buffer_size, CDF_Types type, cdf_encoding encoding);


/*=================================================================================
 Implementation
===================================================================================*/


template <CDF_Types type>
inline decltype(auto) data_t::get()
{
    return std::get<std::vector<from_cdf_type_t<type>>>(this->p_values);
}

template <CDF_Types type>
inline decltype(auto) data_t::get() const
{
    return std::get<std::vector<from_cdf_type_t<type>>>(this->p_values);
}


template <typename T, typename type>
inline decltype(auto) _get_impl(T* self)
{
    return std::get<std::vector<type>>(self->p_values);
}

template <typename type>
inline decltype(auto) data_t::get()
{
    return _get_impl<data_t, type>(this);
}

template <typename type>
inline decltype(auto) data_t::get() const
{
    return _get_impl<const data_t, type>(this);
}


inline data_t& data_t::operator=(data_t&& other)
{
    std::swap(this->p_values, other.p_values);
    std::swap(this->p_type, other.p_type);
    return *this;
}
inline data_t& data_t::operator=(const data_t& other)
{
    this->p_values = other.p_values;
    this->p_type = other.p_type;
    return *this;
}

// https://stackoverflow.com/questions/4059775/convert-iso-8859-1-strings-to-utf-8-in-c-c
template <typename T>
std::vector<T> iso_8859_1_to_utf8(const char* buffer, std::size_t buffer_size)
{
    std::vector<T> out;
    out.reserve(buffer_size);
    std::for_each(buffer, buffer + buffer_size,
        [&out](const uint8_t c)
        {
            if (c < 0x80)
            {
                out.push_back(c);
            }
            else
            {
                out.push_back(0xc0 | c >> 6);
                out.push_back(0x80 | (c & 0x3f));
            }
        });
    return out;
}


template <CDF_Types type, typename endianness_t, bool latin1_to_utf8_conv>
inline auto load_values(const char* buffer, std::size_t buffer_size)
{

    CDFPP_ASSERT(not(buffer == nullptr and buffer_size != 0UL));

    if constexpr (type == CDF_Types::CDF_CHAR
        || type == CDF_Types::CDF_UCHAR) // special case for strings
    {
        if constexpr (latin1_to_utf8_conv)
        {
            return iso_8859_1_to_utf8<from_cdf_type_t<type>>(buffer, buffer_size);
        }
        else
        {
            std::vector<from_cdf_type_t<type>> result(buffer_size, '\0');
            std::transform(buffer, buffer + buffer_size, std::begin(result),
                [](const auto c) { return static_cast<from_cdf_type_t<type>>(c); });
            return result;
        }
    }
    else
    {
        using raw_type = from_cdf_type_t<type>;
        std::size_t size = buffer_size / sizeof(raw_type);
        std::vector<raw_type> result(size);
        if (size != 0UL)
            endianness::decode_v<endianness_t>(buffer, buffer_size, result.data());
        return cdf_values_t { std::move(result) };
    }
}

template <bool iso_8859_1_to_utf8>
inline data_t load_values(
    const char* buffer, std::size_t buffer_size, CDF_Types type, cdf_encoding encoding)
{
    CDFPP_ASSERT(not(buffer == nullptr and buffer_size != 0UL));
#define DATA_FROM_T(type)                                                                          \
    case CDF_Types::type:                                                                          \
        if (endianness::is_big_endian_encoding(encoding))                                          \
            return data_t {                                                                        \
                load_values<CDF_Types::type, endianness::big_endian_t, iso_8859_1_to_utf8>(        \
                    buffer, buffer_size),                                                          \
                CDF_Types::type                                                                    \
            };                                                                                     \
        return data_t {                                                                            \
            load_values<CDF_Types::type, endianness::little_endian_t, iso_8859_1_to_utf8>(         \
                buffer, buffer_size),                                                              \
            CDF_Types::type                                                                        \
        };

    switch (type)
    {
        DATA_FROM_T(CDF_FLOAT)
        DATA_FROM_T(CDF_DOUBLE)
        DATA_FROM_T(CDF_REAL4)
        DATA_FROM_T(CDF_REAL8)
        DATA_FROM_T(CDF_EPOCH)
        DATA_FROM_T(CDF_EPOCH16)
        DATA_FROM_T(CDF_TIME_TT2000)
        DATA_FROM_T(CDF_CHAR)
        DATA_FROM_T(CDF_UCHAR)
        DATA_FROM_T(CDF_INT1)
        DATA_FROM_T(CDF_INT2)
        DATA_FROM_T(CDF_INT4)
        DATA_FROM_T(CDF_INT8)
        DATA_FROM_T(CDF_UINT1)
        DATA_FROM_T(CDF_UINT2)
        DATA_FROM_T(CDF_UINT4)
        DATA_FROM_T(CDF_BYTE)
        case CDF_Types::CDF_NONE:
            return {};
    }
    return {};
}

}
