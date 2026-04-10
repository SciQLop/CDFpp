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
#include "chrono.hpp"
#include "collections.hpp"
#include "data_types.hpp"
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
#include <pybind11/warnings.h>

#include <fmt/core.h>
#include <fmt/ranges.h>

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

template <typename T>
auto to_numerical(const PyObject* o)
{
    if constexpr (std::is_same_v<double, T>)
    {
        if (PyFloat_Check(const_cast<PyObject*>(o)))
        {
            return static_cast<T>(PyFloat_AS_DOUBLE(const_cast<PyObject*>(o)));
        }
        else if (PyLong_Check(const_cast<PyObject*>(o)))
        {
            return static_cast<T>(PyLong_AsDouble(const_cast<PyObject*>(o)));
        }
        else
        {
            throw std::invalid_argument { fmt::format(
                "Expected numeric value, got {}", Py_TYPE(const_cast<PyObject*>(o))->tp_name) };
        }
    }
    if constexpr (helpers::is_any_of_v<T, uint8_t, uint16_t, uint32_t, uint64_t>)
    {
        if (PyLong_Check(const_cast<PyObject*>(o)))
        {
            return static_cast<T>(PyLong_AsUnsignedLongLong(const_cast<PyObject*>(o)));
        }
        else if (PyFloat_Check(const_cast<PyObject*>(o)))
        {
            return static_cast<T>(PyFloat_AsDouble(const_cast<PyObject*>(o)));
        }
        else
        {
            throw std::invalid_argument { fmt::format(
                "Expected numeric value, got {}", Py_TYPE(const_cast<PyObject*>(o))->tp_name) };
        }
    }
    else if constexpr (helpers::is_any_of_v<T, int8_t, int16_t, int32_t, int64_t>)
    {
        if (PyLong_Check(const_cast<PyObject*>(o)))
        {
            return static_cast<T>(PyLong_AsLongLong(const_cast<PyObject*>(o)));
        }
        else if (PyFloat_Check(const_cast<PyObject*>(o)))
        {
            return static_cast<T>(PyFloat_AS_DOUBLE(const_cast<PyObject*>(o)));
        }
        else
        {
            throw std::invalid_argument { fmt::format(
                "Expected numeric value, got {}", Py_TYPE(const_cast<PyObject*>(o))->tp_name) };
        }
    }
    else if constexpr (std::is_same_v<float, T>)
    {
        if (PyFloat_Check(const_cast<PyObject*>(o)))
        {
            return static_cast<T>(PyFloat_AS_DOUBLE(const_cast<PyObject*>(o)));
        }
        else if (PyLong_Check(const_cast<PyObject*>(o)))
        {
            return static_cast<T>(PyLong_AsDouble(const_cast<PyObject*>(o)));
        }
        else
        {
            throw std::invalid_argument { fmt::format(
                "Expected numeric value, got {}", Py_TYPE(const_cast<PyObject*>(o))->tp_name) };
        }
    }
}

