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
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4127) // warning C4127: Conditional expression is constant
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include <cdfpp/attribute.hpp>
#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf-enums.hpp>
#include <cdfpp/cdf-file.hpp>
#include <cdfpp/cdf-map.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
#include <cdfpp/no_init_vector.hpp>
#include <cdfpp_config.h>

#include <cdfpp/cdf-repr.hpp>

#include "repr.hpp"

using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/iostream.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;


namespace docstrings
{
constexpr auto _VariableAddAttribute = R"(
Adds an attribute to the variable. Raises an exception if the attribute already exists.

Parameters
----------
name: str
    attribute name
values: Union[str, List[Union[str, int, float, datetime, timedelta]]]
    attribute values

Returns
-------
Attribute
    The created attribute

Raises
------
ValueError
    If the attribute already exists

)";

constexpr auto _CDFAddAttribute = R"(
Adds a global attribute to the CDF. Raises an exception if the attribute already exists.

Parameters
----------
name: str
    attribute name
values: Union[str, List[Union[str, int, float, datetime, timedelta]]]
    attribute values

Returns
-------
Attribute
    The created attribute

Raises
------
ValueError
    If the attribute already exists

)";
}

using py_cdf_attr_data_t = std::variant<std::monostate, std::string, no_init_vector<char>,
    no_init_vector<uint8_t>, no_init_vector<uint16_t>, no_init_vector<uint32_t>,
    no_init_vector<int8_t>, no_init_vector<int16_t>, no_init_vector<int32_t>,
    no_init_vector<int64_t>, no_init_vector<float>, no_init_vector<double>,
    no_init_vector<tt2000_t>, no_init_vector<epoch>, no_init_vector<epoch16>,
    no_init_vector<decltype(std::chrono::system_clock::now())>>;

using string_or_buffer_t = std::variant<std::string, std::vector<tt2000_t>, std::vector<epoch>,
    std::vector<epoch16>, py::buffer>;

template <typename... Ts>
auto visit(const py_cdf_attr_data_t& data, Ts... lambdas)
{
    return std::visit(helpers::make_visitor(lambdas...), data);
}

template <typename... Ts>
auto visit(py_cdf_attr_data_t& data, Ts... lambdas)
{
    return std::visit(helpers::make_visitor(lambdas...), data);
}

template <typename... Ts>
auto visit(const string_or_buffer_t& data, Ts... lambdas)
{
    return std::visit(helpers::make_visitor(lambdas...), data);
}

template <typename... Ts>
auto visit(string_or_buffer_t& data, Ts... lambdas)
{
    return std::visit(helpers::make_visitor(lambdas...), data);
}

[[nodiscard]] inline py_cdf_attr_data_t to_py_cdf_data(const cdf::data_t& data)
{
    switch (data.type())
    {
        case cdf::CDF_Types::CDF_BYTE:
        case cdf::CDF_Types::CDF_INT1:
            return data.get<int8_t>();
            break;
        case cdf::CDF_Types::CDF_UINT1:
            return data.get<uint8_t>();
            break;
        case cdf::CDF_Types::CDF_INT2:
            return data.get<int16_t>();
            break;
        case cdf::CDF_Types::CDF_INT4:
            return data.get<int32_t>();
            break;
        case cdf::CDF_Types::CDF_INT8:
            return data.get<int64_t>();
            break;
        case cdf::CDF_Types::CDF_UINT2:
            return data.get<uint16_t>();
            break;
        case cdf::CDF_Types::CDF_UINT4:
            return data.get<uint32_t>();
            break;
        case cdf::CDF_Types::CDF_DOUBLE:
        case cdf::CDF_Types::CDF_REAL8:
            return data.get<double>();
            break;
        case cdf::CDF_Types::CDF_FLOAT:
        case cdf::CDF_Types::CDF_REAL4:
            return data.get<float>();
            break;
        case cdf::CDF_Types::CDF_EPOCH:
            return data.get<epoch>();
            break;
        case cdf::CDF_Types::CDF_EPOCH16:
            return data.get<epoch16>();
            break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
            return data.get<tt2000_t>();
            break;
        case cdf::CDF_Types::CDF_UCHAR:
        {
            auto v = data.get<cdf::CDF_Types::CDF_UCHAR>();
            return std::string { reinterpret_cast<char*>(v.data()), std::size(v) };
        }
        break;
        case cdf::CDF_Types::CDF_CHAR:
        {
            auto v = data.get<char>();
            return std::string { v.data(), std::size(v) };
        }
        break;
        default:
            throw std::runtime_error { std::string { "Unsupported CDF type " }
                + std::to_string(static_cast<int>(data.type())) };
            break;
    }
    return {};
}

