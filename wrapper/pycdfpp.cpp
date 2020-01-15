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
#include <cdf.hpp>
using namespace cdf;

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

using py_array = std::variant<py::array_t<double>, py::array_t<float>, py::array_t<uint32_t>,
    py::array_t<uint16_t>, py::array_t<uint8_t>, py::array_t<int64_t>, py::array_t<int32_t>,
    py::array_t<int16_t>, py::array_t<int8_t>, py::array_t<char>>;

namespace
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
        [bw = sizeof(T), next = sizeof(T)](auto v) mutable {
            auto res = next;
            next = static_cast<ssize_t>(v * next);
            return res;
        });
    std::reverse(std::begin(res), std::end(res)); // if column major
    return res;
}

template <typename T, typename U = T>
py::buffer_info impl_make_buffer(cdf::Variable& var)
{
    return py::buffer_info(var.get<U>().data(), /* Pointer to buffer */
        sizeof(T), /* Size of one scalar */
        py::format_descriptor<T>::format(), std::size(var.shape()), /* Number of dimensions */
        shape_ssize_t(var), strides<T>(var));
}

py::buffer_info make_buffer(cdf::Variable& var)
{
#define BUFFER_LAMBDA(type) [&var](const std::vector<type>&) { return impl_make_buffer<type>(var); }

    return cdf::visit(
        var, BUFFER_LAMBDA(double), BUFFER_LAMBDA(float),

        BUFFER_LAMBDA(uint32_t), BUFFER_LAMBDA(uint16_t), BUFFER_LAMBDA(uint8_t),

        BUFFER_LAMBDA(int64_t), BUFFER_LAMBDA(int32_t), BUFFER_LAMBDA(int16_t),
        BUFFER_LAMBDA(int8_t),

        BUFFER_LAMBDA(char),

        [&var](const std::vector<tt2000_t>&) { return impl_make_buffer<int64_t, tt2000_t>(var); },
        [&var](const std::vector<epoch>&) { return impl_make_buffer<double, epoch>(var); },

        [](const std::vector<epoch16>&) { return py::buffer_info {}; },
        [](const std::string&) { return py::buffer_info {}; },
        [](const cdf_none&) { return py::buffer_info {}; });
}

py_array make_array(cdf::Variable& var)
{
#define ARRAY_T_LAMBDA2(type1, type2)                                                              \
    [&var](const std::vector<type2>&) -> py_array {                                                \
        return py_array { py::array_t<type1>(impl_make_buffer<type1, type2>(var)) };               \
    }
#define ARRAY_T_LAMBDA(type) ARRAY_T_LAMBDA2(type, type)

    return cdf::visit(
        var, ARRAY_T_LAMBDA(double), ARRAY_T_LAMBDA(float),

        ARRAY_T_LAMBDA(uint32_t), ARRAY_T_LAMBDA(uint16_t), ARRAY_T_LAMBDA(uint8_t),

        ARRAY_T_LAMBDA(int64_t), ARRAY_T_LAMBDA(int32_t), ARRAY_T_LAMBDA(int16_t),
        ARRAY_T_LAMBDA(int8_t),

        ARRAY_T_LAMBDA(char),

        ARRAY_T_LAMBDA2(int64_t, tt2000_t), ARRAY_T_LAMBDA2(double, epoch),

        [](const std::vector<epoch16>&) {
            throw;
            return py_array { py::array_t<double> {} };
        },
        [](const std::string&) {
            throw;
            return py_array { py::array_t<double> {} };
        },
        [](const cdf_none&) {
            throw;
            return py_array { py::array_t<double> {} };
        });
}

}

PYBIND11_MODULE(pycdfpp, m)
{
    m.doc() = "pycdfpp module";
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
        .def_readonly("attributes", &CDF::attributes)
        .def("__getitem__", [](CDF& cd, const std::string& key) -> Variable& { return cd[key]; });

    py::class_<Attribute>(m, "Attribute");

    py::class_<Variable>(m, "Variable", py::buffer_protocol())
        .def_property_readonly("name", &Variable::name)
        .def_property_readonly("type", &Variable::type)
        .def_property_readonly("shape", &Variable::shape)
        .def_buffer([](Variable& var) -> py::buffer_info { return make_buffer(var); })
        .def("as_array", [](Variable& var) -> py_array { return make_array(var); });

    m.def("load", [](const char* name) { return io::load(name); });
}
