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
#include <cdfpp/attribute.hpp>
#include <cdfpp/cdf-map.hpp>
#include <cdfpp/cdf-repr.hpp>
#include <cdfpp/no_init_vector.hpp>
#include <cdfpp/variable.hpp>
#include <cdfpp_config.h>

#include "attribute.hpp"
#include "cdf.hpp"
#include "chrono.hpp"
#include "enums.hpp"
#include "repr.hpp"
#include "variable.hpp"

using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/iostream.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <fmt/ranges.h>

PYBIND11_MAKE_OPAQUE(cdf_map<std::string, Attribute>);
PYBIND11_MAKE_OPAQUE(cdf_map<std::string, VariableAttribute>);
PYBIND11_MAKE_OPAQUE(cdf_map<std::string, Variable>);

namespace py = pybind11;

#ifdef CDFpp_USE_NOMAP
template <typename T1, typename T2>
class pybind11::detail::type_caster<nomap_node<T1, T2>>
        : public pybind11::detail::tuple_caster<nomap_node, T1, T2>
{
};
#endif

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
            "__iter__", [](const cdf_map<T1, T2>& m)
            { return py::make_key_iterator(std::begin(m), std::end(m)); }, py::keep_alive<0, 1>())
        .def(
            "items", [](const cdf_map<T1, T2>& m)
            { return py::make_iterator(std::begin(m), std::end(m)); }, py::keep_alive<0, 1>())
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

PYBIND11_MODULE(_pycdfpp, m, py::mod_gil_not_used())
{
    m.doc() = R"pbdoc(
        _pycdfpp
        --------

    )pbdoc";

    m.attr("__version__") = CDFPP_VERSION;

    def_enums_wrappers(m);
    def_time_types_wrapper(m);

    def_cdf_map<std::string, Variable>(m, "VariablesMap");
    def_cdf_map<std::string, Attribute>(m, "AttributeMap");
    def_cdf_map<std::string, VariableAttribute>(m, "VariableAttributeMap");


    def_attribute_wrapper(m);
    def_variable_wrapper(m);
    def_time_conversion_functions(m);
    def_cdf_wrapper(m);

    def_cdf_loading_functions(m);
    def_cdf_saving_functions(m);
    m.def("_buffer_info",
        [](py::buffer& buff) -> std::string
        {
            auto info = buff.request();
            return fmt::format(R"(
format = {}
itemsize = {}
size = {}
ndim = {}
shape = [{}]
strides = [{}]
 )",
                info.format, info.itemsize, info.size, info.ndim, fmt::join(info.shape, ", "),
                fmt::join(info.strides, ", "));
        });
}
