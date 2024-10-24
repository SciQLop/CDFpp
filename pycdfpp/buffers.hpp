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

#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace _details
{

[[nodiscard]] std::vector<ssize_t> shape_ssize_t(const Variable& var)
{
    const auto& shape = var.shape();
    std::vector<ssize_t> res(std::size(shape));
    std::transform(std::cbegin(shape), std::cend(shape), std::begin(res),
        [](auto v) { return static_cast<ssize_t>(v); });
    return res;
}

[[nodiscard]] std::vector<ssize_t> str_shape_ssize_t(const Variable& var)
{
    const auto& shape = var.shape();
    std::vector<ssize_t> res(std::size(shape) - 1);
    std::transform(std::cbegin(shape), std::cend(shape) - 1, std::begin(res),
        [](auto v) { return static_cast<ssize_t>(v); });
    return res;
}

template <typename T>
[[nodiscard]] std::vector<ssize_t> strides(const Variable& var)
{
    const auto& shape = var.shape();
    std::vector<ssize_t> res(std::size(shape));
    std::transform(std::crbegin(shape), std::crend(shape), std::begin(res),
        [next = sizeof(T)](auto v) mutable
        {
            auto res = next;
            next = static_cast<ssize_t>(v * next);
            return res;
        });
    std::reverse(std::begin(res), std::end(res));
    return res;
}

template <typename T>
[[nodiscard]] std::vector<ssize_t> str_strides(const Variable& var)
{
    const auto& shape = var.shape();
    std::vector<ssize_t> res(std::size(shape) - 1);
    std::transform(std::crbegin(shape) + 1, std::crend(shape), std::begin(res),
        [next = shape.back()](auto v) mutable
        {
            auto res = next;
            next = static_cast<ssize_t>(v * next);
            return res;
        });
    std::reverse(std::begin(res), std::end(res));
    return res;
}

template <CDF_Types data_t>
[[nodiscard]] py::array make_array(Variable& variable, py::object& obj)
{
    // static_assert(data_t != CDF_Types::CDF_CHAR and data_t != CDF_Types::CDF_UCHAR);
    from_cdf_type_t<data_t>* ptr = nullptr;
    {
        py::gil_scoped_release release;
        ptr = variable.get<from_cdf_type_t<data_t>>().data();
    }
    return py::array_t<from_cdf_type_t<data_t>>(
        shape_ssize_t(variable), strides<from_cdf_type_t<data_t>>(variable), ptr, obj);
}

template <typename T, typename size_type>
[[nodiscard]] static inline std::string_view make_string_view(T* data, size_type len)
{
    return std::string_view(
        reinterpret_cast<const char*>(data), static_cast<std::string_view::size_type>(len));
}

template <typename T>
[[nodiscard]] py::object make_list(
    const T* data, decltype(std::declval<Variable>().shape()) shape, py::object& obj)
{
    if (std::size(shape) > 2)
    {
        py::list res {};
        std::size_t offset = 0;
        auto inner_shape = decltype(shape) { std::begin(shape) + 1, std::end(shape) };
        std::size_t jump = std::accumulate(
            std::cbegin(inner_shape), std::cend(inner_shape), 1UL, std::multiplies<std::size_t>());
        for (auto i = 0UL; i < shape[0]; i++)
        {
            res.append(make_list(data + offset, inner_shape, obj));
            offset += jump;
        }
        return res;
    }
    if (std::size(shape) == 2)
    {
        py::list res {};
        std::size_t offset = 0;
        for (auto i = 0UL; i < shape[0]; i++)
        {
            res.append(make_string_view(data + offset, shape[1]));
            offset += shape[1];
        }
        return res;
    }
    if (std::size(shape) == 1)
    {
        return py::str(make_string_view(data, shape[0]));
    }
    return py::none();
}

template <CDF_Types data_t>
[[nodiscard]] py::object make_list(Variable& variable, py::object& obj)
{
    static_assert(data_t == CDF_Types::CDF_CHAR or data_t == CDF_Types::CDF_UCHAR);
    return make_list(variable.get<from_cdf_type_t<data_t>>().data(), variable.shape(), obj);
}

template <CDF_Types T>
[[nodiscard]] py::buffer_info impl_make_buffer(cdf::Variable& var)
{
    using U = cdf::from_cdf_type_t<T>;
    char* ptr = nullptr;
    {
        py::gil_scoped_release release;
        ptr = var.bytes_ptr();
    }
    if constexpr ((T == CDF_Types::CDF_CHAR) or (T == CDF_Types::CDF_UCHAR))
    {
        return py::buffer_info(ptr, /* Pointer to buffer */
            var.shape().back(), /* Size of one scalar */
            fmt::format("{}s", var.shape().back()),
            static_cast<ssize_t>(std::size(var.shape()) - 1), /* Number of dimensions */
            str_shape_ssize_t(var), str_strides<U>(var), true);
    }
    else
    {
        return py::buffer_info(ptr, /* Pointer to buffer */
            sizeof(U), /* Size of one scalar */
            py::format_descriptor<U>::format(),
            static_cast<ssize_t>(std::size(var.shape())), /* Number of dimensions */
            shape_ssize_t(var), strides<U>(var), true);
    }
}

template <CDF_Types data_t, bool encode_strings>
[[nodiscard]] py::object make_str_array(py::object& obj)
{
    py::module_ np = py::module_::import("numpy");
    if constexpr (encode_strings)
    {
        return np.attr("char").attr("decode")(py::memoryview(obj));
    }
    else
    {
        return np.attr("array")(py::memoryview(obj));
    }
}

}

