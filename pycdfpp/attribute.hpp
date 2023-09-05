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
#include <cdfpp/attribute.hpp>
#include <cdfpp/cdf-data.hpp>
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
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/operators.h>

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


[[nodiscard]] Attribute& add_attribute(
    CDF& cdf, const std::string& name, std::vector<py_cdf_attr_data_t>& values)
{
    if (cdf.attributes.count(name) == 0)
    {
        typename Attribute::attr_data_t attr_values {};
        for (auto& value : values)
        {
            visit(
                value,
                [&attr_values](std::string& value)
                {
                    no_init_vector<char> v(value.length());
                    std::copy(std::cbegin(value), std::cend(value), std::begin(v));
                    attr_values.emplace_back(std::move(v), CDF_Types::CDF_CHAR);
                },
                [&attr_values](no_init_vector<decltype(std::chrono::system_clock::now())>& value)
                { attr_values.emplace_back(cdf::to_tt2000(value)); },
                [&attr_values](auto& value) { attr_values.emplace_back(std::move(value)); },
                [](std::monostate&) {});
        }
        cdf.attributes.emplace(name, name, std::move(attr_values));
        auto& attr = cdf.attributes[name];
        return attr;
    }
    else
    {
        throw std::invalid_argument { "Attribute already exists" };
    }
}

[[nodiscard]] Attribute& add_attribute(
    Variable& var, const std::string& name, py_cdf_attr_data_t& values)
{
    if (var.attributes.count(name) == 0)
    {
        typename Attribute::attr_data_t attr_values = visit(
            values,
            [](std::string& value)
            {
                no_init_vector<char> v(std::size(value));
                std::copy(std::cbegin(value), std::cend(value), std::begin(v));
                return typename Attribute::attr_data_t { { std::move(v), CDF_Types::CDF_CHAR } };
            },
            [](no_init_vector<decltype(std::chrono::system_clock::now())>& value)
            {
                return typename Attribute::attr_data_t { { cdf::to_tt2000(value),
                    CDF_Types::CDF_TIME_TT2000 } };
            },
            [](auto& value)
            {
                typename Attribute::attr_data_t data {};
                data.emplace_back(std::move(value));
                return data;
            },
            [](std::monostate&) { return typename Attribute::attr_data_t {}; });

        var.attributes.emplace(name, name, std::move(attr_values));
        auto& attr = var.attributes[name];
        return attr;
    }
    else
    {
        throw std::invalid_argument { "Attribute already exists" };
    }
}

template <typename T>
void def_attribute_wrapper(T& mod)
{
    py::class_<Attribute>(mod, "Attribute")
        .def_property_readonly("name", [](Attribute& attr) { return attr.name; })
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", __repr__<Attribute>)
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
        .def("__len__", [](const Attribute& att) { return att.size(); });
}
