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
#include "../cdf-data.hpp"
#include "../cdf-endianness.hpp"
#include "../cdf-enums.hpp"
#include "../cdf-helpers.hpp"
#include "cdf-io-buffers.hpp"
#include "cdf-io-common.hpp"
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace cdf::io
{
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

template <typename T, typename cdf_dr_t>
struct table_field_t
{
    using type = T;
    std::vector<T> value;
    operator std::vector<T>() { return value; }
    std::function<std::size_t(const cdf_dr_t&)> size;
    std::function<std::size_t(const cdf_dr_t&)> offset;

    table_field_t(const decltype(size)&& size_func, const decltype(offset)&& offset_func)
            : size { size_func }, offset { offset_func }
    {
    }
};

template <typename T, typename cdf_dr_t, typename buffer_t>
bool load_table_field(table_field_t<T, cdf_dr_t>& field, buffer_t& buffer, const cdf_dr_t& dr)
{
    auto size = field.size(dr);
    if (size)
    {
        auto offset = field.offset(dr);
        field.value.resize(size);
        common::load_values<endianness::big_endian_t>(buffer, dr.offset + offset, field.value);
    }
    return true;
}

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
        field = std::string { buffer.data() + T::offset - offset, size };
    }
    else
        field = endianness::decode<endianness::big_endian_t, typename T::type>(
            buffer.data() + T::offset - offset);
}

template <typename buffer_t, typename... Ts>
void extract_fields(buffer_t buffer, [[maybe_unused]] std::size_t offset, Ts&&... fields)
{
    (extract_field(buffer, offset, std::forward<Ts>(fields)), ...);
}

template <typename field_type>
inline constexpr std::size_t after = field_type::offset + field_type::len;


#define AFTER(f) after<decltype(f)>

struct v3x_tag
{
};

struct v2x_tag
{
};

template <typename version_t>
inline constexpr bool is_v3_v = std::is_same_v<version_t, v3x_tag>;

template <typename version_t, std::size_t offset_param>
using cdf_offset_field_t = std::conditional_t<is_v3_v<version_t>, field_t<offset_param, uint64_t>,
    field_t<offset_param, uint32_t>>;

template <typename version_t, std::size_t offset_param, std::size_t v3size, std::size_t v2size>
using cdf_string_field_t = std::conditional_t<is_v3_v<version_t>, str_field_t<offset_param, v3size>,
    str_field_t<offset_param, v2size>>;

template <typename buffer_t, typename derived_t>
struct cdf_description_record
{
    bool is_loaded = false;
    buffer_t& p_buffer;
    std::size_t offset = 0;

    template <typename... Args>
    bool load(std::size_t offset, Args... opt_args)
    {
        this->offset = offset;
        is_loaded = static_cast<derived_t*>(this)->load_from(
            p_buffer, offset, std::forward<Args>(opt_args)...);
        return is_loaded;
    }

    cdf_description_record(buffer_t& buffer) : p_buffer { buffer } { }
};

template <typename version_t, cdf_record_type... record_t>
struct cdf_DR_header
{
    inline static constexpr std::size_t offset = 0;
    cdf_offset_field_t<version_t, 0> record_size;
    field_t<AFTER(record_size), cdf_record_type> record_type;
    inline static constexpr std::size_t len
        = decltype(record_type)::len + decltype(record_size)::len;
    template <typename buffert_t>
    bool load(buffert_t&& buffer)
    {
        extract_fields(std::forward<buffert_t>(buffer), 0, record_size, record_type);
        return ((record_type == record_t) || ...);
    }
};

template <template <typename, typename> typename comp_t, typename... Ts>
struct most_member_t;

template <template <typename, typename> typename comp_t, typename T1>
struct most_member_t<comp_t, T1> : std::remove_reference_t<T1>
{
};

template <template <typename, typename> typename comp_t, typename T1, typename T2>
struct most_member_t<comp_t, T1, T2>
        : std::conditional_t<
              comp_t<std::remove_reference_t<T1>, std::remove_reference_t<T2>>::value,
              std::remove_reference_t<T1>, std::remove_reference_t<T2>>
{
};

