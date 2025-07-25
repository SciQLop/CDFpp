/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2024, Laboratory of Plasma Physics- CNRS
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the “Software”), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is furnished to do
-- so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
-- INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
-- PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
-- HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
-- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
-- SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace docstrings
{
constexpr auto _Variable = R"(
A CDF Variable (either R or Z variable)

Attributes
----------
attributes: dict
    variable attributes
name: str
    variable name
type: DataType
    variable data type (ie CDF_DOUBLE, CDF_TIME_TT2000, ...)
shape: List[int]
    variable shape (records + record shape)
majority: cdf_majority
    variable majority as writen in the CDF file, note that pycdfpp will always expose row major data.
values_loaded: bool
    True if values are availbale in memory, this is usefull with lazy loading to know if values are already loaded.
compression: CompressionType
    variable compression type (supported values are no_compression, rle_compression, gzip_compression)
values: numpy.array
    returns variable values as a numpy.array of the corresponding dtype and shape, note that no copies are involved, the returned array is just a view on variable data.
values_encoded: numpy.array
    same as `values` except that string variable are encoded wihch involves a data copy and since numpy uses UTF-32, expect a 4x memory increase for string values

Methods
-------
add_attribute
    Adds an attribute to the variable. Raises an exception if the attribute already exists.
set_compression_type
    Sets the variable compression type
set_values
    Sets the variable values

)";

}

namespace py = pybind11;

template <CDF_Types data_type>
std::pair<data_t, typename Variable::shape_t> _numeric_to_nd_data_t(const py::buffer& buffer)
{
    py::buffer_info info = buffer.request();
    using T = from_cdf_type_t<data_type>;
    if (info.itemsize != static_cast<ssize_t>(sizeof(T)))
        throw std::invalid_argument { "Incompatible python and cdf types" };
    typename Variable::shape_t shape(info.ndim);
    std::copy(std::cbegin(info.shape), std::cend(info.shape), std::begin(shape));
    if (info.size != 0)
    {
        no_init_vector<T> values(info.size);
        std::memcpy(values.data(), info.ptr, info.size * sizeof(T));
        return { data_t { std::move(values), data_type }, std::move(shape) };
    }
    else
    {
        return { data_t { no_init_vector<T>(0), data_type }, std::move(shape) };
    }
}

template <CDF_Types data_type>
std::pair<data_t, typename Variable::shape_t> _str_to_nd_data_t(const py::buffer& buffer)
{
    py::buffer_info info = buffer.request();
    typename Variable::shape_t shape(info.ndim + 1);
    std::copy(std::cbegin(info.shape), std::cend(info.shape), std::begin(shape));
    shape[info.ndim] = info.itemsize;
    no_init_vector<from_cdf_type_t<data_type>> values(flat_size(shape));
    std::memcpy(values.data(), info.ptr, std::size(values));
    return { data_t { std::move(values), data_type }, std::move(shape) };
}

template <typename T>
std::pair<data_t, typename Variable::shape_t> _time_to_nd_data_t(const py::buffer& buffer)
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
    return { data_t { std::move(values) }, std::move(shape) };
}

template <CDF_Types cdf_type>
void _set_var_data_t(Variable& var, const py::buffer& buffer)
{

    if constexpr (cdf_type == cdf::CDF_Types::CDF_UCHAR or cdf_type == cdf::CDF_Types::CDF_CHAR)
    {
        auto [data, shape] = _str_to_nd_data_t<cdf_type>(buffer);
        var.set_data(std::move(data), std::move(shape));
    }
    else
    {
        if constexpr (cdf_type == cdf::CDF_Types::CDF_EPOCH
            or cdf_type == cdf::CDF_Types::CDF_EPOCH16
            or cdf_type == cdf::CDF_Types::CDF_TIME_TT2000)
        {
            auto [data, shape] = _time_to_nd_data_t<from_cdf_type_t<cdf_type>>(buffer);
            var.set_data(std::move(data), std::move(shape));
        }
        else
        {
            auto [data, shape] = _numeric_to_nd_data_t<cdf_type>(buffer);
            var.set_data(std::move(data), std::move(shape));
        }
    }
}

