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
#include "cdf-debug.hpp"
#include "cdf-enums.hpp"
#include "cdf-helpers.hpp"
#include "cdf-io/endianness.hpp"
#include "no_init_vector.hpp"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <stdint.h>
#include <string>
#include <variant>
#include <vector>

namespace cdf
{

struct cdf_none
{
    bool operator==([[maybe_unused]] const cdf_none& other) const { return true; }
    inline void* data() { return nullptr; }
};

using cdf_values_t = std::variant<cdf_none, no_init_vector<char>, no_init_vector<uint8_t>,
    no_init_vector<uint16_t>, no_init_vector<uint32_t>, no_init_vector<int8_t>,
    no_init_vector<int16_t>, no_init_vector<int32_t>, no_init_vector<int64_t>,
    no_init_vector<float>, no_init_vector<double>, no_init_vector<tt2000_t>, no_init_vector<epoch>,
    no_init_vector<epoch16>>;

struct data_t
{

    template <CDF_Types _type>
    decltype(auto) get();

    template <CDF_Types _type>
    decltype(auto) get() const;

    template <typename _type>
    decltype(auto) get();

    template <typename _type>
    decltype(auto) get() const;

    const char* bytes_ptr() const;
    char* bytes_ptr();

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t bytes() const noexcept;

    [[nodiscard]] CDF_Types type() const noexcept { return p_type; }

    [[nodiscard]] cdf_values_t& values() noexcept { return p_values; }
    [[nodiscard]] const cdf_values_t& values() const noexcept { return p_values; }

    data_t& operator=(data_t&& other);
    data_t& operator=(const data_t& other);

    inline bool operator==(const data_t& other) const
    {
        return other.p_type == p_type && other.p_values == p_values;
    }

    inline bool operator!=(const data_t& other) const { return !(*this == other); }

    data_t() : p_values { cdf_none {} }, p_type { CDF_Types::CDF_NONE } { }
    data_t(const data_t& other) = default;
    data_t(data_t&& other) = default;


    template <typename T, typename Dummy = void,
        typename = std::enable_if_t<!std::is_same_v<std::remove_reference_t<T>, data_t>, Dummy>>
    explicit data_t(T&& values)
            : p_values { std::forward<T>(values) }
            , p_type { to_cdf_type<typename std::remove_reference_t<T>::value_type>() }
    {
    }

    data_t(const cdf_values_t& values, CDF_Types type) : p_values { values }, p_type { type } { }
    data_t(cdf_values_t&& values, CDF_Types type) : p_values { std::move(values) }, p_type { type }
    {
    }

    template <typename... Ts>
    friend auto visit(data_t& data, Ts... lambdas);
    template <typename... Ts>
    friend auto visit(const data_t& data, Ts... lambdas);

    template <typename T, typename type>
    friend decltype(auto) _get_impl(T* self);

private:
    cdf_values_t p_values;
    CDF_Types p_type;
};

struct lazy_data
{
    lazy_data() = default;
    lazy_data(std::function<data_t(void)>&& loader, CDF_Types type)
            : p_loader { std::move(loader) }, p_type { type }
    {
    }
    lazy_data(const lazy_data&) = default;
    lazy_data(lazy_data&&) = default;
    lazy_data& operator=(const lazy_data&) = default;
    lazy_data& operator=(lazy_data&&) = default;

    [[nodiscard]] inline data_t load() { return p_loader(); }