data_t to_attr_data_entry(const std::string& values, CDF_Types data_type)
{
    if (data_type == CDF_Types::CDF_CHAR or data_type == CDF_Types::CDF_UCHAR)
    {
        if (std::size(values) != 0)
        {
            return { no_init_vector<char>(std::cbegin(values), std::cend(values)), data_type };
        }
        else
        {
            return { no_init_vector<char> { { 0 } }, data_type };
        }
    }
    else
    {
        throw std::invalid_argument { "Incorrect CDF type for string value" };
    }
}

template <typename T>
data_t _time_to_data_t(const py::buffer& buffer)
{
    py::buffer_info info = buffer.request();
    if (info.ndim != 1)
        throw std::invalid_argument { "Incorrect dimension for attribute value" };
    no_init_vector<T> values(info.size);
    std::transform(reinterpret_cast<uint64_t*>(info.ptr),
        reinterpret_cast<uint64_t*>(info.ptr) + info.size, std::begin(values),
        [](const uint64_t& value)
        {
            return to_cdf_time<T>(std::chrono::high_resolution_clock::time_point {
                std::chrono::nanoseconds { value } });
        });
    return data_t { std::move(values) };
}

template <typename T>
data_t time_to_data_t(const std::vector<T>& values)
{
    return data_t { no_init_vector<T>(std::cbegin(values), std::cend(values)) };
}

template <CDF_Types data_type>
data_t _numeric_to_data_t(const py::buffer& buffer)
{
    py::buffer_info info = buffer.request();
    if (info.ndim != 1)
        throw std::invalid_argument { "Incorrect dimension for attribute value" };
    using T = from_cdf_type_t<data_type>;
    if (info.itemsize != static_cast<ssize_t>(sizeof(T)))
        throw std::invalid_argument { "Incompatible python and cdf types" };
    no_init_vector<T> values(info.size);
    std::memcpy(values.data(), info.ptr, info.size * sizeof(T));
    return data_t { std::move(values), data_type };
}

template <CDF_Types data_type>
data_t to_attr_data_entry(const py::buffer& buffer)
{
    if constexpr (data_type == cdf::CDF_Types::CDF_EPOCH or data_type == cdf::CDF_Types::CDF_EPOCH16
        or data_type == cdf::CDF_Types::CDF_TIME_TT2000)
    {
        return _time_to_data_t<from_cdf_type_t<data_type>>(buffer);
    }
    else
    {
        return _numeric_to_data_t<data_type>(buffer);
    }
}

data_t to_attr_data_entry(const py::buffer& buffer, CDF_Types data_type)
{
    switch (data_type)
    {
        case cdf::CDF_Types::CDF_INT1: // int8
            return to_attr_data_entry<cdf::CDF_Types::CDF_INT1>(buffer);
            break;
        case cdf::CDF_Types::CDF_INT2: // int16
            return to_attr_data_entry<cdf::CDF_Types::CDF_INT2>(buffer);
            break;
        case cdf::CDF_Types::CDF_INT4: // int32
            return to_attr_data_entry<cdf::CDF_Types::CDF_INT4>(buffer);
            break;
        case cdf::CDF_Types::CDF_INT8: // int64
            return to_attr_data_entry<cdf::CDF_Types::CDF_INT8>(buffer);
            break;
        case cdf::CDF_Types::CDF_UINT1: // uint8
            return to_attr_data_entry<cdf::CDF_Types::CDF_UINT1>(buffer);
            break;
        case cdf::CDF_Types::CDF_UINT2: // uint16
            return to_attr_data_entry<cdf::CDF_Types::CDF_UINT2>(buffer);
            break;
        case cdf::CDF_Types::CDF_UINT4: // uint32
            return to_attr_data_entry<cdf::CDF_Types::CDF_UINT4>(buffer);
            break;
        case cdf::CDF_Types::CDF_FLOAT: // float32
            return to_attr_data_entry<cdf::CDF_Types::CDF_FLOAT>(buffer);
            break;
        case cdf::CDF_Types::CDF_REAL4: // float32
            return to_attr_data_entry<cdf::CDF_Types::CDF_REAL4>(buffer);
            break;
        case cdf::CDF_Types::CDF_DOUBLE: // float64
            return to_attr_data_entry<cdf::CDF_Types::CDF_DOUBLE>(buffer);
            break;
        case cdf::CDF_Types::CDF_REAL8: // float64
            return to_attr_data_entry<cdf::CDF_Types::CDF_REAL8>(buffer);
            break;
        case cdf::CDF_Types::CDF_EPOCH: // epoch
            return to_attr_data_entry<cdf::CDF_Types::CDF_EPOCH>(buffer);
            break;
        case cdf::CDF_Types::CDF_EPOCH16: // epoch16
            return to_attr_data_entry<cdf::CDF_Types::CDF_EPOCH16>(buffer);
            break;
        case cdf::CDF_Types::CDF_TIME_TT2000: // tt2000
            return to_attr_data_entry<cdf::CDF_Types::CDF_TIME_TT2000>(buffer);
            break;
        default:
            throw std::invalid_argument { "Unsuported CDF Type" };
            break;
    }
}