void set_values(Variable& var, const py::buffer& buffer, CDF_Types data_type)
{
    switch (data_type)
    {
        case cdf::CDF_Types::CDF_UCHAR: // string
            _set_var_data_t<cdf::CDF_Types::CDF_UCHAR>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_CHAR: // string
            _set_var_data_t<cdf::CDF_Types::CDF_CHAR>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_INT1: // int8
            _set_var_data_t<cdf::CDF_Types::CDF_INT1>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_UINT1: // uint8
            _set_var_data_t<cdf::CDF_Types::CDF_UINT1>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_INT2: // int16
            _set_var_data_t<cdf::CDF_Types::CDF_INT2>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_UINT2: // uint16
            _set_var_data_t<cdf::CDF_Types::CDF_UINT2>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_INT4: // int32
            _set_var_data_t<cdf::CDF_Types::CDF_INT4>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_UINT4: // uint32
            _set_var_data_t<cdf::CDF_Types::CDF_UINT4>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_INT8: // int64
            _set_var_data_t<cdf::CDF_Types::CDF_INT8>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_FLOAT: // float
            _set_var_data_t<cdf::CDF_Types::CDF_FLOAT>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_REAL4: // double
            _set_var_data_t<cdf::CDF_Types::CDF_REAL4>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_DOUBLE: // double
            _set_var_data_t<cdf::CDF_Types::CDF_DOUBLE>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_REAL8: // double
            _set_var_data_t<cdf::CDF_Types::CDF_REAL8>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
            _set_var_data_t<cdf::CDF_Types::CDF_TIME_TT2000>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_EPOCH:
            _set_var_data_t<cdf::CDF_Types::CDF_EPOCH>(var, buffer);
            break;
        case cdf::CDF_Types::CDF_EPOCH16:
            _set_var_data_t<cdf::CDF_Types::CDF_EPOCH16>(var, buffer);
            break;
        default:
            throw std::invalid_argument { "Unsuported CDF Type" };
            break;
    }
}


template <typename T>
void def_variable_wrapper(T& mod)
{
    py::class_<Variable>(mod, "Variable", py::buffer_protocol(), docstrings::_Variable)
        .def("__repr__", __repr__<Variable>)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__len__", &Variable::len)
        .def_readonly(
            "attributes", &Variable::attributes, py::return_value_policy::reference_internal)
        .def_property_readonly("name", &Variable::name)
        .def_property_readonly("type", &Variable::type)
        .def_property_readonly("shape",
            [](const Variable& v)
            {
                auto shape = py::tuple(std::size(v.shape()));
                for (auto i = 0UL; i < std::size(v.shape()); i++)
                {
                    shape[i] = v.shape()[i];
                }
                return shape;
            })
        .def_property_readonly("majority", &Variable::majority)
        .def_property_readonly("is_nrv", &Variable::is_nrv)
        .def_property_readonly("values_loaded", &Variable::values_loaded)
        .def_property("compression", &Variable::compression_type, &Variable::set_compression_type)
        .def_buffer([](Variable& var) -> py::buffer_info { return make_buffer(var); })
        .def_property_readonly("values", make_values_view<false>, py::keep_alive<0, 1>())
        .def_property_readonly("values_encoded", make_values_view<true>, py::keep_alive<0, 1>())
        .def("_set_values", set_values, py::arg("values").noconvert(), py::arg("data_type"))
        .def("_set_values",
            [](Variable& var, const Variable& source)
            {
                var.set_data(source);
            },
            py::arg("source"))
        .def("_add_attribute",
            static_cast<VariableAttribute& (*)(Variable&, const std::string&,
                const string_or_buffer_t&, CDF_Types)>(add_attribute),
            py::arg { "name" }, py::arg { "values" }, py::arg { "data_type" },
            py::return_value_policy::reference_internal)
        .def(
            "_add_attribute",
            [](Variable& var, const VariableAttribute& attr)
            {
                if (var.attributes.count(attr.name) == 0)
                {
                    var.attributes.emplace(attr.name, attr);
                    return var.attributes[attr.name];
                }
                else
                {
                    throw std::invalid_argument { "Attribute already exists" };
                }
            },
            py::arg("attribute"), py::return_value_policy::reference_internal);
}
