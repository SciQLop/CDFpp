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
#include <cdfpp/cdf-map.hpp>
#include <cdfpp/no_init_vector.hpp>
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
#include <pybind11/stl_bind.h>

PYBIND11_MAKE_OPAQUE(cdf_map<std::string, Attribute>);
PYBIND11_MAKE_OPAQUE(cdf_map<std::string, Variable>);

namespace py = pybind11;

#ifdef CDFpp_USE_NOMAP
template <typename T1, typename T2>
class pybind11::detail::type_caster<nomap_node<T1, T2>>
        : public pybind11::detail::tuple_caster<nomap_node, T1, T2>
{
};
#endif

using py_cdf_attr_data_t = std::variant<std::string, no_init_vector<char>, no_init_vector<uint8_t>,
    no_init_vector<uint16_t>, no_init_vector<uint32_t>, no_init_vector<int8_t>, no_init_vector<int16_t>,
    no_init_vector<int32_t>, no_init_vector<int64_t>, no_init_vector<float>, no_init_vector<double>,
    no_init_vector<tt2000_t>, no_init_vector<epoch>, no_init_vector<epoch16>>;

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

template <typename T1, typename T2, typename T3>
auto def_cdf_map(T3& mod, const char* name)
{
    return py::class_<cdf_map<T1, T2>>(mod, name)
        .def("__repr__", __repr__<cdf_map<T1, T2>>)
        .def(
            "__getitem__", [](cdf_map<T1, T2>& m, const std::string& key) -> T2& { return m[key]; },
            py::return_value_policy::reference_internal)
        .def("__contains__",
            [](const cdf_map<T1, T2>& m, std::string& key) { return m.count(key) > 0; })
        .def(
            "__iter__",
            [](const cdf_map<T1, T2>& m)
            { return py::make_key_iterator(std::begin(m), std::end(m)); },
            py::keep_alive<0, 1>())
        .def(
            "items",
            [](const cdf_map<T1, T2>& m) { return py::make_iterator(std::begin(m), std::end(m)); },
            py::keep_alive<0, 1>())
        .def("keys",
            [](const cdf_map<T1, T2>& m)
            {
                std::vector<T1> keys(std::size(m));
                std::transform(std::cbegin(m), std::cend(m), std::begin(keys),
                    [](const auto& item) { return item.first; });
                return keys;
            })
        .def("__len__", [](const cdf_map<T1, T2>& m) { return std::size(m); });
}

PYBIND11_MODULE(cdfpp_wrapper, m)
{
    m.doc() = "cdfpp_wrapper module";
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
    // py::bind_map<cdf_map<std::string, Variable>>(m,"VariablesMap");
    // py::bind_map<cdf_map<std::string, Attribute>>(m,"AttributeMap");

    def_cdf_map<std::string, Variable>(m, "VariablesMap");
    def_cdf_map<std::string, Attribute>(m, "AttributeMap");

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

    py::enum_<cdf_compression_type>(m, "CDF_compression_type")
        .value("no_compression", cdf_compression_type::no_compression)
        .value("gzip_compression", cdf_compression_type::gzip_compression)
        .value("rle_compression", cdf_compression_type::rle_compression)
        .value("ahuff_compression", cdf_compression_type::ahuff_compression)
        .value("huff_compression", cdf_compression_type::huff_compression);

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
        .def_property_readonly("lazy_loaded", [](const CDF& cdf) { return cdf.lazy_loaded; })
        .def_property_readonly("compression", [](const CDF& cdf) { return cdf.compression; })
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
        .def_property_readonly("values_loaded", &Variable::values_loaded)
        .def_buffer([](Variable& var) -> py::buffer_info { return make_buffer(var); })
        .def_property_readonly(
            "values", make_values_view, py::return_value_policy::reference_internal);

    m.def(
        "load",
        [](py::bytes& buffer, bool iso_8859_1_to_utf8)
        {
            py::buffer_info info(py::buffer(buffer).request());
            return io::load(static_cast<char*>(info.ptr), static_cast<std::size_t>(info.size),
                iso_8859_1_to_utf8);
        },
        py::arg("buffer"), py::arg("iso_8859_1_to_utf8") = false,
        py::return_value_policy::take_ownership);

    m.def(
        "lazy_load",
        [](py::buffer& buffer, bool iso_8859_1_to_utf8)
        {
            py::buffer_info info(buffer.request());
            if (info.ndim != 1)
                throw std::runtime_error("Incompatible buffer dimension!");
            return io::load(static_cast<char*>(info.ptr), info.shape[0], iso_8859_1_to_utf8, true);
        },
        py::arg("buffer"), py::arg("iso_8859_1_to_utf8") = false,
        py::return_value_policy::take_ownership, py::keep_alive<0, 1>());

    m.def(
        "load",
        [](const char* fname, bool iso_8859_1_to_utf8, bool lazy_load)
        { return io::load(std::string { fname }, iso_8859_1_to_utf8, lazy_load); },
        py::arg("fname"), py::arg("iso_8859_1_to_utf8") = false, py::arg("lazy_load") = true,
        py::return_value_policy::take_ownership);
}
