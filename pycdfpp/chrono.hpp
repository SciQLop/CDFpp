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
#pragma once

#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename time_t, typename function_t>
inline auto transform(time_t* input, std::size_t count, const function_t& f)
{
    auto result = py::array_t<uint64_t>(count);
    py::buffer_info res_buff = result.request(true);
    int64_t* res_ptr = static_cast<int64_t*>(res_buff.ptr);
    std::transform(input, input + count, res_ptr, f);
    return result;
}

template <typename time_t, typename T, typename function_t>
inline auto transform(const py::array_t<T>& input, const function_t& f)
{
    py::buffer_info in_buff = input.request();
    time_t* in_ptr = static_cast<time_t*>(in_buff.ptr);
    return transform(in_ptr, in_buff.shape[0], f);
}

template <typename time_t, typename T, typename function_t>
inline auto transform(const std::vector<T>& input, const function_t& f)
{
    auto result = py::array_t<uint64_t>(std::size(input));
    py::buffer_info res_buff = result.request(true);
    int64_t* res_ptr = static_cast<int64_t*>(res_buff.ptr);
    std::transform(std::cbegin(input), std::cend(input), res_ptr, f);
    return result;
}

template <typename time_t>
constexpr auto time_t_to_dtype()
{
    using period = typename decltype(cdf::to_time_point(std::declval<time_t>()))::duration::period;
    if constexpr (std::is_same_v<period, std::pico>)
        return "datetime64[ps]";
    if constexpr (std::is_same_v<period, std::nano>)
        return "datetime64[ns]";
    if constexpr (std::is_same_v<period, std::micro>)
        return "datetime64[us]";
    if constexpr (std::is_same_v<period, std::milli>)
        return "datetime64[ms]";
    if constexpr (std::is_same_v<period, std::ratio<1>>)
        return "datetime64[s]";
}

template <typename time_t>
inline py::object array_to_datetime64(const py::array_t<time_t>& input)
{
    constexpr auto dtype = time_t_to_dtype<time_t>();
    if (input.ndim() > 0)
    {
        auto result = transform<time_t>(input,
            [](const time_t& v) { return cdf::to_time_point(v).time_since_epoch().count(); });
        return py::cast(&result).attr("astype")(dtype);
    }
    return py::none();
}

template <typename time_t>
inline py::object scalar_to_datetime64(const time_t& input)
{
    constexpr auto dtype = time_t_to_dtype<time_t>();
    auto v = new int64_t;
    *v = cdf::to_time_point(input).time_since_epoch().count();
    return py::array(py::dtype(dtype), {}, {}, v);
}

template <typename time_t>
inline py::object vector_to_datetime64(const std::vector<time_t>& input)
{
    constexpr auto dtype = time_t_to_dtype<time_t>();

    auto result = transform<time_t>(
        input, [](const time_t& v) { return cdf::to_time_point(v).time_since_epoch().count(); });
    return py::cast(&result).attr("astype")(dtype);
}

template <typename time_t>
inline std::vector<decltype(to_time_point(time_t {}))> vector_to_datetime(
    const std::vector<time_t>& input)
{
    std::vector<decltype(to_time_point(time_t {}))> result(std::size(input));
    std::transform(std::cbegin(input), std::cend(input), std::begin(result),
        static_cast<decltype(to_time_point(time_t {})) (*)(const time_t&)>(to_time_point));
    return result;
}

inline py::object var_to_datetime64(const Variable& input)
{
    switch (input.type())
    {
        case cdf::CDF_Types::CDF_EPOCH:
        {
            auto result = transform(input.get<cdf::CDF_Types::CDF_EPOCH>().data(), input.shape()[0],
                [](const epoch& v) { return cdf::to_time_point(v).time_since_epoch().count(); });
            return py::cast(&result).attr("astype")("datetime64[ns]");
        }
        break;
        case cdf::CDF_Types::CDF_EPOCH16:
        {
            auto result = transform(input.get<cdf::CDF_Types::CDF_EPOCH16>().data(),
                input.shape()[0],
                [](const epoch16& v) { return cdf::to_time_point(v).time_since_epoch().count(); });
            return py::cast(&result).attr("astype")("datetime64[ns]");
        }
        break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
        {
            auto result = transform(input.get<cdf::CDF_Types::CDF_TIME_TT2000>().data(),
                input.shape()[0],
                [](const tt2000_t& v) { return cdf::to_time_point(v).time_since_epoch().count(); });
            return py::cast(&result).attr("astype")("datetime64[ns]");
        }
        break;
        default:
            throw std::out_of_range("Only supports cdf time types");
            break;
    }
    return {};
}


inline std::vector<decltype(to_time_point(epoch {}))> var_to_datetime(const Variable& input)
{
    switch (input.type())
    {
        case cdf::CDF_Types::CDF_EPOCH:
        {
            using namespace std::chrono;
            std::vector<decltype(to_time_point(epoch {}))> result(input.len());
            std::transform(std::cbegin(input.get<epoch>()), std::cend(input.get<epoch>()),
                std::begin(result),
                static_cast<decltype(to_time_point(epoch {})) (*)(const epoch&)>(to_time_point));
            return result;
        }
        break;
        case cdf::CDF_Types::CDF_EPOCH16:
        {
            std::vector<decltype(to_time_point(epoch16 {}))> result(input.len());
            std::transform(std::cbegin(input.get<epoch16>()), std::cend(input.get<epoch16>()),
                std::begin(result),
                static_cast<decltype(to_time_point(epoch16 {})) (*)(const epoch16&)>(
                    to_time_point));
            return result;
        }
        break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
        {
            std::vector<decltype(to_time_point(tt2000_t {}))> result(input.len());
            std::transform(std::cbegin(input.get<tt2000_t>()), std::cend(input.get<tt2000_t>()),
                std::begin(result),
                static_cast<decltype(to_time_point(tt2000_t {})) (*)(const tt2000_t&)>(
                    to_time_point));
            return result;
        }
        break;
        default:
            throw std::out_of_range("Only supports cdf time types");
            break;
    }
    return {};
}
