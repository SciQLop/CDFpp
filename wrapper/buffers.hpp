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
#include <cdf-chrono.hpp>
#include <cdf-data.hpp>
#include <cdf.hpp>
using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace _details
{

std::vector<ssize_t> shape_ssize_t(const Variable& var)
{
    auto shape = var.shape();
    std::vector<ssize_t> res(std::size(shape));
    std::transform(std::cbegin(shape), std::cend(shape), std::begin(res),
        [](auto v) { return static_cast<ssize_t>(v); });
    return res;
}

template <typename T>
std::vector<ssize_t> strides(const Variable& var)
{
    auto shape = var.shape();
    std::vector<ssize_t> res(std::size(shape));
    std::transform(std::crbegin(shape), std::crend(shape), std::begin(res),
        [next = sizeof(T)](auto v) mutable
        {
            auto res = next;
            next = static_cast<ssize_t>(v * next);
            return res;
        });
    std::reverse(std::begin(res), std::end(res)); // if column major
    return res;
}

template <typename T, typename U = T>
py::buffer_info make_buffer(cdf::Variable& var, bool readonly = true)
{
    return py::buffer_info(var.get<U>().data(), /* Pointer to buffer */
        sizeof(T), /* Size of one scalar */
        py::format_descriptor<T>::format(),
        static_cast<ssize_t>(std::size(var.shape())), /* Number of dimensions */
        shape_ssize_t(var), strides<T>(var), readonly);
}

template <typename data_t, typename raw_t = data_t>
py::memoryview make_view(cdf::Variable& variable, bool readonly = true)
{
    return py::memoryview::from_buffer(reinterpret_cast<raw_t*>(variable.get<data_t>().data()),
        shape_ssize_t(variable), strides<data_t>(variable), readonly);
}

template <typename data_t, typename raw_t = data_t>
py::array make_array(Variable& variable, py::object& obj, bool readonly = true)
{
    return py::array_t<data_t>(
        shape_ssize_t(variable), strides<data_t>(variable), variable.get<data_t>().data(), obj);
}

template <typename char_type>
std::variant<py::array, std::string_view, std::vector<std::string_view>,
    std::vector<std::vector<std::string_view>>>
make_str_view(Variable& variable)
{
    const auto& shape = variable.shape();
    char* buffer = reinterpret_cast<char*>(variable.get<char_type>().data());
    if (std::size(shape) == 1)
    {
        return std::string_view { buffer, shape[0] };
    }
    if (std::size(shape) == 2)
    {
        std::vector<std::string_view> result(shape[0]);
        for (std::size_t index = 0; index < shape[0]; index++)
        {
            result[index] = std::string_view { buffer + (index * shape[1]), shape[1] };
        }
        return result;
    }
    if (std::size(shape) == 3)
    {
        std::vector<std::vector<std::string_view>> result(shape[0]);
        auto buffer_offset = 0UL;
        for (std::size_t i = 0; i < shape[0]; i++)
        {
            result[i].resize(shape[1]);
            for (std::size_t j = 0; j < shape[1]; j++)
            {
                result[i][j] = std::string_view { buffer + buffer_offset, shape[2] };
                buffer_offset += shape[2];
            }
        }
        return result;
    }
    return {};
}

template <typename T, typename U = T>
py::buffer_info impl_make_buffer(cdf::Variable& var)
{
    return py::buffer_info(var.get<U>().data(), /* Pointer to buffer */
        sizeof(T), /* Size of one scalar */
        py::format_descriptor<T>::format(),
        static_cast<ssize_t>(std::size(var.shape())), /* Number of dimensions */
        shape_ssize_t(var), strides<T>(var), true);
}

}

std::variant<py::array, std::string_view, std::vector<std::string_view>,
    std::vector<std::vector<std::string_view>>>
