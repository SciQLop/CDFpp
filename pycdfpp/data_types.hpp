/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2026, Laboratory of Plasma Physics- CNRS
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

#include "collections.hpp"

#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf-map.hpp>
#include <cdfpp/cdf-repr.hpp>
#include <cdfpp/variable.hpp>

#include <cdfpp/chrono/cdf-chrono.hpp>
#include <cdfpp/no_init_vector.hpp>
#include <cdfpp_config.h>
using namespace cdf;


[[nodiscard]] inline bool is_dt64ns(const py::array& arr)
{
    if (arr.dtype().kind() != 'M')
        return false;
    return helpers::contains(std::string(arr.dtype().attr("str").cast<std::string>()), "[ns]");
}

[[nodiscard]] inline bool is_dt64ms(const py::array& arr)
{
    if (arr.dtype().kind() != 'M')
        return false;
    return helpers::contains(std::string(arr.dtype().attr("str").cast<std::string>()), "[ms]");
}

[[nodiscard]] inline bool is_dt64(const py::array& arr)
{
    return arr.dtype().kind() == 'M';
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
    DateTime = 0x20, // 0010 0000

    Collection = 0x8000 // 1000 0000 0000 0000
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

[[nodiscard]] constexpr bool has_collection(BestTypeId t)
{
    return contains(t, BestTypeId::Collection);
}

[[nodiscard]] constexpr bool is_invalid_mix(BestTypeId t)
{
    return (has_string(t) + has_numerical_value(t) + has_datetime(t) + has_collection(t)) > 1;
}

[[nodiscard]] constexpr BestTypeId _merge_types(BestTypeId a, BestTypeId b)
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
        this->string_length = std::max(other.string_length, this->string_length);
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
    Variable::shape_t shape;
    BestType inferred_type = {};
    CDF_Types inferred_cdf_type = CDF_Types::CDF_NONE;
    bool is_empty = false;
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
        return { BestTypeId::String, 0, 0, _details::string_length(obj) };
    }
    else if (PyDateTime_Check(obj))
    {
        return { BestTypeId::DateTime, 0, 0, 0 };
    }
    throw std::runtime_error(fmt::format("Unsupported data type encountered"));
    return { BestTypeId::None, 0, 0, 0 };
}

[[nodiscard]] constexpr BestTypeId to_best_type_id(const CDF_Types& t)
{
    switch (t)
    {
        case CDF_Types::CDF_UCHAR:
        case CDF_Types::CDF_CHAR:
            return BestTypeId::String;
        case CDF_Types::CDF_TIME_TT2000:
            return BestTypeId::DateTime;
        case CDF_Types::CDF_DOUBLE:
        case CDF_Types::CDF_FLOAT:
            return BestTypeId::Float;
        case CDF_Types::CDF_INT1:
            return BestTypeId::Int8;
        case CDF_Types::CDF_INT2:
            return BestTypeId::Int16;
        case CDF_Types::CDF_INT4:
            return BestTypeId::Int32;
        case CDF_Types::CDF_INT8:
            return BestTypeId::Int64;
        case CDF_Types::CDF_UINT1:
            return BestTypeId::UInt8;
        case CDF_Types::CDF_UINT2:
            return BestTypeId::UInt16;
        case CDF_Types::CDF_UINT4:
            return BestTypeId::UInt32;
        default:
            return BestTypeId::None;
    }
}

