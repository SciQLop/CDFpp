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

#include <ranges>

#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
#include <cdfpp/no_init_vector.hpp>
using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

template <typename T>
concept py_list_or_py_tuple
    = std::is_same_v<std::decay_t<T>, py::list> || std::is_same_v<std::decay_t<T>, py::tuple>;

template <typename T>
concept py_list = std::is_same_v<std::decay_t<T>, py::list>;


template <typename T>
concept np_array
    = std::is_same_v<std::decay_t<T>, py::array> || std::is_base_of_v<py::array, std::decay_t<T>>;

namespace _details
{

[[nodiscard]] inline PyObject* underlying_pyobject(const py::object& obj)
{
    return obj.ptr();
}

template <typename T>
[[nodiscard]] inline const T* cast(const PyObject* const o)
{
    return reinterpret_cast<const T*>(
        reinterpret_cast<const py::detail::instance*>(o)->simple_value_holder[0]);
}

template <typename T>
[[nodiscard]] inline std::size_t flat_size(const T& shape)
{
    return std::accumulate(
        std::cbegin(shape), std::cend(shape), 1UL, std::multiplies<std::size_t>());
}

[[nodiscard]] inline std::size_t flat_size(const py::buffer_info& info)
{
    return std::accumulate(
        std::cbegin(info.shape), std::cend(info.shape), 1UL, std::multiplies<std::size_t>());
}

template <typename T, typename Shape, typename Owner = std::nullptr_t>
[[nodiscard]] inline auto fast_allocate_array(const Shape& shape, Owner&& owner = std::nullptr_t {})
{
    using value_t = std::remove_const_t<T>;
    using alloc_t = default_init_allocator<value_t>;

    auto ptr = alloc_t().allocate(flat_size(shape));

    if constexpr (std::is_same_v<std::nullptr_t, std::remove_cvref_t<Owner>>)
    {
        return py::array_t<value_t>(shape, ptr,
            py::capsule(ptr, [](void* p) { alloc_t().deallocate(static_cast<value_t*>(p), 0); }));
    }
    else
    {
        return py::array_t<value_t>(shape, ptr, std::forward<Owner>(owner));
    }
}

template <typename T>
[[nodiscard]] inline auto fast_allocate_array(const py::buffer_info& info)
{
    return fast_allocate_array<T>(info.shape);
}

template <typename T>
[[nodiscard]] inline auto fast_allocate_array(const py::array& ref)
{
    return fast_allocate_array<T>(std::span(ref.shape(), static_cast<std::size_t>(ref.ndim())));
}

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

template <py_list_or_py_tuple T>
[[nodiscard]] inline PyObject* get_item(const PyObject* const p, std::size_t pos)
{
    if constexpr (std::is_same_v<T, py::list>)
    {
        return PyList_GET_ITEM(p, static_cast<Py_ssize_t>(pos));
    }
    else if constexpr (std::is_same_v<T, py::tuple>)
    {
        return PyTuple_GET_ITEM(p, static_cast<Py_ssize_t>(pos));
    }
}


namespace ranges
{
    struct py_list_set_iterator
    {
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = std::ptrdiff_t;

        PyObject* list;
        std::size_t index;

        inline py_list_set_iterator& operator=(PyObject* r)
        {
            if (r)
            {
                PyList_SET_ITEM(list, index, r);
            }
            return *this;
        }

        inline py_list_set_iterator& operator*() { return *this; }
        inline py_list_set_iterator& operator++()
        {
            ++index;
            return *this;
        }
        inline py_list_set_iterator operator++(int)
        {
            auto temp = *this;
            ++index;
            return temp;
        }
    };

    struct py_raw_sink
    {
        PyObject* list_ptr;

        struct proxy
        {
            PyObject* list;
            void operator=(PyObject* r)
            {
                if (r)
                {
                    PyList_Append(list, r);
                    Py_DECREF(r);
                }
            }
            void operator=(py::object r)
            {
                if (r)
                {
                    PyList_Append(list, r.ptr());
                }
            }
        };

        proxy operator*() { return { list_ptr }; }
        py_raw_sink& operator++() { return *this; }
        py_raw_sink& operator++(int) { return *this; }
    };

    template <py_list_or_py_tuple T>
    struct py_list_or_tuple_view : std::ranges::view_interface<py_list_or_tuple_view<T>>
    {
        struct _iterator
        {
        private:
            PyObject* _collection;
            std::size_t _index;

        public:
            using value_type = const PyObject*;
            using difference_type = std::ptrdiff_t;
            using iterator_concept = std::random_access_iterator_tag;

            _iterator(PyObject* collection, std::size_t index)
                    : _collection(collection), _index(index)
            {
            }

            const PyObject* operator*() const { return get_item<T>(_collection, _index); }

            _iterator& operator++()
            {
                ++_index;
                return *this;
            }

            _iterator& operator--()
            {
                --_index;
                return *this;
            }

            _iterator operator+(difference_type n) const { return { _collection, _index + n }; }

            _iterator operator-(difference_type n) const { return { _collection, _index - n }; }

            difference_type operator-(const _iterator& other) const
            {
                return _index - other._index;
            }

            auto operator<=>(const _iterator& other) const = default;

            bool operator!=(const _iterator& other) const
            {
                return _index != other._index || _collection != other._collection;
            }
        };

        py_list_or_tuple_view(const T& c) : _collection(underlying_pyobject(c)), _size(py::len(c))
        {
        }