template <template <typename, typename> typename comp_t, typename T1, typename T2, typename... Ts>
struct most_member_t<comp_t, T1, T2, Ts...>
        : std::conditional_t<
              comp_t<std::remove_reference_t<T1>, std::remove_reference_t<T2>>::value,
              most_member_t<comp_t, T1, Ts...>, most_member_t<comp_t, T2, Ts...>>
{
};


template <typename T1, typename T2>
using field_is_before_t
    = std::conditional_t<T1::offset <= T2::offset, std::true_type, std::false_type>;

template <typename T1, typename T2>
using field_is_after_t
    = std::conditional_t<T1::offset >= T2::offset, std::true_type, std::false_type>;

template <typename... Args>
using first_field_t = most_member_t<field_is_before_t, Args...>;

template <typename... Args>
using last_field_t = most_member_t<field_is_after_t, Args...>;

template <typename buffer_t, typename cdf_DR_t, typename... Ts>
constexpr bool load_desc_record(
    buffer_t&& buffer, std::size_t offset, cdf_DR_t&& cdf_desc_record, Ts&&... fields)
{
    constexpr std::size_t buffer_size = [len = decltype(cdf_desc_record.header)::len]() constexpr
    {
        if constexpr (sizeof...(Ts) > 0)
        {
            using last_member_t = last_field_t<Ts...>;
            return last_member_t::offset + last_member_t::len;
        }
        else
            return len;
    }
    ();
    auto data = buffer.read(offset, buffer_size);
    if (cdf_desc_record.header.load(data))
    {
        extract_fields(data, 0, fields...);
        return true;
    }
    return false;
}

template <typename buffer_t, typename... Ts>
constexpr bool load_fields(buffer_t&& buffer, std::size_t offset, Ts&&... fields)
{
    using last_member_t = last_field_t<Ts...>;
    using first_member_t = first_field_t<Ts...>;
    constexpr std::size_t buffer_len
        = last_member_t::offset + last_member_t::len - first_member_t::offset;
    auto data = buffer.read(offset + first_member_t::offset, buffer_len);
    extract_fields(data, first_member_t::offset, fields...);
    return true;
}

template <typename version_t, typename buffer_t>
struct cdf_CDR_t : cdf_description_record<buffer_t, cdf_CDR_t<version_t, buffer_t>>
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::CDR> header;
    cdf_offset_field_t<version_t, AFTER(header)> GDRoffset;
    field_t<AFTER(GDRoffset), uint32_t> Version;
    field_t<AFTER(Version), uint32_t> Release;
    field_t<AFTER(Release), cdf_encoding> Encoding;
    field_t<AFTER(Encoding), uint32_t> Flags;
    unused_field_t<AFTER(Flags), uint32_t> rfuA;
    unused_field_t<AFTER(rfuA), uint32_t> rfuB;
    field_t<AFTER(rfuB), uint32_t> Increment;
    field_t<AFTER(Increment), uint32_t> Identifier;
    unused_field_t<AFTER(Identifier), uint32_t> rfuE;
    str_field_t<AFTER(rfuE), 256> copyright; // ignore format < 2.6

    using cdf_description_record<buffer_t, cdf_CDR_t<version_t, buffer_t>>::cdf_description_record;

    friend cdf_description_record<buffer_t, cdf_CDR_t<version_t, buffer_t>>;

protected:
    bool load_from(buffer_t& buffer, std::size_t CDRoffset = 8)
    {
        return load_desc_record(buffer, CDRoffset, *this, GDRoffset, Version, Release, Encoding,
            Flags, Increment, Identifier, copyright);
    }
};