[[nodiscard]] constexpr bool are_compatible_types(const CDF_Types& dest, const CDF_Types& source)
{
    switch (dest)
    {
        case CDF_Types::CDF_UCHAR:
        case CDF_Types::CDF_CHAR:
            return (source == CDF_Types::CDF_UCHAR) || (source == CDF_Types::CDF_CHAR);
        case CDF_Types::CDF_TIME_TT2000:
        case CDF_Types::CDF_EPOCH:
        case CDF_Types::CDF_EPOCH16:
            return (source == CDF_Types::CDF_TIME_TT2000) || (source == CDF_Types::CDF_EPOCH)
                || (source == CDF_Types::CDF_EPOCH16);
        case CDF_Types::CDF_DOUBLE:
        case CDF_Types::CDF_REAL8:
            return helpers::rt_is_in(source, CDF_Types::CDF_DOUBLE, CDF_Types::CDF_REAL8,
                CDF_Types::CDF_FLOAT, CDF_Types::CDF_INT1, CDF_Types::CDF_INT2, CDF_Types::CDF_INT4,
                CDF_Types::CDF_UINT1, CDF_Types::CDF_UINT2, CDF_Types::CDF_UINT4);
        case CDF_Types::CDF_FLOAT:
        case CDF_Types::CDF_REAL4:
            return helpers::rt_is_in(source, CDF_Types::CDF_FLOAT, CDF_Types::CDF_REAL4,
                CDF_Types::CDF_INT1, CDF_Types::CDF_INT2, CDF_Types::CDF_UINT1,
                CDF_Types::CDF_UINT2);
        case CDF_Types::CDF_INT8:
            return helpers::rt_is_in(source, CDF_Types::CDF_INT8, CDF_Types::CDF_INT4,
                CDF_Types::CDF_INT2, CDF_Types::CDF_INT1, CDF_Types::CDF_UINT4,
                CDF_Types::CDF_UINT2, CDF_Types::CDF_UINT1);
        case CDF_Types::CDF_INT4:
            return helpers::rt_is_in(source, CDF_Types::CDF_INT4, CDF_Types::CDF_INT2,
                CDF_Types::CDF_INT1, CDF_Types::CDF_UINT2, CDF_Types::CDF_UINT1);
        case CDF_Types::CDF_INT2:
            return helpers::rt_is_in(
                source, CDF_Types::CDF_INT2, CDF_Types::CDF_INT1, CDF_Types::CDF_UINT1);
        case CDF_Types::CDF_INT1:
        case CDF_Types::CDF_BYTE:
            return source == CDF_Types::CDF_INT1;
        case CDF_Types::CDF_UINT4:
            return helpers::rt_is_in(
                source, CDF_Types::CDF_UINT4, CDF_Types::CDF_UINT2, CDF_Types::CDF_UINT1);
        case CDF_Types::CDF_UINT2:
            return helpers::rt_is_in(source, CDF_Types::CDF_UINT2, CDF_Types::CDF_UINT1);
        case CDF_Types::CDF_UINT1:
            return source == CDF_Types::CDF_UINT1;
        case CDF_Types::CDF_NONE:
            return true;
        default:
            break;
    }
    return false;
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
    auto view = _details::ranges::py_list_or_tuple_view { input };
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
                ctx.inferred_type |= BestType { BestTypeId::Collection, 0, 0, 0 };
                std::size_t curr_inner_size
                    = static_cast<std::size_t>(PyList_Size(const_cast<PyObject*>(obj)));
                if (ctx.inner_collection_size && (curr_inner_size != *ctx.inner_collection_size))
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
                ctx.inferred_type |= BestType { BestTypeId::Collection, 0, 0, 0 };
                std::size_t curr_inner_size
                    = static_cast<std::size_t>(PyTuple_Size(const_cast<PyObject*>(obj)));
                if (ctx.inner_collection_size && (curr_inner_size != *ctx.inner_collection_size))
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
        if (!has_collection(ctx.inferred_type.inferred_type))
            result.inferred_type |= ctx.inferred_type;
    }
}

[[nodiscard]] inline analyze_result analyze_collection(const py_list_or_py_tuple auto& input)
{
    auto result = analyze_result {};
    _analyze_collection_impl(input, result);
    result.is_empty = py::len(input) == 0;
    if (has_string(result.inferred_type.inferred_type))
        result.shape.push_back(result.inferred_type.string_length);
    result.inferred_cdf_type = to_cdf_type(result.inferred_type);
    return result;
}

[[nodiscard]] inline CDF_Types to_cdf_type(const py::array& arr)
{
    if (is_dt64(arr))
    {
        return CDF_Types::CDF_TIME_TT2000;
    }
    switch (arr.dtype().kind())
    {
        case 'i':
        {
            switch (arr.dtype().itemsize())
            {
                case 1:
                    return CDF_Types::CDF_INT1;
                case 2:
                    return CDF_Types::CDF_INT2;
                case 4:
                    return CDF_Types::CDF_INT4;
                case 8:
                    return CDF_Types::CDF_INT8;
                default:
                    break;
            }
            break;
        }
        case 'u':
        {
            switch (arr.dtype().itemsize())
            {
                case 1:
                    return CDF_Types::CDF_UINT1;
                case 2:
                    return CDF_Types::CDF_UINT2;
                case 4:
                    return CDF_Types::CDF_UINT4;
                case 8:
                    // Fallback to signed 64 bits integer since CDF has no uint64 type
                    return CDF_Types::CDF_INT8;
                default:
                    break;
            }
            break;
        }
        case 'f':
        {
            switch (arr.dtype().itemsize())
            {
                case 4:
                    return CDF_Types::CDF_FLOAT;
                case 8:
                    return CDF_Types::CDF_DOUBLE;
                default:
                    break;
            }
            break;
        }
        case 'U':
        case 'S':
            return CDF_Types::CDF_UCHAR;
        default:
            break;
    }
    return CDF_Types::CDF_NONE;
}

[[nodiscard]] inline analyze_result analyze_collection(const py::array& input)
{
    auto result = analyze_result {};
    result.shape = Variable::shape_t { input.shape(), input.shape() + input.ndim() };
    result.inferred_cdf_type = to_cdf_type(input);
    return result;
}
