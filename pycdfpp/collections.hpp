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

template <typename T>
concept Vector
    = std::is_same_v<std::decay_t<T>, no_init_vector<typename std::decay_t<T>::value_type>>
    || std::is_same_v<std::decay_t<T>, std::vector<typename std::decay_t<T>::value_type>>;


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

    template <typename value_t>
    [[nodiscard]]
    auto make_span(const np_array auto& arr)
    {
        py::buffer_info info = arr.request();
        return std::span<value_t>(
            static_cast<value_t*>(info.ptr), static_cast<std::size_t>(info.size));
    }

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

    struct py_stealing_raw_sink
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
        py_stealing_raw_sink& operator++() { return *this; }
        py_stealing_raw_sink& operator++(int) { return *this; }
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

        auto begin() const { return _iterator { _collection, _start_offset }; }
        auto end() const { return _iterator { _collection, _size - _end_offset }; }

        std::size_t size() const { return _size - _start_offset - _end_offset; }

        py_list_or_tuple_view subview(std::size_t start, std::size_t end) const
        {
            py_list_or_tuple_view subview = *this;
            subview._start_offset += start;
            subview._end_offset += (size() - end);
            return subview;
        }

        py_list_or_tuple_view subview(std::size_t start) const
        {
            py_list_or_tuple_view subview = *this;
            subview._start_offset += start;
            return subview;
        }

    private:
        std::size_t _start_offset = 0;
        std::size_t _end_offset = 0;
        PyObject* _collection;
        std::size_t _size;
    };


    [[nodiscard]] std::size_t _string_length(const PyObject* obj)
    {
        Py_ssize_t len = 0;
        PyUnicode_AsUTF8AndSize(const_cast<PyObject*>(obj), &len);
        return static_cast<std::size_t>(len);
    }

    template <typename shape_t = std::vector<std::size_t>>
    [[nodiscard]] inline shape_t _shape(const py_list_or_py_tuple auto& input, shape_t&& shape = {})
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
            else if (PyUnicode_Check(first))
            {
                shape.push_back(static_cast<std::size_t>(_string_length(first)));
            }
        }
        return shape;
    }

    enum class BestTypeId : uint16_t
    {
        None = 0,

        // Category Masks (High Bits)
        // xxxx 01xx xxxx = Unsigned
        // xxxx 10xx xxxx = Signed
        // xx11 xxxx xxxx = Float
        // xxxx 0001 xxxx = String
        // xxxx 0010 xxxx = DateTime

        // Numeric Values (Ranked by power of 2 so OR-ing picks the largest width)
        NumericMask = 0x3C0, // 11 1100 0000
        UInt8 = 0x41, // 0100 0001
        UInt16 = 0x42, // 0100 0010
        UInt32 = 0x44, // 0100 0100
        UInt64 = 0x48, // 0100 1000

        Int8 = 0x81, // 1000 0001
        Int16 = 0x82, // 1000 0010
        Int32 = 0x84, // 1000 0100
        Int64 = 0x88, // 1000 1000

        Float = 0x300, // 1100 0000 (Force Float to win over Int/UInt)

        String = 0x10, // 0001 0000
        DateTime = 0x20 // 0010 0000
    };

    [[nodiscard]] constexpr BestTypeId operator|(BestTypeId lhs, BestTypeId rhs)
    {
        using T = std::underlying_type_t<BestTypeId>;
        return static_cast<BestTypeId>(static_cast<T>(lhs) | static_cast<T>(rhs));
    }

    [[nodiscard]] constexpr BestTypeId operator&(BestTypeId lhs, BestTypeId rhs)
    {
        using T = std::underlying_type_t<BestTypeId>;
        return static_cast<BestTypeId>(static_cast<T>(lhs) & static_cast<T>(rhs));
    }

    [[nodiscard]] constexpr BestTypeId& operator|=(BestTypeId& lhs, BestTypeId rhs)
    {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr bool contains(BestTypeId t, BestTypeId flag)
    {
        return (t & flag) == flag;
    }

    [[nodiscard]] constexpr bool has_string(BestTypeId t)
    {
        return contains(t, BestTypeId::String);
    }

    [[nodiscard]] constexpr bool has_numerical_value(BestTypeId t)
    {
        return (t & BestTypeId::NumericMask) != BestTypeId::None;
    }

    [[nodiscard]] constexpr bool has_datetime(BestTypeId t)
    {
        return contains(t, BestTypeId::DateTime);
    }

    [[nodiscard]] constexpr bool is_invalid_mix(BestTypeId t)
    {
        return (has_string(t) + has_numerical_value(t) + has_datetime(t)) > 1;
    }

    [[nodiscard]] inline BestTypeId _merge_types(BestTypeId a, BestTypeId b)
    {
        auto r = a | b;
        if (is_invalid_mix(r))
        {
            throw std::out_of_range("Incompatible types in nested lists/tuples");
        }
        return r;
    }

    struct BestType
    {
        BestTypeId inferred_type = BestTypeId::None;
        int64_t max_int_value = 0;
        int64_t min_int_value = 0;
        std::size_t string_length = 0;

        constexpr BestType& operator|=(const BestType& other)
        {
            auto _inferred_type = _merge_types(inferred_type, other.inferred_type);
            this->max_int_value = std::max(this->max_int_value, other.max_int_value);
            this->min_int_value = std::min(this->min_int_value, other.min_int_value);
            if (this->string_length != other.string_length
                && this->inferred_type != BestTypeId::None)
            {
                throw std::out_of_range("Inconsistent string lengths in nested lists/tuples");
            }
            this->string_length = other.string_length;
            this->inferred_type = _inferred_type;
            return *this;
        }
    };

    template <typename T>
    [[nodiscard]] constexpr bool fits(auto min, auto max)
    {
        return (min >= static_cast<decltype(min)>(std::numeric_limits<T>::min()))
            && (max <= static_cast<decltype(max)>(std::numeric_limits<T>::max()));
    }

    struct analyze_result
    {
        std::vector<std::size_t> shape;
        BestType inferred_type = {};
        CDF_Types inferred_cdf_type = CDF_Types::CDF_NONE;
    };

    struct analyze_context
    {
        std::optional<std::size_t> inner_collection_size = std::nullopt;
        BestType inferred_type = {};
    };

    [[nodiscard]] inline BestType _min_cdf_int(PyObject* value)
    {
        // CDF format only supports up to signed 64 bits integers and unsigned 32 bits integers
        int64_t val = PyLong_AsLongLong(const_cast<PyObject*>(value));
        return { BestTypeId::Int64, val, val, 0 };
    }

    [[nodiscard]] inline BestType _best_match_type(PyObject* obj)
    {
        assert(obj != nullptr);
        if (PyLong_Check(obj))
        {
            return _min_cdf_int(obj);
        }
        else if (PyFloat_Check(obj))
        {
            return { BestTypeId::Float, 0, 0, 0 };
        }
        else if (PyUnicode_Check(obj))
        {
            return { BestTypeId::String, 0, 0, _string_length(obj) };
        }
        else if (PyDateTime_Check(obj))
        {
            return { BestTypeId::DateTime, 0, 0, 0 };
        }
        return { BestTypeId::None, 0, 0, 0 };
    }


    [[nodiscard]] constexpr CDF_Types to_cdf_type(const BestType& t)
    {
        if (has_string(t.inferred_type))
        {
            return CDF_Types::CDF_UCHAR;
        }
        else if (has_datetime(t.inferred_type))
        {
            return CDF_Types::CDF_TIME_TT2000;
        }
        else if (has_numerical_value(t.inferred_type))
        {
            if (contains(t.inferred_type, BestTypeId::Float))
            {
                return CDF_Types::CDF_DOUBLE;
            }
            else
            {
                if (t.min_int_value < 0)
                {
                    if (fits<int8_t>(t.min_int_value, t.max_int_value))
                    {
                        return CDF_Types::CDF_INT1;
                    }
                    else if (fits<int16_t>(t.min_int_value, t.max_int_value))
                    {
                        return CDF_Types::CDF_INT2;
                    }
                    else if (fits<int32_t>(t.min_int_value, t.max_int_value))
                    {
                        return CDF_Types::CDF_INT4;
                    }
                    else
                    {
                        return CDF_Types::CDF_INT8;
                    }
                }
                else
                {
                    if (fits<uint8_t>(t.min_int_value, t.max_int_value))
                    {
                        return CDF_Types::CDF_UINT1;
                    }
                    else if (fits<uint16_t>(t.min_int_value, t.max_int_value))
                    {
                        return CDF_Types::CDF_UINT2;
                    }
                    else if (fits<uint32_t>(t.min_int_value, t.max_int_value))
                    {
                        return CDF_Types::CDF_UINT4;
                    }
                    else
                    {
                        // Fallback to signed 64 bits integer since CDF has no uint64 type
                        return CDF_Types::CDF_INT8;
                    }
                }
            }
        }
        return CDF_Types::CDF_NONE;
    }

    inline void _analyze_collection_impl(
        const py_list_or_py_tuple auto& input, analyze_result& result, std::size_t depth = 0)
    {
        auto view = py_list_or_tuple_view { input };
        if (py::len(input) > 0)
        {
            if (depth >= result.shape.size())
            {
                result.shape.push_back(std::size(view));
            }
            analyze_context ctx;
            for (const PyObject* obj : view)
            {
                if (PyList_Check(obj))
                {
                    std::size_t curr_inner_size
                        = static_cast<std::size_t>(PyList_Size(const_cast<PyObject*>(obj)));
                    if (ctx.inner_collection_size
                        && (curr_inner_size != *ctx.inner_collection_size))
                    {
                        throw std::out_of_range("Inconsistent shapes in nested lists/tuples");
                    }
                    else
                    {
                        ctx.inner_collection_size = curr_inner_size;
                    }
                    _analyze_collection_impl(
                        py::reinterpret_borrow<py::list>(const_cast<PyObject*>(obj)), result,
                        depth + 1);
                }
                else if (PyTuple_Check(obj))
                {
                    std::size_t curr_inner_size
                        = static_cast<std::size_t>(PyTuple_Size(const_cast<PyObject*>(obj)));
                    if (ctx.inner_collection_size
                        && (curr_inner_size != *ctx.inner_collection_size))
                    {
                        throw std::out_of_range("Inconsistent shapes in nested lists/tuples");
                    }
                    else
                    {
                        ctx.inner_collection_size = curr_inner_size;
                    }
                    _analyze_collection_impl(
                        py::reinterpret_borrow<py::tuple>(const_cast<PyObject*>(obj)), result,
                        depth + 1);
                }
                else
                {
                    ctx.inferred_type |= _best_match_type(const_cast<PyObject*>(obj));
                }
            }
            result.inferred_type |= ctx.inferred_type;
        }
    }

    [[nodiscard]] inline analyze_result analyze_collection(const py_list_or_py_tuple auto& input)
    {
        auto result = analyze_result {};
        _analyze_collection_impl(input, result);
        result.inferred_cdf_type = to_cdf_type(result.inferred_type);
        return result;
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

    template <Vector output_t, typename shape_t = std::vector<std::size_t>>
    [[nodiscard]] inline std::pair<output_t, shape_t> transform(
        const py_list_or_py_tuple auto& input, const auto& f)
    {
        using value_type = typename output_t::value_type;
        auto shape = _shape<shape_t>(input);
        auto result = output_t(_details::flat_size(shape));
        auto r = _transform_inner<value_type>(input, f, result.data(), std::span(shape));
        assert(r == result.data() + result.size());
        return { result, shape };
    }

    template <typename T>
    [[nodiscard]]
    inline T* _string_transform_inner(const py_list_or_py_tuple auto& input, const auto& f,
        T* res_ptr, const auto& shape_span, const std::size_t str_len)
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
                res_ptr = _string_transform_inner<T>(
                    py::reinterpret_borrow<py::list>(const_cast<PyObject*>(obj)), f, res_ptr,
                    shape_span.subspan(1), str_len);
            }
            else
            {
                f(obj, std::span(res_ptr, str_len));
                res_ptr += str_len;
            }
        }
        return res_ptr;
    }

    template <Vector output_t, typename shape_t = std::vector<std::size_t>>
    [[nodiscard]] inline std::pair<output_t, shape_t> string_transform(
        const py_list_or_py_tuple auto& input, const auto& f)
    {
        using value_type = typename output_t::value_type;
        auto shape = _shape<shape_t>(input);
        auto result = output_t(_details::flat_size(shape));
        auto r = _string_transform_inner<value_type>(
            input, f, result.data(), std::span(shape.data(), shape.size() - 1), shape.back());
        assert(r == result.data() + result.size());
        return { result, shape };
    }

    template <np_array output_t>
    [[nodiscard]] inline py::object transform(const py_list_or_py_tuple auto& input, const auto& f)
    {
        using value_type = typename output_t::value_type;
        auto shape = _shape(input);
        auto result = fast_allocate_array<value_type>(shape);
        py::buffer_info res_buff = result.request(true);
        value_type* res_ptr = static_cast<value_type*>(res_buff.ptr);
        res_ptr = _transform_inner<value_type>(input, f, res_ptr, std::span(shape));
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
        auto out = py_stealing_raw_sink { _details::underlying_pyobject(result) };
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
        auto out = py_stealing_raw_sink { _details::underlying_pyobject(result) };
        for (const PyObject* obj : view)
        {
            if (auto r = f(obj))
            {
                *out = r;
            }
            else if (PyList_Check(obj))
            {
                *out = transform<output_t>(
                    py::reinterpret_borrow<py::list>(const_cast<PyObject*>(obj)), f);
            }
            else if (PyTuple_Check(obj))
            {
                *out = transform<output_t>(
                    py::reinterpret_borrow<py::tuple>(const_cast<PyObject*>(obj)), f);
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
    return cdf_type_dipatch(
        variable.type(), []<CDF_Types T>(cdf::Variable& var)
        { return _details::impl_make_buffer<T>(var); }, variable);
}
