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
#include <span>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <type_traits>

#include "cdf-helpers.hpp"

namespace cdf
{

enum class cdf_r_z
{
    r = 0,
    z = 1
};

enum class cdf_majority
{
    column = 0,
    row = 1
};

[[nodiscard]] inline std::string cdf_majority_str(cdf_majority type) noexcept
{
    switch (type)
    {
        case cdf_majority::row:
            return "row";
            break;
        case cdf_majority::column:
            return "column";
            break;
        default:
            break;
    }
    return "Unknown";
}


enum class cdf_record_type : int32_t
{
    CDR = 1,
    GDR = 2,
    rVDR = 3,
    ADR = 4,
    AgrEDR = 5,
    VXR = 6,
    VVR = 7,
    zVDR = 8,
    AzEDR = 9,
    CCR = 10,
    CPR = 11,
    SPR = 12,
    CVVR = 13,
    UIR = -1,
};

enum class cdf_attr_scope : int32_t
{
    global = 1,
    variable = 2,
    global_assumed = 3,
    variable_assumed = 4,

};

enum class cdf_compression_type : int32_t
{
    no_compression = 0,
    rle_compression = 1,
    huff_compression = 2,
    ahuff_compression = 3,
    gzip_compression = 5,
#ifdef CDFPP_USE_ZSTD
    zstd_compression = 16,
#endif
};

[[nodiscard]] inline std::string cdf_compression_type_str(cdf_compression_type type) noexcept
{
    switch (type)
    {
        case cdf_compression_type::no_compression:
            return "None";
            break;
        case cdf_compression_type::ahuff_compression:
            return "Adaptative Huffman";
            break;
        case cdf_compression_type::huff_compression:
            return "Huffman";
            break;
        case cdf_compression_type::rle_compression:
            return "Run-Length Encoding";
            break;
        case cdf_compression_type::gzip_compression:
            return "GNU GZIP";
            break;
        default:
            break;
    }
    return "Unknown";
}

enum class cdf_encoding : int32_t
{
    network = 1,
    SUN = 2,
    VAX = 3,
    decstation = 4,
    SGi = 5,
    IBMPC = 6,
    IBMRS = 7,
    PPC = 9,
    HP = 11,
    NeXT = 12,
    ALPHAOSF1 = 13,
    ALPHAVMSd = 14,
    ALPHAVMSg = 15,
    ALPHAVMSi = 16,
    ARM_LITTLE = 17,
    ARM_BIG = 18,
    IA64VMSi = 19,
    IA64VMSd = 20,
    IA64VMSg = 21
};

enum class CDF_Types : uint32_t
{
    CDF_NONE = 0,
    CDF_INT1 = 1,
    CDF_INT2 = 2,
    CDF_INT4 = 4,
    CDF_INT8 = 8,
    CDF_UINT1 = 11,
    CDF_UINT2 = 12,
    CDF_UINT4 = 14,
    CDF_BYTE = 41,
    CDF_REAL4 = 21,
    CDF_REAL8 = 22,
    CDF_FLOAT = 44,
    CDF_DOUBLE = 45,
    CDF_EPOCH = 31,
    CDF_EPOCH16 = 32,
    CDF_TIME_TT2000 = 33,
    CDF_CHAR = 51,
    CDF_UCHAR = 52
};

consteval bool is_cdf_time_type(CDF_Types type)
{
    return helpers::is_in(
        type, CDF_Types::CDF_EPOCH, CDF_Types::CDF_EPOCH16, CDF_Types::CDF_TIME_TT2000);
}

consteval bool is_cdf_string_type(CDF_Types type)
{
    return helpers::is_in(type, CDF_Types::CDF_CHAR, CDF_Types::CDF_UCHAR);
}

consteval bool is_cdf_integer_type(CDF_Types type)
{
    return helpers::is_in(type, CDF_Types::CDF_INT1, CDF_Types::CDF_INT2, CDF_Types::CDF_INT4,
        CDF_Types::CDF_INT8, CDF_Types::CDF_UINT1, CDF_Types::CDF_UINT2, CDF_Types::CDF_UINT4,
        CDF_Types::CDF_BYTE);
}

consteval bool is_cdf_floating_point_type(CDF_Types type)
{
    return helpers::is_in(type, CDF_Types::CDF_REAL4, CDF_Types::CDF_REAL8, CDF_Types::CDF_FLOAT,
        CDF_Types::CDF_DOUBLE);
}

// number of nanoseconds since 01-Jan-2000 12:00:00.000.000.000  (J2000)
struct tt2000_t
{
    int64_t nseconds;
};

static inline tt2000_t operator""_tt2k(unsigned long long __val)
{
    return tt2000_t { static_cast<int64_t>(__val) };
}

inline bool operator==(const tt2000_t& lhs, const tt2000_t& rhs)
{
    return lhs.nseconds == rhs.nseconds;
}

// number of milliseconds since 01-Jan-0000 00:00:00.000
struct epoch
{
    double mseconds;
};
inline bool operator==(const epoch& lhs, const epoch& rhs)
{
    return lhs.mseconds == rhs.mseconds;
}

// number of picoseconds since 01-Jan-0000 00:00:00.000.000.000.000
struct epoch16
{
    double seconds;
    double picoseconds;
};
inline bool operator==(const epoch16& lhs, const epoch16& rhs)
{
    return lhs.seconds == rhs.seconds && lhs.picoseconds == rhs.picoseconds;
}

template <typename T>
concept cdf_time_t = std::is_same_v<T, cdf::tt2000_t> || std::is_same_v<T, cdf::epoch>
    || std::is_same_v<T, cdf::epoch16>;

template <typename T>
concept cdf_time_t_span_t = std::is_same_v<T, std::span<cdf::tt2000_t>>
    || std::is_same_v<T, std::span<cdf::epoch>> || std::is_same_v<T, std::span<cdf::epoch16>>
    || std::is_same_v<T, std::span<const cdf::tt2000_t>>
    || std::is_same_v<T, std::span<const cdf::epoch>>
    || std::is_same_v<T, std::span<const cdf::epoch16>>;

template <typename T>
concept time_point_t = requires(T t) {
    typename T::clock;
    typename T::duration;
    { t.time_since_epoch() } -> std::convertible_to<typename T::duration>;
};

template <typename T>
concept time_point_collection_t = requires(T t) {
    typename T::value_type;
    { std::size(t) } -> std::convertible_to<std::size_t>;
    { t.data() } -> std::convertible_to<typename T::value_type*>;
    requires time_point_t<typename T::value_type>;
};

template <CDF_Types type>
constexpr auto from_cdf_type()
{
    if constexpr (type == CDF_Types::CDF_CHAR)
        return char {};
    if constexpr (type == CDF_Types::CDF_UCHAR)
        return static_cast<unsigned char>(0);
    if constexpr (type == CDF_Types::CDF_BYTE)
        return int8_t {};
    if constexpr (type == CDF_Types::CDF_INT1)
        return int8_t {};
    if constexpr (type == CDF_Types::CDF_INT2)
        return int16_t {};
    if constexpr (type == CDF_Types::CDF_INT4)
        return int32_t {};
    if constexpr (type == CDF_Types::CDF_INT8)
        return int64_t {};
    if constexpr (type == CDF_Types::CDF_UINT1)
        return uint8_t {};
    if constexpr (type == CDF_Types::CDF_UINT2)
        return uint16_t {};
    if constexpr (type == CDF_Types::CDF_UINT4)
        return uint32_t {};
    if constexpr (type == CDF_Types::CDF_FLOAT)
        return float {};
    if constexpr (type == CDF_Types::CDF_DOUBLE)
        return double {};
    if constexpr (type == CDF_Types::CDF_REAL4)
        return float {};
    if constexpr (type == CDF_Types::CDF_REAL8)
        return double {};
    if constexpr (type == CDF_Types::CDF_TIME_TT2000)
        return tt2000_t {};
    if constexpr (type == CDF_Types::CDF_EPOCH)
        return epoch {};
    if constexpr (type == CDF_Types::CDF_EPOCH16)
        return epoch16 {};
}

[[nodiscard]] inline std::size_t cdf_type_size(CDF_Types type)
{
    switch (type)
    {
        case CDF_Types::CDF_NONE:
            return 0;
        case CDF_Types::CDF_INT1:
        case CDF_Types::CDF_UINT1:
        case CDF_Types::CDF_BYTE:
        case CDF_Types::CDF_CHAR:
        case CDF_Types::CDF_UCHAR:
            return 1;
        case CDF_Types::CDF_INT2:
        case CDF_Types::CDF_UINT2:
            return 2;
        case CDF_Types::CDF_INT4:
        case CDF_Types::CDF_UINT4:
        case CDF_Types::CDF_FLOAT:
        case CDF_Types::CDF_REAL4:
            return 4;
        case CDF_Types::CDF_INT8:
        case CDF_Types::CDF_REAL8:
        case CDF_Types::CDF_DOUBLE:
        case CDF_Types::CDF_EPOCH:
        case CDF_Types::CDF_TIME_TT2000:
            return 8;
        case CDF_Types::CDF_EPOCH16:
            return 16;
    }
    return 0;
}

[[nodiscard]] inline std::string cdf_type_str(CDF_Types type) noexcept
{
    switch (type)
    {
        case CDF_Types::CDF_NONE:
            return "CDF_NONE";
        case CDF_Types::CDF_INT1:
            return "CDF_INT1";
        case CDF_Types::CDF_UINT1:
            return "CDF_UINT1";
        case CDF_Types::CDF_BYTE:
            return "CDF_BYTE";
        case CDF_Types::CDF_CHAR:
            return "CDF_CHAR";
        case CDF_Types::CDF_UCHAR:
            return "CDF_UCHAR";
        case CDF_Types::CDF_INT2:
            return "CDF_INT2";
        case CDF_Types::CDF_UINT2:
            return "CDF_UINT2";
        case CDF_Types::CDF_INT4:
            return "CDF_INT1";
        case CDF_Types::CDF_UINT4:
            return "CDF_UINT4";
        case CDF_Types::CDF_FLOAT:
            return "CDF_FLOAT";
        case CDF_Types::CDF_REAL4:
            return "CDF_REAL4";
        case CDF_Types::CDF_INT8:
            return "CDF_INT8";
        case CDF_Types::CDF_REAL8:
            return "CDF_REAL8";
        case CDF_Types::CDF_DOUBLE:
            return "CDF_DOUBLE";
        case CDF_Types::CDF_TIME_TT2000:
            return "CDF_TIME_TT2000";
        case CDF_Types::CDF_EPOCH:
            return "CDF_EPOCH";
        case CDF_Types::CDF_EPOCH16:
            return "CDF_EPOCH16";
    }
    return "unknown type";
}


template <typename type>
constexpr CDF_Types to_cdf_type()
{
    if constexpr (std::is_same_v<type, float>)
        return CDF_Types::CDF_FLOAT;
    if constexpr (std::is_same_v<type, double>)
        return CDF_Types::CDF_DOUBLE;
    if constexpr (std::is_same_v<type, uint8_t>)
        return CDF_Types::CDF_UINT1;
    if constexpr (std::is_same_v<type, uint16_t>)
        return CDF_Types::CDF_UINT2;
    if constexpr (std::is_same_v<type, uint32_t>)
        return CDF_Types::CDF_UINT4;
    if constexpr (std::is_same_v<type, int8_t>)
        return CDF_Types::CDF_INT1;
    if constexpr (std::is_same_v<type, int16_t>)
        return CDF_Types::CDF_INT2;
    if constexpr (std::is_same_v<type, int32_t>)
        return CDF_Types::CDF_INT4;
    if constexpr (std::is_same_v<type, int64_t>)
        return CDF_Types::CDF_INT8;
    if constexpr (std::is_same_v<type, epoch>)
        return CDF_Types::CDF_EPOCH;
    if constexpr (std::is_same_v<type, epoch16>)
        return CDF_Types::CDF_EPOCH16;
    if constexpr (std::is_same_v<type, tt2000_t>)
        return CDF_Types::CDF_TIME_TT2000;
    else
        throw;
}


template <CDF_Types type>
using from_cdf_type_t = decltype(from_cdf_type<type>());


[[nodiscard]] inline auto cdf_type_dipatch(CDF_Types cdf_type, auto&& f, auto&&... args)
{
    switch (cdf_type)
    {
        case CDF_Types::CDF_UCHAR:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_UCHAR>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_CHAR:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_CHAR>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_BYTE:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_BYTE>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_INT1:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_INT1>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_UINT1:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_UINT1>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_INT2:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_INT2>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_UINT2:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_UINT2>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_INT4:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_INT4>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_UINT4:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_UINT4>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_INT8:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_INT8>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_FLOAT:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_FLOAT>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_REAL4:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_REAL4>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_DOUBLE:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_DOUBLE>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_REAL8:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_REAL8>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_EPOCH:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_EPOCH>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_EPOCH16:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_EPOCH16>(
            std::forward<decltype(args)>(args)...);
        case CDF_Types::CDF_TIME_TT2000:
            return  std::forward<decltype(f)>(f).template operator()<CDF_Types::CDF_TIME_TT2000>(
            std::forward<decltype(args)>(args)...);
        default:
            throw std::runtime_error { std::string { "Unsupported CDF type " }
                + std::to_string(static_cast<int>(cdf_type)) };
            break;
    }
}
}