template <typename version_t, typename buffer_t>
struct cdf_GDR_t : cdf_description_record<buffer_t, cdf_GDR_t<version_t, buffer_t>>
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::GDR> header;
    cdf_offset_field_t<version_t, AFTER(header)> rVDRhead;
    cdf_offset_field_t<version_t, AFTER(rVDRhead)> zVDRhead;
    cdf_offset_field_t<version_t, AFTER(zVDRhead)> ADRhead;
    cdf_offset_field_t<version_t, AFTER(ADRhead)> eof;
    field_t<AFTER(eof), uint32_t> NrVars;
    field_t<AFTER(NrVars), uint32_t> NumAttr;
    field_t<AFTER(NumAttr), uint32_t> rMaxRec;
    field_t<AFTER(rMaxRec), uint32_t> rNumDims;
    field_t<AFTER(rNumDims), uint32_t> NzVars;
    cdf_offset_field_t<version_t, AFTER(NzVars)> UIRhead;
    unused_field_t<AFTER(UIRhead), uint32_t> rfuC;
    field_t<AFTER(rfuC), uint32_t> LeapSecondLastUpdated;
    unused_field_t<AFTER(LeapSecondLastUpdated), uint32_t> rfuE;
    table_field_t<uint32_t, cdf_GDR_t> rDimSizes { [](auto& gdr) -> std::size_t
        { return gdr.rNumDims.value; },
        [offset = AFTER(rfuE)]([[maybe_unused]] auto& gdr) -> std::size_t { return offset; } };

    using cdf_description_record<buffer_t, cdf_GDR_t<version_t, buffer_t>>::cdf_description_record;

    friend cdf_description_record<buffer_t, cdf_GDR_t<version_t, buffer_t>>;

protected:
    bool load_from(buffer_t& buffer, std::size_t GDRoffset)
    {
        return load_desc_record(buffer, GDRoffset, *this, rVDRhead, zVDRhead, ADRhead, eof, NrVars,
                   NumAttr, rMaxRec, rNumDims, NzVars, UIRhead, LeapSecondLastUpdated)
            && load_table_field(rDimSizes, buffer, *this);
    }
};

template <typename version_t, typename buffer_t>
struct cdf_ADR_t : cdf_description_record<buffer_t, cdf_ADR_t<version_t, buffer_t>>
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::ADR> header;
    cdf_offset_field_t<version_t, AFTER(header)> ADRnext;
    cdf_offset_field_t<version_t, AFTER(ADRnext)> AgrEDRhead;
    field_t<AFTER(AgrEDRhead), cdf_attr_scope> scope;
    field_t<AFTER(scope), uint32_t> num;
    field_t<AFTER(num), uint32_t> NgrEntries;
    field_t<AFTER(NgrEntries), uint32_t> MAXgrEntries;
    unused_field_t<AFTER(MAXgrEntries), uint32_t> rfuA;
    cdf_offset_field_t<version_t, AFTER(rfuA)> AzEDRhead;
    field_t<AFTER(AzEDRhead), uint32_t> NzEntries;
    field_t<AFTER(NzEntries), uint32_t> MAXzEntries;
    unused_field_t<AFTER(MAXzEntries), uint32_t> rfuE;
    cdf_string_field_t<version_t, AFTER(rfuE), 256, 64> Name;
    using cdf_description_record<buffer_t, cdf_ADR_t<version_t, buffer_t>>::cdf_description_record;

    friend cdf_description_record<buffer_t, cdf_ADR_t<version_t, buffer_t>>;

protected:
    bool load_from(buffer_t& buffer, std::size_t ADRoffset)
    {
        return load_desc_record(buffer, ADRoffset, *this, ADRnext, AgrEDRhead, scope, num,
            NgrEntries, MAXgrEntries, AzEDRhead, NzEntries, MAXzEntries, Name);
    }
};

template <typename version_t, typename buffer_t>
struct cdf_AEDR_t : cdf_description_record<buffer_t, cdf_AEDR_t<version_t, buffer_t>>
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::AzEDR, cdf_record_type::AgrEDR> header;
    cdf_offset_field_t<version_t, AFTER(header)> AEDRnext;
    field_t<AFTER(AEDRnext), uint32_t> AttrNum;
    field_t<AFTER(AttrNum), CDF_Types> DataType;
    field_t<AFTER(DataType), uint32_t> Num;
    field_t<AFTER(Num), uint32_t> NumElements;
    field_t<AFTER(NumElements), uint32_t> NumStrings;
    unused_field_t<AFTER(NumStrings), uint32_t> rfB;
    unused_field_t<AFTER(rfB), uint32_t> rfC;
    unused_field_t<AFTER(rfC), uint32_t> rfD;
    unused_field_t<AFTER(rfD), uint32_t> rfE;
    field_t<AFTER(rfE), uint32_t> Values;

    using cdf_description_record<buffer_t, cdf_AEDR_t<version_t, buffer_t>>::cdf_description_record;
    friend cdf_description_record<buffer_t, cdf_AEDR_t<version_t, buffer_t>>;