template <CDF_Types data_type>
std::pair<data_t, typename Variable::shape_t> _numeric_to_nd_data_t(const py::buffer& buffer)
{
    py::buffer_info info = buffer.request();
    using T = from_cdf_type_t<data_type>;
    if (info.itemsize != static_cast<ssize_t>(sizeof(T)))
        throw std::invalid_argument { fmt::format(
            "Buffer itemsize mismatch for {}: expected {} bytes, got {}",
            cdf_type_str(data_type), sizeof(T), info.itemsize) };
    typename Variable::shape_t shape(info.ndim);
    std::copy(std::cbegin(info.shape), std::cend(info.shape), std::begin(shape));
    if (info.size != 0)
    {
        /*
         * We could later imagine to avoid this copy by directly using the buffer memory
         * this would require to tie the buffer lifetime to the variable lifetime
         */
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
std::pair<data_t, typename Variable::shape_t> _numeric_to_nd_data_t(
    const py_list_or_py_tuple auto& values)
{
    using T = from_cdf_type_t<data_type>;
    auto [data, shape]
        = _details::ranges::transform<no_init_vector<T>, typename Variable::shape_t>(values,
            [](const PyObject* obj) -> T { return to_numerical<T>(const_cast<PyObject*>(obj)); });
    return std::pair<data_t, typename Variable::shape_t>(data_t(std::move(data), data_type), shape);
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
void to_cdf_string_t(PyObject* o, const std::span<T>& out)
{
    if (PyUnicode_Check(o))
    {
        Py_ssize_t size = 0;
        auto py_str = PyUnicode_AsUTF8AndSize(o, &size);
        if (static_cast<std::size_t>(size) <= out.size())
        {
            std::memcpy(out.data(), py_str, size);
            std::memset(out.data() + size, 0, out.size() - size);
        }
        else
        {
            throw std::invalid_argument { fmt::format(
                "String size exceeds allocated size in CDF variable, max size is {} and got {}",
                out.size(), size) };
        }
    }
    else
    {
        throw std::invalid_argument { fmt::format(
            "Expected str for CDF string variable, got {}",
            Py_TYPE(o)->tp_name) };
    }
}

template <CDF_Types data_type>
std::pair<data_t, typename Variable::shape_t> _str_to_nd_data_t(
    const py_list_or_py_tuple auto& values, const auto& shape)
{
    using T = from_cdf_type_t<data_type>;
    auto [_data, _shape]
        = _details::ranges::string_transform<no_init_vector<T>, typename Variable::shape_t>(values,
            shape, [](const PyObject* obj, const std::span<T>& out)
            { to_cdf_string_t<T>(const_cast<PyObject*>(obj), out); });
    return { data_t { std::move(_data), data_type }, std::move(_shape) };
}

template <CDF_Types data_type>
std::pair<data_t, typename Variable::shape_t> _time_to_nd_data_t(const py::array& arr)
{
    using T = from_cdf_type_t<data_type>;
    typename Variable::shape_t shape(arr.ndim());
    std::copy(arr.shape(), arr.shape() + arr.ndim(), std::begin(shape));
    no_init_vector<T> values(arr.size());
    if (!to_cdf_time_t(arr, values.data()))
    {
        throw std::invalid_argument { fmt::format(
            "Cannot convert numpy array with dtype '{}' to CDF time type {}",
            std::string(py::str(arr.dtype())), cdf_type_str(data_type)) };
    }
    return { data_t { std::move(values), data_type }, std::move(shape) };
}

template <CDF_Types data_type>
std::pair<data_t, typename Variable::shape_t> _time_to_nd_data_t(
    const py_list_or_py_tuple auto& values)
{
    using T = from_cdf_type_t<data_type>;
    auto [data, shape]
        = _details::ranges::transform<no_init_vector<T>, typename Variable::shape_t>(values,
            [](const PyObject* obj) -> T { return to_cdf_time_t<T>(const_cast<PyObject*>(obj)); });
    return { data_t { std::move(data), data_type }, std::move(shape) };
}

template <CDF_Types cdf_type>
std::pair<data_t, typename Variable::shape_t> _set_var_data_t(const py::array& values)
{

    if constexpr (is_cdf_string_type(cdf_type))
    {
        return _str_to_nd_data_t<cdf_type>(values);
    }
    else
    {
        if constexpr (is_cdf_time_type(cdf_type))
        {
            return _time_to_nd_data_t<cdf_type>(values);
        }
        else
        {
            return _numeric_to_nd_data_t<cdf_type>(values);
        }
    }
}

template <CDF_Types cdf_type>
std::pair<data_t, typename Variable::shape_t> _set_var_data_t(
    const py_list_or_py_tuple auto& values, const auto& shape)
{

    if constexpr (is_cdf_string_type(cdf_type))
    {
        return _str_to_nd_data_t<cdf_type>(values, shape);
    }
    else
    {
        if constexpr (is_cdf_time_type(cdf_type))
        {
            return _time_to_nd_data_t<cdf_type>(values);
        }
        else
        {
            return _numeric_to_nd_data_t<cdf_type>(values);
        }
    }
}

struct _min_storage_result
{
    uint8_t size_in_bytes = 1;
    bool is_signed = false;
    bool use_double = false;

    bool operator<(const _min_storage_result& other) const
    {
        if (use_double != other.use_double)
        {
            return other.use_double;
        }
        if (is_signed != other.is_signed)
        {
            return other.is_signed;
        }
        return size_in_bytes < other.size_in_bytes;
    }
    bool operator>(const _min_storage_result& other) const { return other < *this; }
};

[[nodiscard]] inline _min_storage_result _min_storage(PyObject* value)
{
    _min_storage_result min_storage;
    if (PyLong_Check(value))
    {
        // CDF format only supports up to signed 64 bits integers and unsigned 32 bits integers
        int64_t val = PyLong_AsLongLong(const_cast<PyObject*>(value));
        if (val < 0)
        {
            min_storage.is_signed = true;
            if (val < std::numeric_limits<int8_t>::min())
            {
                if (val < std::numeric_limits<int16_t>::min())
                {
                    if (val < std::numeric_limits<int32_t>::min())
                    {
                        min_storage.size_in_bytes = 8;
                    }
                    else
                    {
                        min_storage.size_in_bytes
                            = min_storage.size_in_bytes > 4 ? min_storage.size_in_bytes : 4;
                    }
                }
                else
                {
                    min_storage.size_in_bytes
                        = min_storage.size_in_bytes > 2 ? min_storage.size_in_bytes : 2;
                }
            }
        }
        else
        {
            uint64_t uval = PyLong_AsUnsignedLongLong(const_cast<PyObject*>(value));
            if (uval > std::numeric_limits<uint32_t>::max())
            {
                min_storage.size_in_bytes = 8;
            }
            else if (uval > std::numeric_limits<uint16_t>::max())
            {
                min_storage.size_in_bytes
                    = min_storage.size_in_bytes > 4 ? min_storage.size_in_bytes : 4;
            }
            else if (uval > std::numeric_limits<uint8_t>::max())
            {
                min_storage.size_in_bytes
                    = min_storage.size_in_bytes > 2 ? min_storage.size_in_bytes : 2;
            }
        }
    }
    else if (PyFloat_Check(value))
    {
        min_storage.use_double = true;
    }
    return min_storage;
}


[[nodiscard]] inline _min_storage_result _min_storage(const py_list_or_py_tuple auto& values)
{
    _min_storage_result min_storage;
    for (const PyObject* obj : _details::ranges::py_list_or_tuple_view(values))
    {
        if (PyList_Check(obj))
        {
            auto inner_min_storage
                = _min_storage(py::reinterpret_borrow<py::list>(const_cast<PyObject*>(obj)));
            min_storage = min_storage > inner_min_storage ? min_storage : inner_min_storage;
        }
        else if (PyTuple_Check(obj))
        {
            auto inner_min_storage
                = _min_storage(py::reinterpret_borrow<py::tuple>(const_cast<PyObject*>(obj)));
            min_storage = min_storage > inner_min_storage ? min_storage : inner_min_storage;
        }
        else
        {
            auto inner_min_storage = _min_storage(const_cast<PyObject*>(obj));
            min_storage = min_storage > inner_min_storage ? min_storage : inner_min_storage;
        }
    }
    return min_storage;
}


[[nodiscard]] inline CDF_Types _infer_best_type(const py_list_or_py_tuple auto& values)
{
    static const auto signed_types = std::array { CDF_Types::CDF_INT1, CDF_Types::CDF_INT2,
        CDF_Types::CDF_INT4, CDF_Types::CDF_INT8 };
    static const auto unsigned_types = std::array { CDF_Types::CDF_UINT1, CDF_Types::CDF_UINT2,
        CDF_Types::CDF_UINT4, CDF_Types::CDF_INT8 };
    for (const PyObject* obj : _details::ranges::py_list_or_tuple_view(values))
    {
        if (PyBytes_Check(obj) or PyUnicode_Check(obj))
        {
            return CDF_Types::CDF_UCHAR;
        }
        else if (PyLong_Check(obj))
        {
            auto min_storage = _min_storage(values);
            if (min_storage.use_double)
            {
                return CDF_Types::CDF_DOUBLE;
            }
            if (min_storage.is_signed)
            {
                return signed_types[(min_storage.size_in_bytes - 1) & 0x3];
            }
            else
            {
                return unsigned_types[(min_storage.size_in_bytes - 1) & 0x3];
            }
        }
        else if (PyFloat_Check(obj))
        {
            return CDF_Types::CDF_DOUBLE;
        }
        else if (PyList_Check(obj) or PyTuple_Check(obj))
        {
            return _infer_best_type(py::reinterpret_borrow<py::list>(const_cast<PyObject*>(obj)));
        }
        else if (PyDateTime_Check(obj))
        {
            return CDF_Types::CDF_TIME_TT2000;
        }
        else
        {
            throw std::invalid_argument { fmt::format(
                "Unsupported data type in input values: {}",
                Py_TYPE(const_cast<PyObject*>(obj))->tp_name) };
        }
    }
    return CDF_Types::CDF_NONE;
}

inline void set_values(Variable& var, const py_list_or_py_tuple auto& values,
    std::optional<CDF_Types> data_type, bool force = false)
{
    auto spec = analyze_collection(values);
    if ((!data_type.has_value()) or (*data_type == CDF_Types::CDF_NONE))
    {
        data_type = spec.inferred_cdf_type;
    }
    else
    {
        if (not force)
        {
            if (not are_compatible_types(*data_type, spec.inferred_cdf_type) and not spec.is_empty)
            {
                throw std::invalid_argument { fmt::format(
                    "Incompatible CDF data type: expected {}, but input values have type {}",
                    cdf_type_str(*data_type), cdf_type_str(spec.inferred_cdf_type)) };
            }
            if (not _details::are_compatible_shapes(var, spec.shape))
            {
                throw std::invalid_argument { fmt::format(
                    "Incompatible variable shape: expected [{}], got [{}]",
                    fmt::join(var.shape(), ", "), fmt::join(spec.shape, ", ")) };
            }
        }
    }
    if (not spec.is_empty)
    {
        var.set_data(cdf_type_dispatch(
            *data_type, [&spec]<CDF_Types T>(const auto& values)
            { return _set_var_data_t<T>(values, spec.shape); }, values));
    }
    else
    {
        var.set_data(cdf_type_dispatch(*data_type,
            []<CDF_Types T>()
            {
                return std::pair { Variable::var_data_t {
                                       no_init_vector<from_cdf_type_t<T>> {}, T },
                    Variable::shape_t {} };
            }));
    }
}

inline py::array ensure_utf8(const py::array& values)
{
    if (values.dtype().kind() == 'U')
    {
        auto np = py::module::import("numpy");
        py::array utf8_array = np.attr("char").attr("encode")(values, "utf-8");
        return utf8_array;
    }
    return values;
}

inline void set_values(
    Variable& var, const py::array& values, std::optional<CDF_Types> data_type, bool force = false)
{
    auto spec = analyze_collection(values);
    if (!data_type.has_value() or (*data_type == CDF_Types::CDF_NONE))
    {
        data_type = spec.inferred_cdf_type;
        if (data_type == CDF_Types::CDF_NONE)
        {
            throw std::invalid_argument { fmt::format(
                "Could not infer a compatible CDF data type from numpy array with dtype '{}' and kind '{}'",
                std::string(py::str(values.dtype())), values.dtype().kind()) };
        }
    }
    else
    {
        if (not force)
        {
            if (not are_compatible_types(var.type(), *data_type))
            {
                throw std::invalid_argument { fmt::format(
                    "Incompatible specified CDF data type and input values: expected {}, but input has type {}",
                    cdf_type_str(var.type()), cdf_type_str(spec.inferred_cdf_type)) };
            }
            if (not _details::are_compatible_shapes(var, spec.shape))
            {
                throw std::invalid_argument { fmt::format(
                    "Incompatible variable shape: expected [{}], got [{}]",
                    fmt::join(var.shape(), ", "), fmt::join(spec.shape, ", ")) };
            }
        }
    }
    var.set_data(cdf_type_dispatch(
        *data_type, []<CDF_Types T>(const py::array& values) { return _set_var_data_t<T>(values); },
        values));
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
        .def(
            "_set_values",
            [](Variable& var, const py::array& values, std::optional<CDF_Types> data_type,
                bool force)
            {
                if (not force and var.type() != CDF_Types::CDF_NONE)
                {
                    py::warnings::warn(
                        "Overriding existing variable values without force=True is deprecated and "
                        "will raise an exception in future versions.",
                        PyExc_DeprecationWarning, 3);
                }
                auto effective_type = data_type ? data_type
                    : (force ? std::nullopt : std::optional { var.type() });
                set_values(var, ensure_utf8(values), effective_type, force);
            },
            py::arg("values").noconvert(), py::arg("data_type") = std::nullopt,
            py::arg("force") = false)
        .def(
            "_set_values",
            [](Variable& var, const py::list& values, std::optional<CDF_Types> data_type,
                bool force)
            {
                if (not force and var.type() != CDF_Types::CDF_NONE)
                {
                    py::warnings::warn(
                        "Overriding existing variable values without force=True is deprecated and "
                        "will raise an exception in future versions.",
                        PyExc_DeprecationWarning, 3);
                }
                auto effective_type = data_type ? data_type
                    : (force ? std::nullopt : std::optional { var.type() });
                set_values(var, values, effective_type, force);
            },
            py::arg("values").noconvert(), py::arg("data_type") = std::nullopt,
            py::arg("force") = false)
        .def(
            "_set_values",
            [](Variable& var, const py::tuple& values, std::optional<CDF_Types> data_type,
                bool force)
            {
                if (not force and var.type() != CDF_Types::CDF_NONE)
                {
                    py::warnings::warn(
                        "Overriding existing variable values without force=True is deprecated and "
                        "will raise an exception in future versions.",
                        PyExc_DeprecationWarning, 3);
                }
                auto effective_type = data_type ? data_type
                    : (force ? std::nullopt : std::optional { var.type() });
                set_values(var, values, effective_type, force);
            },
            py::arg("values").noconvert(), py::arg("data_type") = std::nullopt,
            py::arg("force") = false)
        .def(
            "_set_values",
            [](Variable& var, const Variable& source, bool force)
            {
                if (var.type() != CDF_Types::CDF_NONE and not force)
                {
                    if (var.type() != source.type())
                        throw std::invalid_argument { fmt::format(
                            "Incompatible variable types: destination is {}, source is {}",
                            cdf_type_str(var.type()), cdf_type_str(source.type())) };
                    if (var.is_nrv() != source.is_nrv())
                        throw std::invalid_argument { fmt::format(
                            "Incompatible record variance: destination is_nrv={}, source is_nrv={}",
                            var.is_nrv(), source.is_nrv()) };
                    if (var.shape() != source.shape())
                        throw std::invalid_argument { fmt::format(
                            "Incompatible variable shapes: destination [{}], source [{}]",
                            fmt::join(var.shape(), ", "), fmt::join(source.shape(), ", ")) };
                }
                var.set_data(source);
            },
            py::arg("source"), py::arg("force") = false)
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
                    throw std::invalid_argument { fmt::format(
                        "Variable attribute '{}' already exists", attr.name) };
                }
            },
            py::arg("attribute"), py::return_value_policy::reference_internal);
}