        auto begin() const { return _iterator { _collection, 0 }; }
        auto end() const { return _iterator { _collection, _size }; }

    private:
        PyObject* _collection;
        std::size_t _size;
    };

    [[nodiscard]] inline std::vector<std::size_t> _shape(
        const py_list_or_py_tuple auto& input, std::vector<std::size_t>&& shape = {})
    {
        auto view = py_list_or_tuple_view { input };
        shape.push_back(py::len(input));
        if (py::len(input) > 0)
        {
            const PyObject* first = *view.begin();
            if (PyList_Check(first) || PyTuple_Check(first))
            {
                return _shape(py::reinterpret_borrow<py::list>(const_cast<PyObject*>(first)),
                    std::move(shape));
            }
        }
        // std::reverse(std::begin(shape), std::end(shape));
        return shape;
    }


    template <typename T>
    [[nodiscard]]
    inline T* _transform_inner(
        const py_list_or_py_tuple auto& input, const auto& f, T* res_ptr, const auto& shape_span)
    {
        if (py::len(input) != shape_span[0])
        {
            throw std::out_of_range("Inconsistent shapes in nested lists/tuples");
        }
        auto view = py_list_or_tuple_view { input };
        for (const PyObject* obj : view)
        {
            if (PyList_Check(obj))
            {
                if (shape_span.size() < 2)
                {
                    throw std::out_of_range("Inconsistent shapes in nested lists/tuples");
                }
                res_ptr = _transform_inner<T>(
                    py::reinterpret_borrow<py::list>(const_cast<PyObject*>(obj)), f, res_ptr,
                    shape_span.subspan(1));
            }
            else
            {
                *res_ptr = f(obj);
                ++res_ptr;
            }
        }
        return res_ptr;
    }

    template <np_array output_t>
    [[nodiscard]] inline py::object transform(const py_list_or_py_tuple auto& input, const auto& f)
    {
        using value_type = typename output_t::value_type;
        auto shape = _shape(input);
        auto result = fast_allocate_array<value_type>(shape);
        py::buffer_info res_buff = result.request(true);
        value_type* res_ptr = static_cast<value_type*>(res_buff.ptr);
        res_ptr = _transform_inner<value_type>(
            input, f, res_ptr, std::span(shape.data(), shape.size()));
        return result;
    }

    template <np_array output_t, typename in_value_t, typename out_value_t>
    [[nodiscard]] inline py::object transform(const py::array& input, const auto& f)
    {
        py::buffer_info in_buff = input.request();
        auto result = _details::fast_allocate_array<out_value_t>(in_buff);
        in_value_t* in_ptr = static_cast<in_value_t*>(in_buff.ptr);
        f(std::span(in_ptr, result.size()), static_cast<out_value_t*>(result.request(true).ptr));
        return result;
    }

    template <py_list output_t>
    [[nodiscard]] inline py::object transform(
        const auto* input, const auto& shape_span, const auto& f)
    {
        py::list result;
        auto out = py_raw_sink { _details::underlying_pyobject(result) };
        const auto flat_sz = _details::flat_size(shape_span);
        for (std::size_t i = 0; i < shape_span[0]; ++i)
        {
            if (shape_span.size() > 1)
            {
                *out = transform<output_t>(input + i * flat_sz, shape_span.subspan(1), f);
            }
            else
            {
                if (auto r = f(input[i]))
                {
                    *out = r;
                }
                else
                {
                    throw std::out_of_range("Conversion failed");
                }
            }
        }
        return result;
    }

    template <py_list output_t, typename value_t>
    [[nodiscard]] inline py::object transform(const Variable& v, const auto& f)
    {
        return transform<output_t>(v.get<value_t>().data(), std::span(v.shape()), f);
    }

    template <py_list output_t, typename value_t>
    [[nodiscard]] inline py::object transform(const py::array& input, const auto& f)
    {
        auto view = [&input]() constexpr
        {
            if constexpr (helpers::is_any_of_v<value_t, int64_t, uint64_t>)
            {
                return py::array(input.attr("view")("int64"));
            }
            else
            {
                return input;
            }
        }();
        auto buffer = view.request();
        value_t* in_ptr = reinterpret_cast<value_t*>(buffer.ptr);
        return transform<output_t>(
            in_ptr, std::span(input.shape(), static_cast<std::size_t>(input.ndim())), f);
    }

    template <py_list output_t>
    [[nodiscard]] inline py::object transform(const py_list_or_py_tuple auto& input, const auto& f)
    {
        using namespace _details::ranges;
        py::list result;
        auto view = py_list_or_tuple_view(input);
        auto out = py_raw_sink { _details::underlying_pyobject(result) };
        for (const PyObject* obj : view)
        {
            if (auto r = f(obj))
            {
                *out = r;
            }
            else if (PyList_Check(obj))
            {
                *out = transform<output_t>(
                    py::reinterpret_borrow<py::list>(const_cast<PyObject*>(obj)), f)
                           .release()
                           .ptr();
            }
            else if (PyTuple_Check(obj))
            {
                *out = transform<output_t>(
                    py::reinterpret_borrow<py::tuple>(const_cast<PyObject*>(obj)), f)
                           .release()
                           .ptr();
            }
            else
            {
                throw std::out_of_range(
                    "Only supports datetime.datetime, tt2000_t, epoch and epoch16 types");
            }
        }
        return result;
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