protected:
    bool load_from(buffer_t& buffer, std::size_t AEDRoffset)
    {
        return load_desc_record(
            buffer, AEDRoffset, *this, AEDRnext, AttrNum, DataType, Num, NumElements, NumStrings);
    }
};

template <cdf_r_z>
struct cdf_rzVDR_t
{
};

template <>
struct cdf_rzVDR_t<cdf_r_z::r>
{
    uint32_t rNumDims;
};

template <cdf_r_z rz_, typename version_t, typename buffer_t>
struct cdf_VDR_t : cdf_description_record<buffer_t, cdf_VDR_t<rz_, version_t, buffer_t>>,
                   cdf_rzVDR_t<rz_>
{
    static constexpr cdf_r_z rz = rz_;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::rVDR, cdf_record_type::zVDR> header;
    cdf_offset_field_t<version_t, AFTER(header)> VDRnext;
    field_t<AFTER(VDRnext), CDF_Types> DataType;
    field_t<AFTER(DataType), uint32_t> MaxRec;
    cdf_offset_field_t<version_t, AFTER(MaxRec)> VXRhead;
    cdf_offset_field_t<version_t, AFTER(VXRhead)> VXRtail;
    field_t<AFTER(VXRtail), uint32_t> Flags;
    field_t<AFTER(Flags), uint32_t> SRecords;
    field_t<AFTER(SRecords), uint32_t> rfuB;
    field_t<AFTER(rfuB), uint32_t> rfuC;
    field_t<AFTER(rfuC), uint32_t> rfuF;
    field_t<AFTER(rfuF), uint32_t> NumElems;
    field_t<AFTER(NumElems), uint32_t> Num;
    cdf_offset_field_t<version_t, AFTER(Num)> CPRorSPRoffset;
    field_t<AFTER(CPRorSPRoffset), uint32_t> BlockingFactor;
    cdf_string_field_t<version_t, AFTER(BlockingFactor), 256, 64> Name;
    field_t<AFTER(Name), uint32_t> zNumDims;
    table_field_t<uint32_t, cdf_VDR_t> zDimSizes { [](auto& vdr) -> std::size_t
        {
            if constexpr (rz_ == cdf_r_z::z)
                return vdr.zNumDims.value;
            else
            {
                return 0;
            }
        },
        [zoffset = AFTER(zNumDims), roffset = AFTER(Name)](
            [[maybe_unused]] auto& vdr) -> std::size_t
        {
            if constexpr (rz_ == cdf_r_z::z)
                return zoffset;
            else
                return roffset;
        } };
    table_field_t<int32_t, cdf_VDR_t> DimVarys { [](auto& vdr) -> std::size_t
        {
            if constexpr (rz_ == cdf_r_z::z)
                return vdr.zNumDims.value;
            else
            {
                return vdr.rNumDims;
            }
        },
        [this](auto& vdr) -> std::size_t
        {
            if constexpr (rz_ == cdf_r_z::z)
                return vdr.zDimSizes.offset(vdr) + (vdr.zDimSizes.size(vdr) * 4);
            else
                return AFTER(this->Name);
        } };
    table_field_t<uint32_t, cdf_VDR_t> PadValues { [](auto& vdr) -> std::size_t
        {
            if ((vdr.Flags.value & 2) == 0)
                return 0;
            else
                return 0;
        },
        [](auto& vdr) -> std::size_t
        { return vdr.DimVarys.offset(vdr) + (vdr.DimVarys.size(vdr) * 4); } };

    using cdf_description_record<buffer_t,
        cdf_VDR_t<rz_, version_t, buffer_t>>::cdf_description_record;
    friend cdf_description_record<buffer_t, cdf_VDR_t<rz_, version_t, buffer_t>>;

protected:
    bool impl_load_from(
        buffer_t& buffer, std::size_t VDRoffset, [[maybe_unused]] uint32_t rNumDims = 0)
    {
        if constexpr (rz_ == cdf_r_z::r)
            this->rNumDims = rNumDims;
        return load_desc_record(buffer, VDRoffset, *this, VDRnext, DataType, MaxRec, VXRhead,
                   VXRtail, Flags, SRecords, NumElems, Num, CPRorSPRoffset, BlockingFactor, Name,
                   zNumDims)
            && load_table_field(zDimSizes, buffer, *this)
            && load_table_field(DimVarys, buffer, *this)
            && load_table_field(PadValues, buffer, *this);
    }

    template <typename Dummy = bool>
    auto load_from(buffer_t& buffer, std::size_t VDRoffset, uint32_t rNumDims)
        -> std::enable_if_t<rz_ == cdf_r_z::r, Dummy>
    {
        return impl_load_from(buffer, VDRoffset, rNumDims);
    }
    template <typename Dummy = bool>
    auto load_from(buffer_t& buffer, std::size_t VDRoffset)
        -> std::enable_if_t<rz_ == cdf_r_z::z, Dummy>
    {
        return impl_load_from(buffer, VDRoffset);
    }
};