template <bool encode_strings>
[[nodiscard]] py::object make_values_view(py::object& obj)
{
    Variable& variable = obj.cast<Variable&>();
    switch (variable.type())
    {
        case cdf::CDF_Types::CDF_CHAR:
            return _details::make_str_array<cdf::CDF_Types::CDF_CHAR, encode_strings>(obj);
        case cdf::CDF_Types::CDF_UCHAR:
            return _details::make_str_array<cdf::CDF_Types::CDF_UCHAR, encode_strings>(obj);
        case cdf::CDF_Types::CDF_INT1:
            return _details::make_array<CDF_Types::CDF_INT1>(variable, obj);
        case cdf::CDF_Types::CDF_INT2:
            return _details::make_array<CDF_Types::CDF_INT2>(variable, obj);
        case cdf::CDF_Types::CDF_INT4:
            return _details::make_array<CDF_Types::CDF_INT4>(variable, obj);
        case cdf::CDF_Types::CDF_INT8:
            return _details::make_array<CDF_Types::CDF_INT8>(variable, obj);
        case cdf::CDF_Types::CDF_UINT1:
            return _details::make_array<CDF_Types::CDF_UINT1>(variable, obj);
        case cdf::CDF_Types::CDF_BYTE:
            return _details::make_array<CDF_Types::CDF_BYTE>(variable, obj);
        case cdf::CDF_Types::CDF_UINT2:
            return _details::make_array<CDF_Types::CDF_UINT2>(variable, obj);
        case cdf::CDF_Types::CDF_UINT4:
            return _details::make_array<CDF_Types::CDF_UINT4>(variable, obj);
        case cdf::CDF_Types::CDF_FLOAT:
            return _details::make_array<CDF_Types::CDF_FLOAT>(variable, obj);
        case cdf::CDF_Types::CDF_REAL4:
            return _details::make_array<CDF_Types::CDF_REAL4>(variable, obj);
        case cdf::CDF_Types::CDF_DOUBLE:
            return _details::make_array<CDF_Types::CDF_DOUBLE>(variable, obj);
        case cdf::CDF_Types::CDF_REAL8:
            return _details::make_array<CDF_Types::CDF_REAL8>(variable, obj);
        case cdf::CDF_Types::CDF_EPOCH:
            return _details::make_array<CDF_Types::CDF_EPOCH>(variable, obj);
        case cdf::CDF_Types::CDF_EPOCH16:
            return _details::make_array<CDF_Types::CDF_EPOCH16>(variable, obj);
        case cdf::CDF_Types::CDF_TIME_TT2000:
            return _details::make_array<CDF_Types::CDF_TIME_TT2000>(variable, obj);
        default:
            throw std::runtime_error { std::string { "Unsupported CDF type " }
                + std::to_string(static_cast<int>(variable.type())) };
            break;
    }
    return {};
}

[[nodiscard]] py::buffer_info make_buffer(cdf::Variable& variable)
{

    using namespace cdf;
    switch (variable.type())
    {
        case CDF_Types::CDF_UCHAR:
            return _details::impl_make_buffer<CDF_Types::CDF_UCHAR>(variable);
        case CDF_Types::CDF_CHAR:
            return _details::impl_make_buffer<CDF_Types::CDF_CHAR>(variable);
        case CDF_Types::CDF_INT1:
            return _details::impl_make_buffer<CDF_Types::CDF_INT1>(variable);
        case CDF_Types::CDF_INT2:
            return _details::impl_make_buffer<CDF_Types::CDF_INT2>(variable);
        case CDF_Types::CDF_INT4:
            return _details::impl_make_buffer<CDF_Types::CDF_INT4>(variable);
        case CDF_Types::CDF_INT8:
            return _details::impl_make_buffer<CDF_Types::CDF_INT8>(variable);
        case CDF_Types::CDF_BYTE:
            return _details::impl_make_buffer<CDF_Types::CDF_BYTE>(variable);
        case CDF_Types::CDF_UINT1:
            return _details::impl_make_buffer<CDF_Types::CDF_UINT1>(variable);
        case CDF_Types::CDF_UINT2:
            return _details::impl_make_buffer<CDF_Types::CDF_UINT2>(variable);
        case CDF_Types::CDF_UINT4:
            return _details::impl_make_buffer<CDF_Types::CDF_UINT4>(variable);
        case CDF_Types::CDF_FLOAT:
        case CDF_Types::CDF_REAL4:
            return _details::impl_make_buffer<CDF_Types::CDF_FLOAT>(variable);
        case CDF_Types::CDF_DOUBLE:
        case CDF_Types::CDF_REAL8:
            return _details::impl_make_buffer<CDF_Types::CDF_DOUBLE>(variable);
        case CDF_Types::CDF_EPOCH:
            return _details::impl_make_buffer<CDF_Types::CDF_EPOCH>(variable);
        case CDF_Types::CDF_EPOCH16:
            return _details::impl_make_buffer<CDF_Types::CDF_EPOCH16>(variable);
        case CDF_Types::CDF_TIME_TT2000:
            return _details::impl_make_buffer<CDF_Types::CDF_TIME_TT2000>(variable);
        default:
            throw std::runtime_error { std::string { "Unsupported CDF type " }
                + std::to_string(static_cast<int>(variable.type())) };
            break;
    }
}
