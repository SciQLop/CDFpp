/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#include "attribute.hpp"
#include "buffers.hpp"
#include "repr.hpp"

#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf-map.hpp>
#include <cdfpp/cdf-repr.hpp>
#include <cdfpp/variable.hpp>

#include <cdfpp/chrono/cdf-chrono.hpp>
#include <cdfpp/no_init_vector.hpp>
#include <cdfpp_config.h>
using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/iostream.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

template <typename T>
void _set_values(Variable& var, const py::buffer& buffer)
{
    py::buffer_info info = buffer.request();
    if (info.itemsize != static_cast<ssize_t>(sizeof(T)))
        throw std::invalid_argument { "Incompatible python and cdf types" };
    typename Variable::shape_t shape(info.ndim);
    std::copy(std::cbegin(info.shape), std::cend(info.shape), std::begin(shape));
    no_init_vector<T> values(info.size);
    std::memcpy(values.data(), info.ptr, info.size * sizeof(T));
    var.set_data(data_t { std::move(values) }, std::move(shape));
}

template <typename T>
void _set_time_values(Variable& var, const py::buffer& buffer)
{
    py::buffer_info info = buffer.request();
    typename Variable::shape_t shape(info.ndim);
    std::copy(std::cbegin(info.shape), std::cend(info.shape), std::begin(shape));
    no_init_vector<T> values(info.size);
    std::transform(reinterpret_cast<uint64_t*>(info.ptr),
        reinterpret_cast<uint64_t*>(info.ptr) + info.size, std::begin(values),
        [](const uint64_t& value)
        {
            return to_cdf_time<T>(std::chrono::high_resolution_clock::time_point {
                std::chrono::nanoseconds { value } });
        });
    var.set_data(data_t { std::move(values) }, std::move(shape));
}

void set_values(Variable& var, const py::buffer& buffer, CDF_Types cdf_type)
{
    switch (cdf_type)
    {
        case cdf::CDF_Types::CDF_INT1: // int8
            _set_values<int8_t>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_UINT1: // uint8
            _set_values<uint8_t>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_INT2: // int16
            _set_values<int16_t>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_UINT2: // uint16
            _set_values<uint16_t>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_INT4: // int32
            _set_values<int32_t>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_UINT4: // uint32
            _set_values<uint32_t>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_INT8: // int64
            _set_values<int64_t>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_FLOAT: // float
            _set_values<float>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_DOUBLE: // double
            _set_values<double>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
            _set_time_values<tt2000_t>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_EPOCH:
            _set_time_values<epoch>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_EPOCH16:
            _set_time_values<epoch16>(var, buffer);
            break;
        default:
            throw std::invalid_argument { "Unsuported CDF Type" };
            break;
    }
}


template <typename T>
void def_variable_wrapper(T& mod)
{
    py::class_<Variable>(mod, "Variable", py::buffer_protocol())
        .def("__repr__", __repr__<Variable>)
        .def_readonly(
            "attributes", &Variable::attributes, py::return_value_policy::reference_internal)
        .def_property_readonly("name", &Variable::name)
        .def_property_readonly("type", &Variable::type)
        .def_property_readonly("shape", &Variable::shape)
        .def_property_readonly("majority", &Variable::majority)
        .def_property_readonly("values_loaded", &Variable::values_loaded)
        .def_property("compression", &Variable::compression_type, &Variable::set_compression_type)
        .def_buffer([](Variable& var) -> py::buffer_info { return make_buffer(var); })
        .def_property_readonly("values", make_values_view, py::keep_alive<0, 1>())
        .def("_set_values", set_values, py::arg("values").noconvert(), py::arg("cdf_type"))
        .def("add_attribute",
            static_cast<Attribute& (*)(Variable&, const std::string&, py_cdf_attr_data_t&)>(
                add_attribute),
            py::arg { "name" }, py::arg { "value" }, py::return_value_policy::reference_internal);
}