data_t to_attr_data_entry(const string_or_buffer_t& value, CDF_Types data_type)
{
    return visit(
        value,
        [data_type](const py::buffer& buffer) { return to_attr_data_entry(buffer, data_type); },
        [](const std::vector<tt2000_t>& values) { return time_to_data_t<tt2000_t>(values); },
        [](const std::vector<epoch>& values) { return time_to_data_t<epoch>(values); },
        [](const std::vector<epoch16>& values) { return time_to_data_t<epoch16>(values); },
        [data_type](const std::string& values) { return to_attr_data_entry(values, data_type); });
}

Attribute::attr_data_t to_attr_data_entries(
    const std::vector<string_or_buffer_t>& values, const std::vector<CDF_Types>& cdf_types)
{
    Attribute::attr_data_t attr_values {};
    std::transform(std::cbegin(values), std::cend(values), std::cbegin(cdf_types),
        std::back_inserter(attr_values),
        static_cast<data_t (*)(const string_or_buffer_t&, CDF_Types)>(to_attr_data_entry));
    return attr_values;
}

[[nodiscard]] Attribute& add_attribute(CDF& cdf, const std::string& name,
    const std::vector<string_or_buffer_t>& values, const std::vector<CDF_Types>& cdf_types)
{
    auto [it, success]
        = cdf.attributes.emplace(name, name, to_attr_data_entries(values, cdf_types));
    if (success)
    {
        Attribute& attr = it->second;
        return attr;
    }
    else
    {
        throw std::invalid_argument { "Attribute already exists" };
    }
}

[[nodiscard]] VariableAttribute& add_attribute(
    Variable& var, const std::string& name, const string_or_buffer_t& values, CDF_Types cdf_types)
{
    auto [it, success] = var.attributes.emplace(
        name, name, VariableAttribute::attr_data_t { to_attr_data_entry(values, cdf_types) });
    if (success)
    {
        VariableAttribute& attr = it->second;
        return attr;
    }
    else
    {
        throw std::invalid_argument { "Attribute already exists" };
    }
}

void set_vattr_value(VariableAttribute& attr, const string_or_buffer_t& value, CDF_Types cdf_type)
{
    attr = to_attr_data_entry(value, cdf_type);
}

void set_attr_values(Attribute& attr, const std::vector<string_or_buffer_t>& values,
    const std::vector<CDF_Types>& cdf_types)
{
    attr = to_attr_data_entries(values, cdf_types);
}

template <typename T>
void def_attribute_wrapper(T& mod)
{
    py::class_<Attribute>(mod, "Attribute")
        .def_property_readonly("name", [](Attribute& attr) { return attr.name; })
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", __repr__<Attribute>)
        .def("_set_values", set_attr_values, py::arg("values").noconvert(), py::arg("data_type"))
        .def(
            "_set_values", [](Attribute& att, const Attribute& source) { att.set_data(source); },
            py::arg("source"))
        .def(
            "__getitem__",
            [](Attribute& att, std::size_t index) -> py_cdf_attr_data_t
            {
                if (index >= att.size())
                    throw std::out_of_range(
                        "Trying to get an attribute value outside of its range");
                return to_py_cdf_data(att[index]);
            },
            py::return_value_policy::copy)
        .def("__len__", [](const Attribute& att) { return att.size(); })
        .def("type",
            [](Attribute& att, std::size_t index)
            {
                if (index >= att.size())
                    throw std::out_of_range(
                        "Trying to get an attribute value outside of its range");
                return att[index].type();
            });

    py::class_<VariableAttribute>(mod, "VariableAttribute")
        .def_property_readonly("name", [](VariableAttribute& attr) { return attr.name; })
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", __repr__<VariableAttribute>)
        .def("_set_value", set_vattr_value, py::arg("value").noconvert(), py::arg("data_type"))
        .def(
            "_set_value", [](VariableAttribute& att, const VariableAttribute& source)
            { att.set_data(source); }, py::arg("source"))
        .def(
            "__getitem__",
            [](VariableAttribute& att, std::size_t index) -> py_cdf_attr_data_t
            {
                if (index != 0)
                    throw std::out_of_range(
                        "Trying to get an attribute value outside of its range");
                return to_py_cdf_data(*att);
            },
            py::return_value_policy::copy)
        .def("__len__", [](const VariableAttribute&) { return 1; })
        .def_property_readonly("value",
            [](VariableAttribute& att) -> py_cdf_attr_data_t { return to_py_cdf_data(*att); })
        .def("type", [](VariableAttribute& att) { return att.type(); });
}