make_values_view(py::object& obj)
{
    Variable& variable = obj.cast<Variable&>();
    switch (variable.type())
    {
        case cdf::CDF_Types::CDF_CHAR:
            return _details::make_str_view<char>(variable);
        case cdf::CDF_Types::CDF_UCHAR:
            return _details::make_str_view<unsigned char>(variable);
        case cdf::CDF_Types::CDF_INT1:
            return _details::make_array<int8_t>(variable, obj);
        case cdf::CDF_Types::CDF_INT2:
            return _details::make_array<int16_t>(variable, obj);
        case cdf::CDF_Types::CDF_INT4:
            return _details::make_array<int32_t>(variable, obj);
        case cdf::CDF_Types::CDF_INT8:
            return _details::make_array<int64_t>(variable, obj);
        case cdf::CDF_Types::CDF_UINT1:
        case cdf::CDF_Types::CDF_BYTE:
            return _details::make_array<uint8_t>(variable, obj);
        case cdf::CDF_Types::CDF_UINT2:
            return _details::make_array<uint16_t>(variable, obj);
        case cdf::CDF_Types::CDF_UINT4:
            return _details::make_array<uint32_t>(variable, obj);
        case cdf::CDF_Types::CDF_FLOAT:
        case cdf::CDF_Types::CDF_REAL4:
            return _details::make_array<float>(variable, obj);
        case cdf::CDF_Types::CDF_DOUBLE:
        case cdf::CDF_Types::CDF_REAL8:
            return _details::make_array<double>(variable, obj);
        case cdf::CDF_Types::CDF_EPOCH:
            return _details::make_array<epoch, double>(variable, obj);
        case cdf::CDF_Types::CDF_EPOCH16:
            return _details::make_array<epoch16, long double>(variable, obj);
        case cdf::CDF_Types::CDF_TIME_TT2000:
            return _details::make_array<tt2000_t, int64_t>(variable, obj);
    }
}

py::memoryview make_view(cdf::Variable& variable)
{
    switch (variable.type())
    {
        case cdf::CDF_Types::CDF_INT1:
            return _details::make_view<int8_t>(variable);
        case cdf::CDF_Types::CDF_INT2:
            return _details::make_view<int16_t>(variable);
        case cdf::CDF_Types::CDF_INT4:
            return _details::make_view<int32_t>(variable);
        case cdf::CDF_Types::CDF_INT8:
            return _details::make_view<int64_t>(variable);
        case cdf::CDF_Types::CDF_UINT1:
        case cdf::CDF_Types::CDF_BYTE:
            return _details::make_view<uint8_t>(variable);
        case cdf::CDF_Types::CDF_UINT2:
            return _details::make_view<uint16_t>(variable);
        case cdf::CDF_Types::CDF_UINT4:
            return _details::make_view<uint32_t>(variable);
        case cdf::CDF_Types::CDF_FLOAT:
        case cdf::CDF_Types::CDF_REAL4:
            return _details::make_view<float>(variable);
        case cdf::CDF_Types::CDF_DOUBLE:
        case cdf::CDF_Types::CDF_REAL8:
            return _details::make_view<double>(variable);
        case cdf::CDF_Types::CDF_EPOCH:
            return _details::make_view<epoch, double>(variable);
        case cdf::CDF_Types::CDF_EPOCH16:
            return _details::make_view<epoch16, long double>(variable);
        case cdf::CDF_Types::CDF_TIME_TT2000:
            return _details::make_view<tt2000_t, int64_t>(variable);
    }
}

py::buffer_info make_buffer(cdf::Variable& variable)
{

    switch (variable.type())
    {
        case cdf::CDF_Types::CDF_INT1:
            return _details::impl_make_buffer<int8_t>(variable);
        case cdf::CDF_Types::CDF_INT2:
            return _details::impl_make_buffer<int16_t>(variable);
        case cdf::CDF_Types::CDF_INT4:
            return _details::impl_make_buffer<int32_t>(variable);
        case cdf::CDF_Types::CDF_INT8:
            return _details::impl_make_buffer<int64_t>(variable);
        case cdf::CDF_Types::CDF_UINT1:
        case cdf::CDF_Types::CDF_BYTE:
            return _details::impl_make_buffer<uint8_t>(variable);
        case cdf::CDF_Types::CDF_UINT2:
            return _details::impl_make_buffer<uint16_t>(variable);
        case cdf::CDF_Types::CDF_UINT4:
            return _details::impl_make_buffer<uint32_t>(variable);
        case cdf::CDF_Types::CDF_FLOAT:
        case cdf::CDF_Types::CDF_REAL4:
            return _details::impl_make_buffer<float>(variable);
        case cdf::CDF_Types::CDF_DOUBLE:
        case cdf::CDF_Types::CDF_REAL8:
            return _details::impl_make_buffer<double>(variable);
        case cdf::CDF_Types::CDF_EPOCH:
            return _details::impl_make_buffer<epoch>(variable);
        case cdf::CDF_Types::CDF_EPOCH16:
            return _details::impl_make_buffer<epoch16>(variable);
        case cdf::CDF_Types::CDF_TIME_TT2000:
            return _details::impl_make_buffer<tt2000_t, int64_t>(variable);
        default:
            return py::buffer_info {};
    }
}