template <typename version_t, typename buffer_t>
struct cdf_VXR_t : cdf_description_record<buffer_t, cdf_VXR_t<version_t, buffer_t>>
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::VXR> header;
    cdf_offset_field_t<version_t, AFTER(header)> VXRnext;
    field_t<AFTER(VXRnext), uint32_t> Nentries;
    field_t<AFTER(Nentries), uint32_t> NusedEntries;
    table_field_t<uint32_t, cdf_VXR_t> First { [](auto& vxr) -> std::size_t
        { return vxr.NusedEntries.value; },
        [offset = AFTER(NusedEntries)]([[maybe_unused]] auto& vxr) -> std::size_t
        { return offset; } };
    table_field_t<uint32_t, cdf_VXR_t> Last { [](auto& vxr) -> std::size_t
        { return vxr.NusedEntries.value; },
        [offset = AFTER(NusedEntries)](auto& vxr) -> std::size_t
        { return offset + (vxr.Nentries.value * 4); } };
    table_field_t<std::conditional_t<is_v3_v<version_t>, uint64_t, uint32_t>, cdf_VXR_t> Offset {
        [](auto& vxr) -> std::size_t { return vxr.NusedEntries.value; },
        [offset = AFTER(NusedEntries)](auto& vxr) -> std::size_t
        { return offset + (vxr.Nentries.value * 4 * 2); }
    };

    using cdf_description_record<buffer_t, cdf_VXR_t<version_t, buffer_t>>::cdf_description_record;
    friend cdf_description_record<buffer_t, cdf_VXR_t<version_t, buffer_t>>;

protected:
    bool load_from(buffer_t& buffer, std::size_t VXRoffset)
    {
        return load_desc_record(buffer, VXRoffset, *this, VXRnext, Nentries, NusedEntries)
            && load_table_field(First, buffer, *this) && load_table_field(Last, buffer, *this)
            && load_table_field(Offset, buffer, *this);
    }
};

template <typename version_t, typename buffer_t>
struct cdf_VVR_t : cdf_description_record<buffer_t, cdf_VVR_t<version_t, buffer_t>>
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::VVR> header;

    constexpr std::size_t data_size() const
    {
        return header.record_size.value - AFTER(header.record_type);
    }

    using cdf_description_record<buffer_t, cdf_VVR_t<version_t, buffer_t>>::cdf_description_record;
    friend cdf_description_record<buffer_t, cdf_VVR_t<version_t, buffer_t>>;

protected:
    bool load_from(buffer_t& buffer, std::size_t VVRoffset)
    {
        return load_desc_record(buffer, VVRoffset, *this);
    }
};


template <typename version_t, typename buffer_t>
struct cdf_CVVR_t : cdf_description_record<buffer_t, cdf_CVVR_t<version_t, buffer_t>>
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::CVVR> header;
    field_t<AFTER(header), uint32_t> rfuA;
    cdf_offset_field_t<version_t, AFTER(rfuA)> cSize;
    table_field_t<char, cdf_CVVR_t> data { [](const cdf_CVVR_t& cvvr) -> std::size_t
        { return static_cast<std::size_t>(cvvr.cSize.value); },
        [offset = AFTER(cSize)]([[maybe_unused]] const cdf_CVVR_t& cvvr) -> std::size_t
        { return offset; } };

    using cdf_description_record<buffer_t, cdf_CVVR_t<version_t, buffer_t>>::cdf_description_record;
    friend cdf_description_record<buffer_t, cdf_CVVR_t<version_t, buffer_t>>;

