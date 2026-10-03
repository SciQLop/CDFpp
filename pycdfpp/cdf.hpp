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
#include <cdfpp/attribute.hpp>
#include <cdfpp/cdf-file.hpp>
#include <cdfpp/cdf-io/loading/loading.hpp>
#include <cdfpp/cdf-io/saving/saving.hpp>
#include <cpp_utils/containers/no_init_vector.hpp>
using cpp_utils::containers::no_init_vector;
#include <cdfpp_config.h>

#include "attribute.hpp"
#include "repr.hpp"
#include "variable.hpp"

#include <fmt/core.h>


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
majority: Majority
    file majority: values are always given in row major order, saving converts them
declared_variable_attributes: list of str
    the variable attributes the file declares, in their order, also those no variable uses;
    saving declares them first, then the other ones in the order the variables use them; it is
    a copy: assign a new list to change it
checksum: Checksum
    whether the file ends with the MD5 digest of the rest of it (md5_checksum); loaded files
    keep theirs, the digest is not checked when loading
encoding: Encoding
    byte order of the values in the file (IBMPC is little endian, network big endian); new
    files use the host's, loaded files keep theirs, saving converts the values
distribution_version: int
    file distribution version
lazy_loaded: bool
    file lazy loading state
compression: CompressionType
    file compression type