    [[nodiscard]] inline CDF_Types type() const noexcept { return p_type; }

private:
    std::function<data_t(void)> p_loader;
    CDF_Types p_type;
};

template <typename... Ts>
auto visit(data_t& data, Ts... lambdas)
{
    return std::visit(helpers::make_visitor(lambdas...), data.p_values);
}

template <typename... Ts>
auto visit(const data_t& data, Ts... lambdas)
{
    return std::visit(helpers::make_visitor(lambdas...), data.p_values);
}

template <CDF_Types type, typename endianness_t>
auto load_values(data_t& data);

data_t load_values(data_t& data, cdf_encoding encoding);


/*=================================================================================
 Implementation
===================================================================================*/


template <CDF_Types _type>
inline decltype(auto) data_t::get()
{
    return std::get<no_init_vector<from_cdf_type_t<_type>>>(this->p_values);
}

template <CDF_Types _type>
inline decltype(auto) data_t::get() const
{
    return std::get<no_init_vector<from_cdf_type_t<_type>>>(this->p_values);
}


template <typename T, typename _type>
decltype(auto) _get_impl(T* self)
{
    return std::get<no_init_vector<_type>>(self->p_values);
}

template <typename T>
inline decltype(auto) data_t::get()
{
    return _get_impl<data_t, T>(this);
}

template <typename T>
inline decltype(auto) data_t::get() const
{
    return _get_impl<const data_t, T>(this);
}


inline data_t& data_t::operator=(data_t&& other)
{
    std::swap(this->p_values, other.p_values);
    std::swap(this->p_type, other.p_type);
    return *this;
}
inline data_t& data_t::operator=(const data_t& other)
{
    this->p_values = other.p_values;
    this->p_type = other.p_type;
    return *this;
}

// https://stackoverflow.com/questions/4059775/convert-iso-8859-1-strings-to-utf-8-in-c-c
template <typename T>
[[nodiscard]] no_init_vector<T> iso_8859_1_to_utf8(const char* buffer, std::size_t buffer_size)
{
    no_init_vector<T> out;
    out.reserve(buffer_size);
    std::for_each(buffer, buffer + buffer_size,
        [&out](const uint8_t c)
        {
            if (c < 0x80)
            {
                out.push_back(c);
            }
            else
            {
                out.push_back(0xc0 | c >> 6);
                out.push_back(0x80 | (c & 0x3f));
            }
        });
    return out;
}


template <CDF_Types _type, typename endianness_t, bool latin1_to_utf8_conv>
[[nodiscard]] inline data_t load_values(data_t&& data) noexcept
{

    if constexpr (_type == CDF_Types::CDF_CHAR
        || _type == CDF_Types::CDF_UCHAR) // special case for strings
    {
        if constexpr (latin1_to_utf8_conv)
        {
            return data_t { cdf_values_t { iso_8859_1_to_utf8<from_cdf_type_t<_type>>(
                                data.bytes_ptr(), data.bytes()) },
                _type };
        }
        else
        {
            return std::move(data);
        }
    }
    else
    {
        if (std::size(data) != 0UL)
            endianness::decode_v<endianness_t>(
                reinterpret_cast<from_cdf_type_t<_type>*>(data.bytes_ptr()), data.size());
        return std::move(data);
    }
}

template <bool iso_8859_1_to_utf8>
[[nodiscard]] inline data_t load_values(data_t&& data, cdf_encoding encoding) noexcept
{
#define DATA_FROM_T(_type)                                                                         \
    case CDF_Types::_type:                                                                         \
        if (endianness::is_big_endian_encoding(encoding))                                          \
            return load_values<CDF_Types::_type, endianness::big_endian_t, iso_8859_1_to_utf8>(    \
                std::move(data));                                                                  \
        return load_values<CDF_Types::_type, endianness::little_endian_t, iso_8859_1_to_utf8>(     \
            std::move(data));


    switch (data.type())
    {
        DATA_FROM_T(CDF_FLOAT)
        DATA_FROM_T(CDF_DOUBLE)
        DATA_FROM_T(CDF_REAL4)
        DATA_FROM_T(CDF_REAL8)
        DATA_FROM_T(CDF_EPOCH)
        DATA_FROM_T(CDF_EPOCH16)
        DATA_FROM_T(CDF_TIME_TT2000)
        DATA_FROM_T(CDF_CHAR)
        DATA_FROM_T(CDF_UCHAR)
        DATA_FROM_T(CDF_INT1)
        DATA_FROM_T(CDF_INT2)
        DATA_FROM_T(CDF_INT4)
        DATA_FROM_T(CDF_INT8)
        DATA_FROM_T(CDF_UINT1)
        DATA_FROM_T(CDF_UINT2)
        DATA_FROM_T(CDF_UINT4)
        DATA_FROM_T(CDF_BYTE)
        case CDF_Types::CDF_NONE:
            return {};
    }
    return {};
}

template <CDF_Types _type>
[[nodiscard]] cdf_values_t new_cdf_values_container(std::size_t len)
{
    using raw_type = from_cdf_type_t<_type>;
    std::size_t size = len / sizeof(raw_type);
    return cdf_values_t { no_init_vector<raw_type>(size) };
}


[[nodiscard]] inline data_t new_data_container(std::size_t bytes_len, CDF_Types _type)
{
#define DC_FROM_T(_type)                                                                           \
    case CDF_Types::_type:                                                                         \
        return data_t { new_cdf_values_container<CDF_Types::_type>(bytes_len), CDF_Types::_type };

    switch (_type)
    {
        DC_FROM_T(CDF_FLOAT)
        DC_FROM_T(CDF_DOUBLE)
        DC_FROM_T(CDF_REAL4)
        DC_FROM_T(CDF_REAL8)
        DC_FROM_T(CDF_EPOCH)
        DC_FROM_T(CDF_EPOCH16)
        DC_FROM_T(CDF_TIME_TT2000)
        DC_FROM_T(CDF_CHAR)
        DC_FROM_T(CDF_UCHAR)
        DC_FROM_T(CDF_INT1)
        DC_FROM_T(CDF_INT2)
        DC_FROM_T(CDF_INT4)
        DC_FROM_T(CDF_INT8)
        DC_FROM_T(CDF_UINT1)
        DC_FROM_T(CDF_UINT2)
        DC_FROM_T(CDF_UINT4)
        DC_FROM_T(CDF_BYTE)
        case CDF_Types::CDF_NONE:
            return {};
    }
    return {};
}


inline const char* data_t::bytes_ptr() const
{
    switch (this->type())
    {
        case cdf::CDF_Types::CDF_BYTE:
        case cdf::CDF_Types::CDF_INT1:
            return reinterpret_cast<const char*>(this->get<int8_t>().data());
            break;
        case cdf::CDF_Types::CDF_UINT1:
            return reinterpret_cast<const char*>(this->get<uint8_t>().data());
            break;
        case cdf::CDF_Types::CDF_INT2:
            return reinterpret_cast<const char*>(this->get<int16_t>().data());
            break;
        case cdf::CDF_Types::CDF_INT4:
            return reinterpret_cast<const char*>(this->get<int32_t>().data());
            break;
        case cdf::CDF_Types::CDF_INT8:
            return reinterpret_cast<const char*>(this->get<int64_t>().data());
            break;
        case cdf::CDF_Types::CDF_UINT2:
            return reinterpret_cast<const char*>(this->get<uint16_t>().data());
            break;
        case cdf::CDF_Types::CDF_UINT4:
            return reinterpret_cast<const char*>(this->get<uint32_t>().data());
            break;
        case cdf::CDF_Types::CDF_DOUBLE:
        case cdf::CDF_Types::CDF_REAL8:
            return reinterpret_cast<const char*>(this->get<double>().data());
            break;
        case cdf::CDF_Types::CDF_FLOAT:
        case cdf::CDF_Types::CDF_REAL4:
            return reinterpret_cast<const char*>(this->get<float>().data());
            break;
        case cdf::CDF_Types::CDF_EPOCH:
            return reinterpret_cast<const char*>(this->get<epoch>().data());
            break;
        case cdf::CDF_Types::CDF_EPOCH16:
            return reinterpret_cast<const char*>(this->get<epoch16>().data());
            break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
            return reinterpret_cast<const char*>(this->get<tt2000_t>().data());
            break;
        case cdf::CDF_Types::CDF_UCHAR:
            return reinterpret_cast<const char*>(this->get<cdf::CDF_Types::CDF_UCHAR>().data());
            break;
        case cdf::CDF_Types::CDF_CHAR:
            return reinterpret_cast<const char*>(this->get<cdf::CDF_Types::CDF_CHAR>().data());
            break;
        default:
            return nullptr;
            break;
    }
    return nullptr;
}

inline char* data_t::bytes_ptr()
{
    switch (this->type())
    {
        case cdf::CDF_Types::CDF_BYTE:
        case cdf::CDF_Types::CDF_INT1:
            return reinterpret_cast<char*>(this->get<int8_t>().data());
            break;
        case cdf::CDF_Types::CDF_UINT1:
            return reinterpret_cast<char*>(this->get<uint8_t>().data());
            break;
        case cdf::CDF_Types::CDF_INT2:
            return reinterpret_cast<char*>(this->get<int16_t>().data());
            break;
        case cdf::CDF_Types::CDF_INT4:
            return reinterpret_cast<char*>(this->get<int32_t>().data());
            break;
        case cdf::CDF_Types::CDF_INT8:
            return reinterpret_cast<char*>(this->get<int64_t>().data());
            break;
        case cdf::CDF_Types::CDF_UINT2:
            return reinterpret_cast<char*>(this->get<uint16_t>().data());
            break;
        case cdf::CDF_Types::CDF_UINT4:
            return reinterpret_cast<char*>(this->get<uint32_t>().data());
            break;
        case cdf::CDF_Types::CDF_DOUBLE:
        case cdf::CDF_Types::CDF_REAL8:
            return reinterpret_cast<char*>(this->get<double>().data());
            break;
        case cdf::CDF_Types::CDF_FLOAT:
        case cdf::CDF_Types::CDF_REAL4:
            return reinterpret_cast<char*>(this->get<float>().data());
            break;
        case cdf::CDF_Types::CDF_EPOCH:
            return reinterpret_cast<char*>(this->get<epoch>().data());
            break;
        case cdf::CDF_Types::CDF_EPOCH16:
            return reinterpret_cast<char*>(this->get<epoch16>().data());
            break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
            return reinterpret_cast<char*>(this->get<tt2000_t>().data());
            break;
        case cdf::CDF_Types::CDF_UCHAR:
            return reinterpret_cast<char*>(this->get<cdf::CDF_Types::CDF_UCHAR>().data());
            break;
        case cdf::CDF_Types::CDF_CHAR:
            return reinterpret_cast<char*>(this->get<cdf::CDF_Types::CDF_CHAR>().data());
            break;
        default:
            return nullptr;
            break;
    }
    return nullptr;
}

inline std::size_t data_t::size() const noexcept
{
    switch (this->type())
    {
        case cdf::CDF_Types::CDF_BYTE:
        case cdf::CDF_Types::CDF_INT1:
            return std::size(this->get<int8_t>());
            break;
        case cdf::CDF_Types::CDF_UINT1:
            return std::size(this->get<uint8_t>());
            break;
        case cdf::CDF_Types::CDF_INT2:
            return std::size(this->get<int16_t>());
            break;
        case cdf::CDF_Types::CDF_INT4:
            return std::size(this->get<int32_t>());
            break;
        case cdf::CDF_Types::CDF_INT8:
            return std::size(this->get<int64_t>());
            break;
        case cdf::CDF_Types::CDF_UINT2:
            return std::size(this->get<uint16_t>());
            break;
        case cdf::CDF_Types::CDF_UINT4:
            return std::size(this->get<uint32_t>());
            break;
        case cdf::CDF_Types::CDF_DOUBLE:
        case cdf::CDF_Types::CDF_REAL8:
            return std::size(this->get<double>());
            break;
        case cdf::CDF_Types::CDF_FLOAT:
        case cdf::CDF_Types::CDF_REAL4:
            return std::size(this->get<float>());
            break;
        case cdf::CDF_Types::CDF_EPOCH:
            return std::size(this->get<epoch>());
            break;
        case cdf::CDF_Types::CDF_EPOCH16:
            return std::size(this->get<epoch16>());
            break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
            return std::size(this->get<tt2000_t>());
            break;
        case cdf::CDF_Types::CDF_UCHAR:
            return std::size(this->get<cdf::CDF_Types::CDF_UCHAR>());
            break;
        case cdf::CDF_Types::CDF_CHAR:
            return std::size(this->get<cdf::CDF_Types::CDF_CHAR>());
            break;
        default:
            return 0;
            break;
    }
    return 0;
}

inline std::size_t data_t::bytes() const noexcept
{
    switch (this->type())
    {
        case cdf::CDF_Types::CDF_BYTE:
        case cdf::CDF_Types::CDF_INT1:
            return std::size(this->get<int8_t>());
            break;
        case cdf::CDF_Types::CDF_UINT1:
            return std::size(this->get<uint8_t>());
            break;
        case cdf::CDF_Types::CDF_INT2:
            return std::size(this->get<int16_t>()) * 2;
            break;
        case cdf::CDF_Types::CDF_INT4:
            return std::size(this->get<int32_t>()) * 4;
            break;
        case cdf::CDF_Types::CDF_INT8:
            return std::size(this->get<int64_t>()) * 8;
            break;
        case cdf::CDF_Types::CDF_UINT2:
            return std::size(this->get<uint16_t>()) * 2;
            break;
        case cdf::CDF_Types::CDF_UINT4:
            return std::size(this->get<uint32_t>()) * 4;
            break;
        case cdf::CDF_Types::CDF_DOUBLE:
        case cdf::CDF_Types::CDF_REAL8:
            return std::size(this->get<double>()) * 8;
            break;
        case cdf::CDF_Types::CDF_FLOAT:
        case cdf::CDF_Types::CDF_REAL4:
            return std::size(this->get<float>()) * 4;
            break;
        case cdf::CDF_Types::CDF_EPOCH:
            return std::size(this->get<epoch>()) * sizeof(epoch);
            break;
        case cdf::CDF_Types::CDF_EPOCH16:
            return std::size(this->get<epoch16>()) * sizeof(epoch16);
            break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
            return std::size(this->get<tt2000_t>()) * sizeof(tt2000_t);
            break;
        case cdf::CDF_Types::CDF_UCHAR:
            return std::size(this->get<cdf::CDF_Types::CDF_UCHAR>());
            break;
        case cdf::CDF_Types::CDF_CHAR:
            return std::size(this->get<cdf::CDF_Types::CDF_CHAR>());
            break;
        default:
            return 0;
            break;
    }
    return 0;
}

[[nodiscard]] constexpr bool is_string(const CDF_Types type)
{
    return type == CDF_Types::CDF_CHAR or type == CDF_Types::CDF_UCHAR;
}

}