protected:
    bool load_from(buffer_t& buffer, std::size_t VVRoffset)
    {
        return load_desc_record(buffer, VVRoffset, *this, cSize)
            && load_table_field(data, buffer, *this);
    }
};


template <typename version_t, typename buffer_t>
struct cdf_mutable_variable_record_t
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using vvr_t = cdf_VVR_t<version_t, buffer_t>;
    using cvvr_t = cdf_CVVR_t<version_t, buffer_t>;
    using vxr_t = cdf_VXR_t<version_t, buffer_t>;

    std::variant<std::monostate, vvr_t, cvvr_t, vxr_t> actual_record;

    cdf_offset_field_t<version_t, 0> record_size;
    field_t<AFTER(record_size), cdf_record_type> record_type;

    bool load_from(buffer_t& buffer, std::size_t offset)
    {
        actual_record = std::monostate {};
        if (load_fields(buffer, offset, record_size, record_type))
        {
            switch (record_type.value)
            {
                case cdf_record_type::CVVR:
                    actual_record.template emplace<cvvr_t>(buffer);
                    std::get<cvvr_t>(actual_record).load(offset);
                    return true;
                    break;
                case cdf_record_type::VVR:
                    actual_record.template emplace<vvr_t>(buffer);
                    ;
                    std::get<vvr_t>(actual_record).load(offset);
                    return true;
                    break;
                case cdf_record_type::VXR:
                    actual_record.template emplace<vxr_t>(buffer);
                    std::get<vxr_t>(actual_record).load(offset);
                    return true;
                    break;
                default:
                    return false;
                    break;
            }
        }
        return false;
    }
    template <typename... Ts>
    auto visit(Ts... lambdas) const
    {
        return std::visit(helpers::make_visitor(lambdas...), actual_record);
    }
};


template <typename version_t, typename buffer_t>
struct cdf_CCR_t : cdf_description_record<buffer_t, cdf_CCR_t<version_t, buffer_t>>
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::CCR> header;
    cdf_offset_field_t<version_t, AFTER(header)> CPRoffset;
    cdf_offset_field_t<version_t, AFTER(CPRoffset)> uSize;
    field_t<AFTER(uSize), uint32_t> rfuA;
    table_field_t<char, cdf_CCR_t> data { [offset = AFTER(rfuA)](
                                              const cdf_CCR_t& ccr) -> std::size_t
        { return static_cast<std::size_t>(ccr.header.record_size.value - offset); },
        [offset = AFTER(rfuA)]([[maybe_unused]] const cdf_CCR_t& ccr) -> std::size_t
        { return offset; } };

    using cdf_description_record<buffer_t, cdf_CCR_t<version_t, buffer_t>>::cdf_description_record;
    friend cdf_description_record<buffer_t, cdf_CCR_t<version_t, buffer_t>>;

protected:
    bool load_from(buffer_t& buffer, std::size_t CCRoffset)
    {
        return load_desc_record(buffer, CCRoffset, *this, CPRoffset, uSize)
            && load_table_field(data, buffer, *this);
    }
};

template <typename version_t, typename buffer_t>
struct cdf_CPR_t : cdf_description_record<buffer_t, cdf_CPR_t<version_t, buffer_t>>
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::CPR> header;
    field_t<AFTER(header), cdf_compression_type> cType;
    field_t<AFTER(cType), uint32_t> rfuA;
    field_t<AFTER(rfuA), uint32_t> pCount;
    table_field_t<uint32_t, cdf_CPR_t> cParms { [](auto& cpr) -> std::size_t
        { return cpr.pCount.value; },
        [offset = AFTER(pCount)]([[maybe_unused]] auto& cpr) -> std::size_t { return offset; } };

    using cdf_description_record<buffer_t, cdf_CPR_t<version_t, buffer_t>>::cdf_description_record;
    friend cdf_description_record<buffer_t, cdf_CPR_t<version_t, buffer_t>>;

protected:
    bool load_from(buffer_t& buffer, std::size_t CPRoffset)
    {
        return load_desc_record(buffer, CPRoffset, *this, cType, rfuA, pCount)
            && load_table_field(cParms, buffer, *this);
    }
};

