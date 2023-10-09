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
#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf-map.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
#include <chrono>
#include <ctime>
#include <fmt/core.h>
#include <iomanip>
#include <sstream>
#include <string>
using namespace cdf;

template <class stream_t>
stream_t& operator<<(stream_t& os, const cdf::data_t& data);

template <typename collection_t>
constexpr bool is_char_like_v
    = std::is_same_v<int8_t,
          std::remove_cv_t<
              std::remove_reference_t<decltype(*std::cbegin(std::declval<collection_t>()))>>>
    or std::is_same_v<uint8_t,
        std::remove_cv_t<
            std::remove_reference_t<decltype(*std::cbegin(std::declval<collection_t>()))>>>;


template <class stream_t>
inline stream_t& operator<<(stream_t& os, const decltype(cdf::to_time_point(tt2000_t {}))& tp)
{
    const auto tp_time_t = std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::time_point::duration>(tp));
    auto tt = std::gmtime(&tp_time_t);
    if (tt)
    {
        const uint64_t ns = (std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch())
            % 1000000000)
                            .count();
        // clang-format off
        os << std::setprecision(4) << std::setw(4) << std::setfill('0')  << tt->tm_year + 1900 << '-'
           << std::setprecision(2) << std::setw(2) << std::setfill('0')  << tt->tm_mon + 1     << '-'
           << std::setprecision(2) << std::setw(2) << std::setfill('0')  << tt->tm_mday        << 'T'
           << std::setprecision(2) << std::setw(2) << std::setfill('0')  << tt->tm_hour        << ':'
           << std::setprecision(2) << std::setw(2) << std::setfill('0')  << tt->tm_min         << ':'
           << std::setprecision(2) << std::setw(2) << std::setfill('0')  << tt->tm_sec         << '.'
           << std::setprecision(9) << std::setw(9) << std::setfill('0')  << ns;
        // clang-format on
    }

    return os;
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const epoch& time)
{
    os << cdf::to_time_point(time);
    return os;
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const epoch16& time)
{
    os << cdf::to_time_point(time);
    return os;
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const tt2000_t& time)
{
    if (time.value == static_cast<int64_t>(0x8000000000000000))
    {
        os << "9999-12-31T23:59:59.999999999";
        return os;
    }
    if (time.value == static_cast<int64_t>(0x8000000000000001))
    {
        os << "0000-01-01T00:00:00.000000000";
        return os;
    }
    if (time.value == static_cast<int64_t>(0x8000000000000003))
    {
        os << "9999-12-31T23:59:59.999999999";
        return os;
    }
    os << cdf::to_time_point(time);
    return os;
}

template <class stream_t, class input_t, class item_t>
inline auto stream_collection(stream_t& os, const input_t& input, const item_t& sep)
    -> decltype(input.back(), os)
{
    os << "[ ";
    if (std::size(input))
    {
        if (std::size(input) > 1)
        {
            if constexpr (is_char_like_v<input_t>)
            {
                std::for_each(std::cbegin(input), --std::cend(input),
                    [&sep, &os](const auto& item) { os << int { item } << sep; });
            }
            else
            {
                std::for_each(std::cbegin(input), --std::cend(input),
                    [&sep, &os](const auto& item) { os << item << sep; });
            }
        }
        if constexpr (is_char_like_v<input_t>)
        {
            os << int { input.back() };
        }
        else
        {
            os << input.back();
        }
    }
    os << " ]";
    return os;
}

template <class stream_t, class input_t>
inline stream_t& stream_string_like(stream_t& os, const input_t& input)
{
    os << "\"";
    std::string_view sv { reinterpret_cast<const char*>(input.data()), std::size(input) };
    os << sv;
    os << "\"";
    return os;
}

template <class stream_t>
stream_t& operator<<(stream_t& os, const cdf::data_t& data)
{
    using namespace cdf;
    switch (data.type())
    {
        case CDF_Types::CDF_BYTE:
            stream_collection(os, data.get<CDF_Types::CDF_BYTE>(), ", ");
            break;
        case CDF_Types::CDF_INT1:
            stream_collection(os, data.get<CDF_Types::CDF_INT1>(), ", ");
            break;
        case CDF_Types::CDF_UINT1:
            stream_collection(os, data.get<CDF_Types::CDF_UINT1>(), ", ");
            break;
        case CDF_Types::CDF_INT2:
            stream_collection(os, data.get<CDF_Types::CDF_INT2>(), ", ");
            break;
        case CDF_Types::CDF_INT4:
            stream_collection(os, data.get<CDF_Types::CDF_INT4>(), ", ");
            break;
        case CDF_Types::CDF_INT8:
            stream_collection(os, data.get<CDF_Types::CDF_INT8>(), ", ");
            break;
        case CDF_Types::CDF_UINT2:
            stream_collection(os, data.get<CDF_Types::CDF_UINT2>(), ", ");
            break;
        case CDF_Types::CDF_UINT4:
            stream_collection(os, data.get<CDF_Types::CDF_UINT4>(), ", ");
            break;
        case CDF_Types::CDF_DOUBLE:
            stream_collection(os, data.get<CDF_Types::CDF_DOUBLE>(), ", ");
            break;
        case CDF_Types::CDF_REAL8:
            stream_collection(os, data.get<CDF_Types::CDF_REAL8>(), ", ");
            break;
        case CDF_Types::CDF_FLOAT:
            stream_collection(os, data.get<CDF_Types::CDF_FLOAT>(), ", ");
            break;
        case CDF_Types::CDF_REAL4:
            stream_collection(os, data.get<CDF_Types::CDF_REAL4>(), ", ");
            break;
        case CDF_Types::CDF_EPOCH:
            stream_collection(os, data.get<CDF_Types::CDF_EPOCH>(), ", ");
            break;
        case CDF_Types::CDF_EPOCH16:
            stream_collection(os, data.get<CDF_Types::CDF_EPOCH16>(), ", ");
            break;
        case CDF_Types::CDF_TIME_TT2000:
            stream_collection(os, data.get<CDF_Types::CDF_TIME_TT2000>(), ", ");
            break;
        case cdf::CDF_Types::CDF_UCHAR:
            stream_string_like(os, data.get<CDF_Types::CDF_UCHAR>());
            break;
        case cdf::CDF_Types::CDF_CHAR:
            stream_string_like(os, data.get<CDF_Types::CDF_CHAR>());
            break;
        default:
            break;
    }

    return os;
}


template <class stream_t>
inline stream_t& operator<<(stream_t& os, const cdf_majority& majority)
{
    os << fmt::format("majority: {}", cdf_majority_str(majority));
    return os;
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const cdf_compression_type& compression)
{
    os << fmt::format("compression: {}", cdf_compression_type_str(compression));
    return os;
}

struct indent_t
{
    int n = 0;
    char c = ' ';
    indent_t operator+(int v) const { return indent_t { n + v, c }; }
};

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const indent_t& ind)
{
    for (int i = 0; i < ind.n; i++)
        os << ind.c;
    return os;
}
