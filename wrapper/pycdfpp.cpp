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
#include "buffers.hpp"
#include "chrono.hpp"
#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf-repr.hpp>
#include <cdfpp/cdf.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
#include <cdfpp_config.h>
using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/iostream.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

using py_cdf_attr_data_t = std::variant<std::string, std::vector<char>, std::vector<uint8_t>,
    std::vector<uint16_t>, std::vector<uint32_t>, std::vector<int8_t>, std::vector<int16_t>,
    std::vector<int32_t>, std::vector<int64_t>, std::vector<float>, std::vector<double>,
    std::vector<tt2000_t>, std::vector<epoch>, std::vector<epoch16>>;

namespace
{

inline py_cdf_attr_data_t to_py_cdf_data(const cdf::data_t& data)
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


}

PYBIND11_MODULE(pycdfpp, m)
{
    m.doc() = "pycdfpp module";
    m.attr("__version__") = CDFPP_VERSION;

    py::class_<tt2000_t>(m, "tt2000_t")
        .def_readwrite("value", &tt2000_t::value)
        .def("__repr__", __repr__<tt2000_t>);
    py::class_<epoch>(m, "epoch")
        .def_readwrite("value", &epoch::value)
        .def("__repr__", __repr__<epoch>);
    py::class_<epoch16>(m, "epoch16")
        .def_readwrite("seconds", &epoch16::seconds)
        .def_readwrite("picoseconds", &epoch16::picoseconds)
        .def("__repr__", __repr__<epoch16>);

    PYBIND11_NUMPY_DTYPE(tt2000_t, value);
    PYBIND11_NUMPY_DTYPE(epoch, value);
    PYBIND11_NUMPY_DTYPE(epoch16, seconds, picoseconds);

    m.def("to_datetime64", array_to_datetime64<epoch>, py::arg {}.noconvert());
    m.def("to_datetime64", array_to_datetime64<epoch16>, py::arg {}.noconvert());
    m.def("to_datetime64", array_to_datetime64<tt2000_t>, py::arg {}.noconvert());
    m.def("to_datetime64", scalar_to_datetime64<epoch>);
    m.def("to_datetime64", scalar_to_datetime64<epoch16>);
    m.def("to_datetime64", scalar_to_datetime64<tt2000_t>);
    m.def("to_datetime64", vector_to_datetime64<epoch>, py::arg {}.noconvert());
    m.def("to_datetime64", vector_to_datetime64<epoch16>, py::arg {}.noconvert());
    m.def("to_datetime64", vector_to_datetime64<tt2000_t>, py::arg {}.noconvert());
    m.def("to_datetime64", var_to_datetime64);

    m.def("to_datetime", vector_to_datetime<epoch>);
    m.def("to_datetime", vector_to_datetime<epoch16>);
    m.def("to_datetime", vector_to_datetime<tt2000_t>);
    m.def("to_datetime",
        static_cast<decltype(to_time_point(epoch {})) (*)(const epoch&)>(to_time_point));
    m.def("to_datetime",
        static_cast<decltype(to_time_point(epoch16 {})) (*)(const epoch16&)>(to_time_point));
    m.def("to_datetime",
        static_cast<decltype(to_time_point(tt2000_t {})) (*)(const tt2000_t&)>(to_time_point));

    m.def("to_datetime", var_to_datetime);

    m.def("to_tt2000",
        [](decltype(std::chrono::system_clock::now()) tp) { return cdf::to_tt2000(tp); });

    m.def("to_epoch",
        [](decltype(std::chrono::system_clock::now()) tp) { return cdf::to_epoch(tp); });

    m.def("to_epoch16",
        [](decltype(std::chrono::system_clock::now()) tp) { return cdf::to_epoch16(tp); });


    py::enum_<cdf_majority>(m, "CDF_majority")
        .value("row", cdf_majority::row)
        .value("column", cdf_majority::column);

    py::enum_<CDF_Types>(m, "CDF_Types")
        .value("CDF_BYTE", CDF_Types::CDF_BYTE)
        .value("CDF_CHAR", CDF_Types::CDF_CHAR)
        .value("CDF_INT1", CDF_Types::CDF_INT1)
        .value("CDF_INT2", CDF_Types::CDF_INT2)
        .value("CDF_INT4", CDF_Types::CDF_INT4)
        .value("CDF_INT8", CDF_Types::CDF_INT8)
        .value("CDF_NONE", CDF_Types::CDF_NONE)
        .value("CDF_EPOCH", CDF_Types::CDF_EPOCH)
        .value("CDF_FLOAT", CDF_Types::CDF_FLOAT)
        .value("CDF_REAL4", CDF_Types::CDF_REAL4)
        .value("CDF_REAL8", CDF_Types::CDF_REAL8)
        .value("CDF_UCHAR", CDF_Types::CDF_UCHAR)
        .value("CDF_UINT1", CDF_Types::CDF_UINT1)
        .value("CDF_UINT2", CDF_Types::CDF_UINT2)
        .value("CDF_UINT4", CDF_Types::CDF_UINT4)
        .value("CDF_DOUBLE", CDF_Types::CDF_DOUBLE)
        .value("CDF_EPOCH16", CDF_Types::CDF_EPOCH16)
        .value("CDF_TIME_TT2000", CDF_Types::CDF_TIME_TT2000)
        .export_values();

    py::class_<CDF>(m, "CDF")
        .def_readonly("attributes", &CDF::attributes, py::return_value_policy::reference,
            py::keep_alive<0, 1>())
        .def_property_readonly("majority", [](const CDF& cdf) { return cdf.majority; })
        .def_property_readonly(
            "distribution_version", [](const CDF& cdf) { return cdf.distribution_version; })
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
        .def("__len__", [](const CDF& cd) { return std::size(cd.variables); });

    py::class_<Attribute>(m, "Attribute")
        .def_property_readonly("name", [](Attribute& attr) { return attr.name; })
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
            py::return_value_policy::reference_internal)
        .def("__len__", [](const Attribute& att) { return att.size(); });

    py::class_<Variable>(m, "Variable", py::buffer_protocol())
        .def("__repr__", __repr__<Variable>)
        .def_readonly("attributes", &Variable::attributes, py::return_value_policy::reference)
        .def_property_readonly("name", &Variable::name)
        .def_property_readonly("type", &Variable::type)
        .def_property_readonly("shape", &Variable::shape)
        .def_property_readonly("majority", &Variable::majority)
        .def_buffer([](Variable& var) -> py::buffer_info { return make_buffer(var); })
        .def_property_readonly("values", make_values_view,
            py::return_value_policy::reference_internal);

    m.def(
        "load",
        [](py::bytes& buffer, bool iso_8859_1_to_utf8)
        {
            py::buffer_info info(py::buffer(buffer).request());
            return io::load(static_cast<char*>(info.ptr), static_cast<std::size_t>(info.size),
                iso_8859_1_to_utf8);
        },
        py::arg("buffer"), py::arg("iso_8859_1_to_utf8") = false,
        py::return_value_policy::reference);

    m.def(
        "load",
        [](const char* fname, bool iso_8859_1_to_utf8)
        { return io::load(std::string { fname }, iso_8859_1_to_utf8); },
        py::arg("fname"), py::arg("iso_8859_1_to_utf8") = false,
        py::return_value_policy::reference);
}