template <typename version_t, typename buffer_t>
auto begin_ADR(const cdf_GDR_t<version_t, buffer_t>& gdr)
{
    using adr_t = cdf_ADR_t<version_t, buffer_t>;
    return common::blk_iterator<adr_t, buffer_t> { gdr.ADRhead.value, gdr.p_buffer,
        [](const adr_t& adr) { return adr.ADRnext.value; } };
}

template <typename version_t, typename buffer_t>
auto end_ADR(const cdf_GDR_t<version_t, buffer_t>& gdr)
{
    return common::blk_iterator<cdf_ADR_t<version_t, buffer_t>, buffer_t> { 0, gdr.p_buffer,
        []([[maybe_unused]] const auto& adr) { return 0; } };
}

template <cdf_r_z type, typename version_t, typename buffer_t>
auto begin_AEDR(const cdf_ADR_t<version_t, buffer_t>& adr)
{
    using aedr_t = cdf_AEDR_t<version_t, buffer_t>;
    if constexpr (type == cdf_r_z::r)
    {
        return common::blk_iterator<aedr_t, buffer_t> { adr.AgrEDRhead.value, adr.p_buffer,
            [](const aedr_t& aedr) { return aedr.AEDRnext.value; } };
    }
    else if constexpr (type == cdf_r_z::z)
    {
        return common::blk_iterator<aedr_t, buffer_t> { adr.AzEDRhead.value, adr.p_buffer,
            [](const aedr_t& aedr) { return aedr.AEDRnext.value; } };
    }
}

template <cdf_r_z type, typename version_t, typename buffer_t>
auto end_AEDR(const cdf_ADR_t<version_t, buffer_t>& adr)
{
    return common::blk_iterator<cdf_AEDR_t<version_t, buffer_t>, buffer_t> { 0, adr.p_buffer,
        []([[maybe_unused]] const auto& aedr) { return 0; } };
}

template <cdf_r_z type, typename version_t, typename buffer_t>
auto begin_VDR(const cdf_GDR_t<version_t, buffer_t>& gdr)
{
    using vdr_t = cdf_VDR_t<type, version_t, buffer_t>;
    if constexpr (type == cdf_r_z::r)
    {
        return common::blk_iterator<vdr_t, buffer_t, decltype(gdr.rNumDims.value)> {
            gdr.rVDRhead.value, gdr.p_buffer, [](const vdr_t& vdr) { return vdr.VDRnext.value; },
            gdr.rNumDims.value
        };
    }
    else if constexpr (type == cdf_r_z::z)
    {
        return common::blk_iterator<vdr_t, buffer_t> { gdr.zVDRhead.value, gdr.p_buffer,
            [](const vdr_t& vdr) { return vdr.VDRnext.value; } };
    }
}

template <cdf_r_z type, typename version_t, typename buffer_t>
auto end_VDR(const cdf_GDR_t<version_t, buffer_t>& gdr)
{
    using vdr_t = cdf_VDR_t<type, version_t, buffer_t>;
    if constexpr (type == cdf_r_z::r)
    {
        return common::blk_iterator<vdr_t, buffer_t, decltype(gdr.rNumDims.value)> { 0,
            gdr.p_buffer, []([[maybe_unused]] const auto& vdr) { return 0; }, gdr.rNumDims.value };
    }
    else if constexpr (type == cdf_r_z::z)
    {
        return common::blk_iterator<vdr_t, buffer_t> { 0, gdr.p_buffer,
            []([[maybe_unused]] const auto& vdr) { return 0; } };
    }
}

template <cdf_r_z rz_, typename version_t, typename buffer_t>
auto begin_VXR(const cdf_VDR_t<rz_, version_t, buffer_t>& vdr)
{
    using vxr_t = cdf_VXR_t<version_t, buffer_t>;
    return common::blk_iterator<vxr_t, buffer_t> { vdr.VXRhead.value, vdr.p_buffer,
        [](const vxr_t& vxr) { return vxr.VXRnext.value; } };
}

template <cdf_r_z rz_, typename version_t, typename buffer_t>
auto end_VXR(const cdf_VDR_t<rz_, version_t, buffer_t>& vdr)
{
    return common::blk_iterator<cdf_VXR_t<version_t, buffer_t>, buffer_t> { 0, vdr.p_buffer,
        []([[maybe_unused]] const auto& vxr) { return 0; } };
}
}
