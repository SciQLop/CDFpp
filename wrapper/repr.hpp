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
#include <cdf-data.hpp>
#include <cdf-chrono.hpp>
#include <cdf.hpp>
#include <sstream>
#include <string>
using namespace cdf;

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

inline std::ostream& operator<<(std::ostream& os, const cdf::data_t& data);

template <typename collection_t>
constexpr bool is_char_like_v
    = std::is_same_v<int8_t,
          std::remove_cv_t<std::remove_reference_t<decltype(*std::cbegin(std::declval<
              collection_t>()))>>> or std::is_same_v<uint8_t, std::remove_cv_t<std::remove_reference_t<decltype(*std::cbegin(std::declval<collection_t>()))>>>;

template <class input_t, class item_t>
inline std::ostream& stream_collection(std::ostream& os, const input_t& input, const item_t& sep)
{
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
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const cdf::data_t& data)
{
    switch (data.type())
    {
        case cdf::CDF_Types::CDF_BYTE:
        case cdf::CDF_Types::CDF_INT1:
            os << "[ ";
            stream_collection(os, data.get<int8_t>(), ", ");
            os << " ]";
            break;
        case cdf::CDF_Types::CDF_UINT1:
            os << "[ ";
            stream_collection(os, data.get<uint8_t>(), ", ");
            os << " ]";
            break;
        case cdf::CDF_Types::CDF_INT2:
            os << "[ ";
            stream_collection(os, data.get<int16_t>(), ", ");
            os << " ]";
            break;
        case cdf::CDF_Types::CDF_INT4:
            os << "[ ";
            stream_collection(os, data.get<int32_t>(), ", ");
            os << " ]";
            break;
        case cdf::CDF_Types::CDF_INT8:
            os << "[ ";
            stream_collection(os, data.get<int64_t>(), ", ");
            os << " ]";
            break;
        case cdf::CDF_Types::CDF_UINT2:
            os << "[ ";
            stream_collection(os, data.get<uint16_t>(), ", ");
            os << " ]";
            break;
        case cdf::CDF_Types::CDF_UINT4:
            os << "[ ";
            stream_collection(os, data.get<uint32_t>(), ", ");
            os << " ]";
            break;
        case cdf::CDF_Types::CDF_DOUBLE:
        case cdf::CDF_Types::CDF_REAL8:
            os << "[ ";
            stream_collection(os, data.get<double>(), ", ");
            os << " ]";
            break;
        case cdf::CDF_Types::CDF_FLOAT:
        case cdf::CDF_Types::CDF_REAL4:
            os << "[ ";
            stream_collection(os, data.get<float>(), ", ");
            os << " ]";
            break;
        case cdf::CDF_Types::CDF_UCHAR:
        case cdf::CDF_Types::CDF_CHAR:
        {
            os << "\"";
            auto v = data.get<char>();
            std::string sv { v.data(), std::size(v) };
            os << sv;
            os << "\"";
        }
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
    if(majority==cdf_majority::row)
        os << "majority: row";
    else
        os << "majority: row";
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const cdf::CDF& cdf_file)
{
    os << "CDF:\n" << cdf_file.majority <<"\n\nAttributes:\n";
    std::for_each(std::cbegin(cdf_file.attributes), std::cend(cdf_file.attributes),
        [&os](const auto& item) { os << "\t" << item.second; });
    os << "\nVariables:\n";
    std::for_each(std::cbegin(cdf_file.variables), std::cend(cdf_file.variables),
        [&os](const auto& item) { os << "\t" << item.second; });
    os << std::endl;

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


template <typename T>
inline std::string __repr__(T& obj)
{
    std::stringstream sstr;
    sstr << obj;
    return sstr.str();
}
