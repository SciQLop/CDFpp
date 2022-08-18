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
    std::reverse(std::begin(res), std::end(res));
    return res;
}

template <CDF_Types data_t>
py::array make_array(Variable& variable, py::object& obj)
{
    static_assert(data_t != CDF_Types::CDF_CHAR and data_t != CDF_Types::CDF_UCHAR);
    return py::array_t<from_cdf_type_t<data_t>>(shape_ssize_t(variable),
        strides<from_cdf_type_t<data_t>>(variable), variable.get<from_cdf_type_t<data_t>>().data(),
        obj);
}

template <typename T, typename size_type>
static inline std::string_view make_string_view(T* data, size_type len)
{
    return std::string_view(reinterpret_cast<const char*>(data),
        static_cast<std::string_view::size_type>(len));
}

template <typename T>
py::object make_list(const T* data, decltype(std::declval<Variable>().shape()) shape)
{
    if (std::size(shape) > 2)
    {
        py::list res {};
        std::size_t offset = 0;
        auto inner_shape = decltype(shape) { std::begin(shape) + 1, std::end(shape) };
        std::size_t jump = std::accumulate(
            std::cbegin(inner_shape), std::cend(inner_shape), 1UL, std::multiplies<std::size_t>());
        for (auto i = 0UL; i < shape[0]; i++)
        {
            res.append(make_list(data+offset, inner_shape));
            offset += jump;
        }
        return res;
    }
    if(std::size(shape)==2)
    {
        py::list res {};
        std::size_t offset = 0;
        for (auto i = 0UL; i < shape[0]; i++)
        {
            res.append(make_string_view(data + offset,shape[1]));
            offset += shape[1];
        }
        return res;
    }
    if(std::size(shape)==1)
    {
        return py::str(make_string_view(data ,shape[0]));
    }
    return py::none();
}

template <CDF_Types data_t>
py::object make_list(Variable& variable)
{
    static_assert(data_t == CDF_Types::CDF_CHAR or data_t == CDF_Types::CDF_UCHAR);
    return make_list(variable.get<from_cdf_type_t<data_t>>().data(), variable.shape());
}

template <CDF_Types T>
py::buffer_info impl_make_buffer(cdf::Variable& var)
{
    using U = cdf::from_cdf_type_t<T>;
    return py::buffer_info(var.get<T>().data(), /* Pointer to buffer */
        sizeof(U), /* Size of one scalar */
        py::format_descriptor<U>::format(),
        static_cast<ssize_t>(std::size(var.shape())), /* Number of dimensions */
        shape_ssize_t(var), strides<U>(var), true);
}

}

py::object make_values_view(py::object& obj)
{
    Variable& variable = obj.cast<Variable&>();
    switch (variable.type())
    {
        case cdf::CDF_Types::CDF_CHAR:
            return _details::make_list<cdf::CDF_Types::CDF_CHAR>(variable);
        case cdf::CDF_Types::CDF_UCHAR:
            return _details::make_list<cdf::CDF_Types::CDF_UCHAR>(variable);
        case cdf::CDF_Types::CDF_INT1:
            return _details::make_array<CDF_Types::CDF_INT1>(variable, obj);
        case cdf::CDF_Types::CDF_INT2:
            return _details::make_array<CDF_Types::CDF_INT2>(variable, obj);
        case cdf::CDF_Types::CDF_INT4:
            return _details::make_array<CDF_Types::CDF_INT4>(variable, obj);
        case cdf::CDF_Types::CDF_INT8:
            return _details::make_array<CDF_Types::CDF_INT8>(variable, obj);
        case cdf::CDF_Types::CDF_UINT1:
            return _details::make_array<CDF_Types::CDF_UINT1>(variable, obj);
        case cdf::CDF_Types::CDF_BYTE:
            return _details::make_array<CDF_Types::CDF_BYTE>(variable, obj);
        case cdf::CDF_Types::CDF_UINT2:
            return _details::make_array<CDF_Types::CDF_UINT2>(variable, obj);
        case cdf::CDF_Types::CDF_UINT4:
            return _details::make_array<CDF_Types::CDF_UINT4>(variable, obj);
        case cdf::CDF_Types::CDF_FLOAT:
            return _details::make_array<CDF_Types::CDF_FLOAT>(variable, obj);
        case cdf::CDF_Types::CDF_REAL4:
            return _details::make_array<CDF_Types::CDF_REAL4>(variable, obj);
        case cdf::CDF_Types::CDF_DOUBLE:
            return _details::make_array<CDF_Types::CDF_DOUBLE>(variable, obj);
        case cdf::CDF_Types::CDF_REAL8:
            return _details::make_array<CDF_Types::CDF_REAL8>(variable, obj);
        case cdf::CDF_Types::CDF_EPOCH:
            return _details::make_array<CDF_Types::CDF_EPOCH>(variable, obj);
        case cdf::CDF_Types::CDF_EPOCH16:
            return _details::make_array<CDF_Types::CDF_EPOCH16>(variable, obj);
        case cdf::CDF_Types::CDF_TIME_TT2000:
            return _details::make_array<CDF_Types::CDF_TIME_TT2000>(variable, obj);
        default:
            throw std::runtime_error { std::string { "Unsupported CDF type " }
                + std::to_string(static_cast<int>(variable.type())) };
            break;
    }
    return {};
}

py::buffer_info make_buffer(cdf::Variable& variable)
{

    using namespace cdf;
    switch (variable.type())
    {
        case CDF_Types::CDF_INT1:
            return _details::impl_make_buffer<CDF_Types::CDF_INT1>(variable);
        case CDF_Types::CDF_INT2:
            return _details::impl_make_buffer<CDF_Types::CDF_INT2>(variable);
        case CDF_Types::CDF_INT4:
            return _details::impl_make_buffer<CDF_Types::CDF_INT4>(variable);
        case CDF_Types::CDF_INT8:
            return _details::impl_make_buffer<CDF_Types::CDF_INT8>(variable);
        case CDF_Types::CDF_BYTE:
            return _details::impl_make_buffer<CDF_Types::CDF_BYTE>(variable);
        case CDF_Types::CDF_UINT1:
            return _details::impl_make_buffer<CDF_Types::CDF_UINT1>(variable);
        case CDF_Types::CDF_UINT2:
            return _details::impl_make_buffer<CDF_Types::CDF_UINT2>(variable);
        case CDF_Types::CDF_UINT4:
            return _details::impl_make_buffer<CDF_Types::CDF_UINT4>(variable);
        case CDF_Types::CDF_FLOAT:
        case CDF_Types::CDF_REAL4:
            return _details::impl_make_buffer<CDF_Types::CDF_FLOAT>(variable);
        case CDF_Types::CDF_DOUBLE:
        case CDF_Types::CDF_REAL8:
            return _details::impl_make_buffer<CDF_Types::CDF_DOUBLE>(variable);
        case CDF_Types::CDF_EPOCH:
            return _details::impl_make_buffer<CDF_Types::CDF_EPOCH>(variable);
        case CDF_Types::CDF_EPOCH16:
            return _details::impl_make_buffer<CDF_Types::CDF_EPOCH16>(variable);
        case CDF_Types::CDF_TIME_TT2000:
            return _details::impl_make_buffer<CDF_Types::CDF_TIME_TT2000>(variable);
        default:
            throw std::runtime_error { std::string { "Unsupported CDF type " }
                + std::to_string(static_cast<int>(variable.type())) };
            break;
    }
}
