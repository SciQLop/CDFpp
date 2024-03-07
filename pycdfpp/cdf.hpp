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
#include <cdfpp/cdf-file.hpp>
#include <cdfpp/cdf-io/loading/loading.hpp>
#include <cdfpp/cdf-io/saving/saving.hpp>
#include <cdfpp/no_init_vector.hpp>
#include <cdfpp_config.h>

#include "attribute.hpp"
#include "repr.hpp"
#include "variable.hpp"


using namespace cdf;

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;
namespace docstrings
{
constexpr auto _CDF = R"delimiter(
A CDF file object.

Attributes
----------
attributes: dict
    file attributes
variables: dict
    file variables
majority: cdf_majority
    file majority
distribution_version: int
    file distribution version
lazy_loaded: bool
    file lazy loading state
compression: CompressionType
    file compression type

Methods
-------
add_attribute
    Adds an attribute to the file. Raises an exception if the attribute already exists.
add_variable
    Adds a variable to the file. Raises an exception if the variable already exists.

)delimiter";

}

template <typename T>
void def_cdf_wrapper(T& mod)
{
    py::class_<CDF>(mod, "CDF", docstrings::_CDF)
        .def(py::init<>())
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def_readonly("attributes", &CDF::attributes, py::return_value_policy::reference,
            py::keep_alive<0, 1>())
        .def_property_readonly("majority", [](const CDF& cdf) { return cdf.majority; })
        .def_property_readonly(
            "distribution_version", [](const CDF& cdf) { return cdf.distribution_version; })
        .def_property_readonly("lazy_loaded", [](const CDF& cdf) { return cdf.lazy_loaded; })
        .def_property(
            "compression", [](const CDF& cdf) { return cdf.compression; },
            [](CDF& cdf, cdf_compression_type ct) { cdf.compression = ct; })
        .def("__repr__", __repr__<CDF>)
        .def(
            "__getitem__", [](CDF& cd, const std::string& key) -> Variable& { return cd[key]; },
            py::return_value_policy::reference_internal)
        .def("__contains__",
            [](const CDF& cd, std::string& key) { return cd.variables.count(key) > 0; })
        .def(
            "__iter__",
            [](const CDF& cd)
            { return py::make_key_iterator(std::begin(cd.variables), std::end(cd.variables)); },
            py::keep_alive<0, 1>())
        .def(
            "items",
            [](const CDF& cd)
            { return py::make_iterator(std::begin(cd.variables), std::end(cd.variables)); },
            py::keep_alive<0, 1>())
        .def("__len__", [](const CDF& cd) { return std::size(cd.variables); })
        .def(
            "_add_variable",
            [](CDF& cdf, const std::string& name, bool is_nrv,
                cdf_compression_type compression) -> Variable&
            {
                if (cdf.variables.count(name) == 0)
                {
                    cdf.variables.emplace(name, name, std::size(cdf.variables), data_t {},
                        typename Variable::shape_t {}, cdf_majority::row, is_nrv, compression);
                    auto& var = cdf[name];
                    return var;
                }
                else
                {
                    throw std::invalid_argument { "Variable already exists" };
                }
            },
            py::arg("name"), py::arg("is_nrv") = false,
            py::arg("compression") = cdf_compression_type::no_compression,
            py::return_value_policy::reference_internal)
        .def(
            "_add_variable",
            [](CDF& cdf, const std::string& name, const py::buffer& buffer, CDF_Types data_type,
                bool is_nrv, cdf_compression_type compression) -> Variable&
            {
                if (cdf.variables.count(name) == 0)
                {
                    cdf.variables.emplace(name, name, std::size(cdf.variables), data_t {},
                        typename Variable::shape_t {}, cdf_majority::row, is_nrv, compression);
                    auto& var = cdf[name];
                    set_values(var, buffer, data_type);
                    return var;
                }
                else
                {
                    throw std::invalid_argument { "Variable already exists" };
                }
            },
            py::arg("name"), py::arg("values").noconvert(), py::arg("data_type"),
            py::arg("is_nrv") = false,
            py::arg("compression") = cdf_compression_type::no_compression,
            py::return_value_policy::reference_internal)
        .def("_add_attribute",
            static_cast<Attribute& (*)(CDF&, const std::string&,
                const std::vector<string_or_buffer_t>&, const std::vector<CDF_Types>&)>(
                add_attribute),
            py::arg { "name" }, py::arg { "entries_values" }, py::arg { "entries_types" },
            py::return_value_policy::reference_internal);
}

template <typename T>
void def_cdf_loading_functions(T& mod)
{
    mod.def(
        "load",
        [](py::bytes& buffer, bool iso_8859_1_to_utf8)
        {
            py::buffer_info info(py::buffer(buffer).request());
            py::gil_scoped_release release;
            return io::load(static_cast<char*>(info.ptr), static_cast<std::size_t>(info.size),
                iso_8859_1_to_utf8);
        },
        py::arg("buffer"), py::arg("iso_8859_1_to_utf8") = false, py::return_value_policy::move);

    mod.def(
        "lazy_load",
        [](py::buffer& buffer, bool iso_8859_1_to_utf8)
        {
            py::buffer_info info(buffer.request());
            if (info.ndim != 1)
                throw std::runtime_error("Incompatible buffer dimension!");
            py::gil_scoped_release release;
            return io::load(static_cast<char*>(info.ptr), info.shape[0], iso_8859_1_to_utf8, true);
        },
        py::arg("buffer"), py::arg("iso_8859_1_to_utf8") = false, py::return_value_policy::move,
        py::keep_alive<0, 1>());

    mod.def(
        "load",
        [](const char* fname, bool iso_8859_1_to_utf8, bool lazy_load)
        {
            py::gil_scoped_release release;
            return io::load(std::string { fname }, iso_8859_1_to_utf8, lazy_load);
        },
        py::arg("fname"), py::arg("iso_8859_1_to_utf8") = false, py::arg("lazy_load") = true,
        py::return_value_policy::move);
}

struct cdf_bytes
{
    no_init_vector<char> data;
};

template <typename T>
void def_cdf_saving_functions(T& mod)
{

    mod.def(
        "save",
        [](const CDF& cdf, const char* fname)
        {
            py::gil_scoped_release release;
            return io::save(cdf, std::string { fname });
        },
        py::arg("cdf"), py::arg("fname"));


    py::class_<cdf_bytes>(mod, "_cdf_bytes", py::buffer_protocol())
        .def_buffer(
            [](cdf_bytes& b) -> py::buffer_info
            {
                py::gil_scoped_release release;
                return py::buffer_info(b.data.data(), std::size(b.data), true);
            });

    mod.def(
        "save",
        [](const CDF& cdf)
        {
            py::gil_scoped_release release;
            return cdf_bytes { io::save(cdf) };
        },
        py::arg("cdf"));
}
