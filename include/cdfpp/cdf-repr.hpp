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
#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
#include <chrono>
#include <fmt/core.h>
#include <iomanip>
#include <sstream>
#include <string>
using namespace cdf;

inline std::ostream& operator<<(std::ostream& os, const cdf::data_t& data);

template <typename collection_t>
constexpr bool is_char_like_v
    = std::is_same_v<int8_t,
          std::remove_cv_t<
              std::remove_reference_t<decltype(*std::cbegin(std::declval<collection_t>()))>>>
    or std::is_same_v<uint8_t,
        std::remove_cv_t<
            std::remove_reference_t<decltype(*std::cbegin(std::declval<collection_t>()))>>>;

template <int widtw>
std::ostream& fixed_width(std::ostream& os)
{
    return os << std::setprecision(widtw) << std::setw(widtw) << std::setfill('0');
};

inline std::ostream& operator<<(
    std::ostream& os, const std::chrono::time_point<std::chrono::system_clock>& tp)
{
    const auto time_t = std::chrono::system_clock::to_time_t(tp);
    const auto tt = std::gmtime(&time_t);
    const auto us
        = (std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()) % 1000000)
              .count();
    // clang-format off
    os << fixed_width<4> << tt->tm_year + 1900 << '-'
       << fixed_width<2> << tt->tm_mon + 1     << '-'
       << fixed_width<2> << tt->tm_mday        << 'T'
       << fixed_width<2> << tt->tm_hour        << ':'
       << fixed_width<2> << tt->tm_min         << ':'
       << fixed_width<2> << tt->tm_sec         << '.'
       << fixed_width<6> << us;
    // clang-format on
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const epoch& time)
{
    os << cdf::to_time_point(time) << std::endl;
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const epoch16& time)
{
    os << cdf::to_time_point(time) << std::endl;
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const tt2000_t& time)
{
    os << cdf::to_time_point(time) << std::endl;
    return os;
}

template <class input_t, class item_t>
inline std::ostream& stream_collection(std::ostream& os, const input_t& input, const item_t& sep)
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

template <class input_t>
inline std::ostream& stream_string_like(std::ostream& os, const input_t& input)
{
    os << "\"";
    std::string_view sv { reinterpret_cast<const char*>(input.data()), std::size(input) };
    os << sv;
    os << "\"";
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const cdf::data_t& data)
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

inline std::ostream& operator<<(std::ostream& os, const cdf::Attribute& attribute)
{
    if (std::size(attribute) == 1
        and (attribute.front().type() == cdf::CDF_Types::CDF_CHAR
            or attribute.front().type() == cdf::CDF_Types::CDF_UCHAR))
    {
        os << attribute.name << ": " << attribute.front() << std::endl;
    }
    else
    {
        os << attribute.name << ": [ ";
        stream_collection(os, attribute, ", ");
        os << " ]" << std::endl;
    }
    return os;
}


inline std::ostream& operator<<(std::ostream& os, const cdf::Variable& var)
{
    os << var.name() << ": (";
    stream_collection(os, var.shape(), ", ");
    os << ") [" << cdf_type_str(var.type()) << "]" << std::endl;
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const cdf_majority& majority)
{
    if (majority == cdf_majority::row)
        os << "majority: row";
    else
        os << "majority: row";
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const cdf::CDF& cdf_file)
{
    os << "CDF:\n"
       << fmt::format("version: {}.{}.{}\n", std::get<0>(cdf_file.distribution_version),
              std::get<1>(cdf_file.distribution_version),
              std::get<2>(cdf_file.distribution_version))
       << cdf_file.majority << "\n\nAttributes:\n";
    std::for_each(std::cbegin(cdf_file.attributes), std::cend(cdf_file.attributes),
        [&os](const auto& item) { os << "\t" << item.second; });
    os << "\nVariables:\n";
    std::for_each(std::cbegin(cdf_file.variables), std::cend(cdf_file.variables),
        [&os](const auto& item) { os << "\t" << item.second; });
    os << std::endl;

    return os;
}


template <typename T>
inline std::string __repr__(T& obj)
{
    std::stringstream sstr;
    sstr << obj;
    return sstr.str();
}
