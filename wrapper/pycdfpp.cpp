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
#include "repr.hpp"
#include <cdf-chrono.hpp>
#include <cdf-data.hpp>
#include <cdf.hpp>
using namespace cdf;

#include <pybind11/chrono.h>
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

py_cdf_attr_data_t to_py_cdf_data(const cdf::data_t& data)
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
        case cdf::CDF_Types::CDF_UCHAR:
        {
            auto v = data.get<unsigned char>();
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
            break;
    }
    return {};
}

py::object to_datetime64(py::array_t<epoch> input)
{
    using namespace pybind11::literals;
    static_assert(
        std::is_same_v<std::chrono::nanoseconds, decltype(cdf::to_time_point(epoch {}))::duration>,
        " Expecting nanoseconds from cdf::to_time_point function");

    py::buffer_info in_buff = input.request();
    auto result = py::array_t<uint64_t>(in_buff.shape);
    py::buffer_info res_buff = result.request(true);
    epoch* in_ptr = static_cast<epoch*>(in_buff.ptr);
    int64_t* res_ptr = static_cast<int64_t*>(res_buff.ptr);

    for (py::ssize_t idx = 0; idx < in_buff.shape[0]; idx++)
        res_ptr[idx] = cdf::to_time_point(in_ptr[idx]).time_since_epoch().count();
    return py::cast(&result).attr("astype")("datetime64[ns]");
}

}

PYBIND11_MODULE(pycdfpp, m)
{
    m.doc() = "pycdfpp module";

    PYBIND11_NUMPY_DTYPE(tt2000_t, value);
    PYBIND11_NUMPY_DTYPE(epoch, value);
    PYBIND11_NUMPY_DTYPE(epoch16, seconds, picoseconds);

    py::class_<tt2000_t>(m, "tt2000");
    py::class_<epoch>(m, "epoch").def("to_datetime", [](const epoch&) {});
    py::class_<epoch16>(m, "epoch16");

    m.def("to_datetime64", to_datetime64);

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
        .def_readonly("attributes", &CDF::attributes, py::return_value_policy::reference)
        .def("__repr__", __repr__<CDF>)
        .def(
            "__getitem__", [](CDF& cd, const std::string& key) -> Variable& { return cd[key]; },
            py::return_value_policy::reference)
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
        .def("__getitem__",
            [](Attribute& att, std::size_t index) -> py_cdf_attr_data_t
            {
                if (index >= att.size())
                    throw std::out_of_range(
                        "Trying to get an attribute value outside of its range");
                return to_py_cdf_data(att[index]);
            })
        .def("__len__", [](const Attribute& att) { return att.size(); });

    py::class_<Variable>(m, "Variable", py::buffer_protocol())
        .def("__repr__", __repr__<Variable>)
        .def_readonly("attributes", &Variable::attributes, py::return_value_policy::reference)
        .def_property_readonly("name", &Variable::name)
        .def_property_readonly("type", &Variable::type)
        .def_property_readonly("shape", &Variable::shape)
        .def_buffer([](Variable& var) -> py::buffer_info { return make_buffer(var); })
        .def_property_readonly(
            "values", make_values_view, py::return_value_policy::reference_internal);

    m.def(
        "load", [](const char* name) { return io::load(name); },
        py::return_value_policy::reference);

    m.def("to_tt2000",
        [](decltype(std::chrono::system_clock::now()) tp) { return cdf::to_tt2000(tp); });
    m.def("to_timepoint", [](cdf::tt2000_t epoch) { return cdf::to_time_point(epoch); });
    m.def("tt2000_value", [](const cdf::tt2000_t& epoch) { return epoch.value; });
}
