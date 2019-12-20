#pragma once
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
#include "cdf-endianness.hpp"
#include "cdf-enums.hpp"
#include <type_traits>
#include <cstdint>
#include <tuple>

namespace cdf {
using magic_numbers_t = std::pair<uint32_t, uint32_t>;

template <std::size_t offset_param, typename T>
struct field_t
{
    using type = T;
    inline static constexpr std::size_t len = sizeof(T);
    inline static constexpr std::size_t offset = offset_param;
    T value;
    operator T() { return value; }
    field_t& operator=(const T& v)
    {
        this->value = v;
        return *this;
    }
};

template <std::size_t offset_param, std::size_t len_param>
struct str_field_t
{
    using type = std::string;
    inline static constexpr std::size_t offset = offset_param;
    inline static constexpr std::size_t len = len_param;
    std::string value;
    operator std::string() { return value; }
    str_field_t& operator=(const std::string& v)
    {
        this->value = v;
        return *this;
    }
};

template <std::size_t offset_param, typename T>
struct unused_field_t
{
    using type = T;
    inline static constexpr std::size_t len = sizeof(T);
    inline static constexpr std::size_t offset = offset_param;
    inline static constexpr T value = 0;
    operator T() { return value; }
};

template <typename buffer_t, typename T>
void extract_field(buffer_t buffer, std::size_t offset, T& field)
{
    if constexpr (std::is_same_v<std::string, typename T::type>)
    {
        std::size_t size = 0;
        for (; size < T::len; size++)
        {
            if (buffer[T::offset - offset + size] == '\0')
                break;
        }
        field = std::string { buffer + T::offset - offset, size };
    }
    else
        field = endianness::decode<typename T::type>(buffer + T::offset - offset);
}

template <typename buffer_t, typename... Ts>
void extract_fields(buffer_t buffer, std::size_t offset, Ts&&... fields)
{
    (extract_field(buffer, offset, std::forward<Ts>(fields)), ...);
}

template <typename field_type>
inline constexpr std::size_t after = field_type::offset + sizeof(typename field_type::type);

struct v3x_tag
{
};

struct v2x_tag
{
};

template <typename version_t>
inline constexpr bool is_v3_v = std::is_same_v<version_t, v3x_tag>;

template <typename version_t, typename previous_member_t>
using cdf_offset_field_t = std::conditional_t<is_v3_v<version_t>,
    field_t<after<previous_member_t>, uint64_t>, field_t<after<previous_member_t>, uint32_t>>;

template <typename version_t, typename previous_member_t, std::size_t v3size,
    std::size_t v2size>
using cdf_string_field_t
    = std::conditional_t<is_v3_v<version_t>, str_field_t<after<previous_member_t>, v3size>,
        str_field_t<after<previous_member_t>, v2size>>;

template <typename version_t, cdf_record_type record_t>
struct cdf_DR_header
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    inline static constexpr std::size_t offset = 0;
    inline static constexpr cdf_record_type rec_type = record_t;
    using type = uint64_t;
    std::conditional_t<v3, field_t<0, uint64_t>, field_t<0, uint32_t>> record_size;
    field_t<after<decltype(record_size)>, cdf_record_type> record_type;
    template <typename buffert_t>
    bool load(buffert_t&& buffer)
    {
        extract_fields(std::forward<buffert_t>(buffer), 0, record_size, record_type);
        return record_type == record_type;
    }
};

template <template<typename, typename> typename  comp_t, typename... Ts> struct most_member_t;

template <template<typename, typename> typename  comp_t, typename T1>
struct most_member_t<comp_t,T1>: std::remove_reference_t<T1>{};

template <template<typename, typename> typename  comp_t, typename T1, typename T2>
struct most_member_t<comp_t, T1,T2>:std::conditional_t<comp_t<std::remove_reference_t<T1>, std::remove_reference_t<T2>>::value, std::remove_reference_t<T1>, std::remove_reference_t<T2>>{};

template <template<typename, typename> typename  comp_t, typename T1, typename T2, typename... Ts>
struct most_member_t<comp_t, T1, T2, Ts...> : std::conditional_t<comp_t<std::remove_reference_t<T1>, std::remove_reference_t<T2>>::value, most_member_t<comp_t, T1, Ts...>, most_member_t<comp_t, T2, Ts...>>{};


template <typename T1, typename T2>
using field_is_before_t = std::conditional_t< T1::offset <= T2::offset, std::true_type, std::false_type>;

template <typename T1, typename T2>
using field_is_after_t = std::conditional_t< T1::offset >= T2::offset, std::true_type, std::false_type>;


template <typename streamT, typename cdf_DR_t, typename... Ts>
constexpr bool load_desc_record(
    streamT&& stream, std::size_t offset, cdf_DR_t&& cdf_desc_record, Ts&&... fields)
{
    using last_member_t = most_member_t<field_is_after_t,Ts...>;
    constexpr std::size_t buffer_len = last_member_t::offset + last_member_t::len;
    char buffer[buffer_len];
    stream.seekg(offset);
    stream.read(buffer, buffer_len);
    if (cdf_desc_record.header.load(buffer))
    {
        extract_fields(buffer, 0, fields...);
        return true;
    }
    return false;
}

template <typename streamT, typename... Ts>
constexpr bool load_fields(streamT&& stream, std::size_t offset, Ts&&... fields)
{
    using last_member_t = most_member_t<field_is_after_t,Ts...>;
    using first_member_t = most_member_t<field_is_before_t,Ts...>;
    constexpr std::size_t buffer_len
        = last_member_t::offset + last_member_t::len - first_member_t::offset;
    char buffer[buffer_len];
    std::size_t pos = offset + first_member_t::offset;
    stream.seekg(pos);
    stream.read(buffer, buffer_len);
    extract_fields(buffer, first_member_t::offset, fields...);
    return true;
}