compression_level: int
    GZIP compression level, from 1 to 9 (default 6), ignored by other compression types

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
        .def(py::self == py::self, py::call_guard<py::gil_scoped_release>())
        .def(py::self != py::self, py::call_guard<py::gil_scoped_release>())
        .def(
            "__copy__", [](const CDF& cdf) -> CDF { return CDF(cdf); },
            py::call_guard<py::gil_scoped_release>(),
            py::return_value_policy::move)
        .def(
            "__deepcopy__", [](const CDF& cdf, const py::dict&) -> CDF { return CDF(cdf); },
            py::call_guard<py::gil_scoped_release>(),
            py::return_value_policy::move)
        .def_readonly(
            "attributes", &CDF::attributes, py::return_value_policy::reference_internal)
        .def_property(
            "majority", [](const CDF& cdf) { return cdf.majority; },
            [](CDF& cdf, cdf_majority majority) { cdf.majority = majority; })
        .def_readwrite("declared_variable_attributes", &CDF::declared_variable_attributes)
        .def_property(
            "checksum", [](const CDF& cdf) { return cdf.checksum; },
            [](CDF& cdf, cdf_checksum checksum) { cdf.checksum = checksum; })
        .def_property(
            "encoding", [](const CDF& cdf) { return cdf.encoding; },
            [](CDF& cdf, cdf_encoding encoding)
            {
                if (!has_ieee_floats(encoding))
                    throw std::invalid_argument { fmt::format(
                        "CDFpp can't write floats in {} encoding: its floats aren't IEEE 754",
                        cdf_encoding_str(encoding)) };
                cdf.encoding = encoding;
            })
        .def_property_readonly(
            "distribution_version", [](const CDF& cdf) { return cdf.distribution_version; })
        .def_property_readonly("lazy_loaded", [](const CDF& cdf) { return cdf.lazy_loaded; })
        .def_property(
            "compression", [](const CDF& cdf) { return cdf.compression; },
            [](CDF& cdf, cdf_compression_type ct) { cdf.compression = ct; })
        .def_property(
            "compression_level", [](const CDF& cdf) { return cdf.compression_level; },
            [](CDF& cdf, int32_t level) { cdf.compression_level = checked_gzip_level(level); })
        .def("__repr__", __repr__<CDF>)
        .def(
            "__getitem__",
            [](CDF& cd, const std::string& key) -> Variable&
            {
                auto it = cd.variables.find(key);
                if (it == cd.variables.end())
                    throw py::key_error(key);
                return it->second;
            },
            py::return_value_policy::reference_internal)
        .def("__contains__",
            [](const CDF& cd, std::string& key) { return cd.variables.count(key) > 0; })
        .def("__iter__", [](const CDF& cd) { return iter_keys(cd.variables); })
        .def("items",
            [](const py::object& self) { return iter_items(self.cast<const CDF&>().variables, self); })
        .def("keys",
            [](const CDF& cd)
            {
                std::vector<std::string> keys(std::size(cd.variables));
                std::transform(std::cbegin(cd.variables), std::cend(cd.variables), std::begin(keys),
                    [](const auto& item) { return item.first; });
                return keys;
            })
        .def("__len__", [](const CDF& cd) { return std::size(cd.variables); })
        .def(
            "_add_variable",
            [](CDF& cdf, const std::string& name, bool is_nrv, cdf_compression_type compression,
                int32_t compression_level) -> Variable&
            {
                if (cdf.variables.count(name) == 0)
                {
                    const auto level = checked_gzip_level(compression_level);
                    cdf.variables.emplace(name, name, std::size(cdf.variables), data_t {},
                        typename Variable::shape_t {}, cdf_majority::row, is_nrv, compression);
                    auto& var = cdf[name];
                    var.set_compression_level(level);
                    return var;
                }
                else
                {
                    throw std::invalid_argument { fmt::format(
                        "Variable '{}' already exists", name) };
                }
            },
            py::arg("name"), py::arg("is_nrv") = false,
            py::arg("compression") = cdf_compression_type::no_compression,
            py::arg("compression_level") = default_gzip_level,
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
                    throw std::invalid_argument { fmt::format(
                        "Variable '{}' already exists", name) };
                }
            },
            py::arg("name"), py::arg("values").noconvert(), py::arg("data_type"),
            py::arg("is_nrv") = false,
            py::arg("compression") = cdf_compression_type::no_compression,
            py::return_value_policy::reference_internal)
        .def(
            "_add_variable",
            [](CDF& cdf, const Variable& var)
            {
                if (cdf.variables.count(var.name()) == 0)
                {
                    cdf.variables.emplace(var.name(), var);
                    return cdf[var.name()];
                }
                else
                {
                    throw std::invalid_argument { fmt::format(
                        "Variable '{}' already exists", var.name()) };
                }
            },
            py::arg("variable"), py::return_value_policy::reference_internal)
        .def(
            "_remove_variable",
            [](CDF& cdf, const std::string& name)
            {
                auto it = cdf.variables.find(name);
                if (it != cdf.variables.end())
                {
                    cdf.variables.erase(it);
                }
                else
                {
                    throw std::invalid_argument { fmt::format(
                        "Variable '{}' does not exist", name) };
                }
            },
            py::arg("name"))
        .def("_add_attribute",
            static_cast<Attribute& (*)(CDF&, const std::string&,
                const std::vector<string_or_buffer_t>&, const std::vector<CDF_Types>&)>(
                add_attribute),
            py::arg { "name" }, py::arg { "entries_values" }, py::arg { "entries_types" },
            py::return_value_policy::reference_internal)
        .def(
            "_add_attribute",
            [](CDF& cdf, const Attribute& attr)
            {
                if (cdf.attributes.count(attr.name) == 0)
                {
                    cdf.attributes.emplace(attr.name, attr);
                    return cdf.attributes[attr.name];
                }
                else
                {
                    throw std::invalid_argument { fmt::format(
                        "Global attribute '{}' already exists", attr.name) };
                }
            },
            py::arg("attribute"), py::return_value_policy::reference_internal)
        .def(
            "_remove_attribute",
            [](CDF& cdf, const std::string& name)
            {
                auto it = cdf.attributes.find(name);
                if (it != cdf.attributes.end())
                {
                    cdf.attributes.erase(it);
                }
                else
                {
                    throw std::invalid_argument { fmt::format(
                        "Global attribute '{}' does not exist", name) };
                }
            },
            py::arg("name"));
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
            if (info.ndim != 1 or info.strides[0] != info.itemsize)
                throw std::invalid_argument(fmt::format(
                    "Loading from memory needs contiguous 1-D bytes, got ndim={}", info.ndim));
            py::gil_scoped_release release;
            return io::load(static_cast<char*>(info.ptr),
                static_cast<std::size_t>(info.size * info.itemsize), iso_8859_1_to_utf8, true);
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
            })
        .def("__len__", [](const cdf_bytes& b) { return std::size(b.data); });

    mod.def(
        "save",
        [](const CDF& cdf)
        {
            py::gil_scoped_release release;
            return cdf_bytes { io::save(cdf) };
        },
        py::arg("cdf"));
}