template <typename version_t>
struct cdf_CDR_t
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::CDR> header;
    cdf_offset_field_t<version_t, decltype(header)> GDRoffset;
    field_t<after<decltype(GDRoffset)>, uint32_t> Version;
    field_t<after<decltype(Version)>, uint32_t> Release;
    field_t<after<decltype(Release)>, cdf_encoding> Encoding;
    field_t<after<decltype(Encoding)>, uint32_t> Flags;
    unused_field_t<after<decltype(Flags)>, uint32_t> rfuA;
    unused_field_t<after<decltype(rfuA)>, uint32_t> rfuB;
    field_t<after<decltype(rfuB)>, uint32_t> Increment;
    field_t<after<decltype(Increment)>, uint32_t> Identifier;
    unused_field_t<after<decltype(Identifier)>, uint32_t> rfuE;
    str_field_t<after<decltype(rfuE)>, 256> copyright; // ignore format < 2.6

    template <typename streamT>
    bool load(streamT&& stream)
    {
        return load_desc_record(stream, 8, *this, GDRoffset, Version, Release, Encoding, Flags,
            Increment, Identifier, copyright);
    }
};

template <typename version_t>
struct cdf_GDR_t
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::GDR> header;
    cdf_offset_field_t<version_t, decltype(header)> rVDRhead;
    cdf_offset_field_t<version_t, decltype(rVDRhead)> zVDRhead;
    cdf_offset_field_t<version_t, decltype(zVDRhead)> ADRhead;
    cdf_offset_field_t<version_t, decltype(ADRhead)> eof;
    field_t<after<decltype(eof)>, uint32_t> NrVars;
    field_t<after<decltype(NrVars)>, uint32_t> NumAttr;
    field_t<after<decltype(NumAttr)>, uint32_t> rMaxRec;
    field_t<after<decltype(rMaxRec)>, uint32_t> rNumDims;
    field_t<after<decltype(rNumDims)>, uint32_t> NzVars;
    field_t<after<decltype(NzVars)>, uint32_t> UIRhead;
    unused_field_t<after<decltype(UIRhead)>, uint32_t> rfuC;
    field_t<after<decltype(rfuC)>, uint32_t> LeapSecondLastUpdated;
    unused_field_t<after<decltype(LeapSecondLastUpdated)>, uint32_t> rfuE;
    template <typename streamT>
    bool load(streamT&& stream, std::size_t GDRoffset)
    {
        return load_desc_record(stream, GDRoffset, *this, rVDRhead, zVDRhead, ADRhead, eof,
            NrVars, NumAttr, rMaxRec, rNumDims, NzVars, UIRhead, LeapSecondLastUpdated);
    }
};

template <typename version_t>
struct cdf_ADR_t
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::ADR> header;
    cdf_offset_field_t<version_t, decltype(header)> ADRnext;
    cdf_offset_field_t<version_t, decltype(ADRnext)> AgrEDRhead;
    field_t<after<decltype(AgrEDRhead)>, uint32_t> scope;
    field_t<after<decltype(scope)>, uint32_t> num;
    field_t<after<decltype(num)>, uint32_t> NgrEntries;
    field_t<after<decltype(NgrEntries)>, uint32_t> MAXgrEntries;
    unused_field_t<after<decltype(MAXgrEntries)>, uint32_t> rfuA;
    cdf_offset_field_t<version_t, decltype(rfuA)> AzEDRhead;
    field_t<after<decltype(AzEDRhead)>, uint32_t> NzEntries;
    field_t<after<decltype(NzEntries)>, uint32_t> MAXzEntries;
    unused_field_t<after<decltype(MAXzEntries)>, uint32_t> rfuE;
    cdf_string_field_t<version_t, decltype(rfuE), 256, 64> Name;

    template <typename streamT>
    bool load(streamT&& stream, std::size_t ADRoffset)
    {
        return load_desc_record(stream, ADRoffset, *this, ADRnext, AgrEDRhead, scope, num,
            NgrEntries, MAXgrEntries, AzEDRhead, NzEntries, MAXzEntries, Name);
    }
};

template <typename version_t>
struct cdf_AEDR_t
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::ADR> header;
    cdf_offset_field_t<version_t, decltype(header)> AEDRnext;
    field_t<after<decltype(AEDRnext)>, uint32_t> AttrNum;
    field_t<after<decltype(AttrNum)>, CDF_Types> DataType;
    field_t<after<decltype(DataType)>, uint32_t> Num;
    field_t<after<decltype(Num)>, uint32_t> NumElements;
    field_t<after<decltype(NumElements)>, uint32_t> NumStrings;
    unused_field_t<after<decltype(NumStrings)>, uint32_t> rfB;
    unused_field_t<after<decltype(rfB)>, uint32_t> rfC;
    unused_field_t<after<decltype(rfC)>, uint32_t> rfD;
    unused_field_t<after<decltype(rfD)>, uint32_t> rfE;
    field_t<after<decltype(rfE)>, uint32_t> Values;

    template <typename streamT>
    bool load(streamT&& stream, std::size_t AEDRoffset)
    {
        return load_desc_record(stream, AEDRoffset, *this, AEDRnext, AttrNum, DataType, Num,
            NumElements, NumStrings);
    }
};

}
