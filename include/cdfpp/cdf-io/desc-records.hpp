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
#include "../cdf-enums.hpp"
#include "special-fields.hpp"
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace cdf::io
{

struct v3x_tag
{
};

struct v2x_tag
{
};

struct v2_4_or_less_tag
{
};

struct v2_5_or_more_tag
{
};

template <typename version_t>
inline constexpr bool is_v3_v = std::is_same_v<version_t, v3x_tag>;

template <typename version_t>
inline constexpr bool is_v2_4_or_less_v = std::is_same_v<version_t, v2_4_or_less_tag>;

template <typename version_t>
using cdf_offset_field_t = std::conditional_t<is_v3_v<version_t>, uint64_t, uint32_t>;

template <typename version_t, std::size_t v3size, std::size_t v2size>
using cdf_string_field_t
    = std::conditional_t<is_v3_v<version_t>, string_field<v3size>, string_field<v2size>>;

template <typename version_t, cdf_record_type record_t>
struct cdf_DR_header
{
    using cdf_version_t = version_t;
    static constexpr cdf_record_type expected_record_type = record_t;
    cdf_offset_field_t<version_t> record_size;
    cdf_record_type record_type;
};

template <typename T, typename = void>
struct is_cdf_DR_header : std::false_type
{
};

template <typename T>
struct is_cdf_DR_header<T,
    decltype(std::is_same_v<cdf_DR_header<typename T::cdf_version_t, T::expected_record_type>, T>,
        void())>
        : std::is_same<cdf_DR_header<typename T::cdf_version_t, T::expected_record_type>, T>
{
};

template <typename T>
static inline constexpr bool is_cdf_DR_header_v
    = is_cdf_DR_header<std::remove_cv_t<std::remove_reference_t<T>>>::value;


template <typename version_t>
struct cdf_CDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::CDR> header;
    cdf_offset_field_t<version_t> GDRoffset;
    uint32_t Version;
    uint32_t Release;
    cdf_encoding Encoding;
    uint32_t Flags;
    unused_field<int32_t> rfuA;
    unused_field<int32_t> rfuB;
    uint32_t Increment;
    uint32_t Identifier;
    unused_field<int32_t> rfuE;
    std::conditional_t<is_v2_4_or_less_v<version_t>, string_field<1945>, string_field<256>>
        copyright;
};

template <typename version_t>
struct cdf_GDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::GDR> header;
    cdf_offset_field_t<version_t> rVDRhead;
    cdf_offset_field_t<version_t> zVDRhead;
    cdf_offset_field_t<version_t> ADRhead;
    cdf_offset_field_t<version_t> eof;
    uint32_t NrVars;
    uint32_t NumAttr;
    uint32_t rMaxRec;
    uint32_t rNumDims;
    uint32_t NzVars;
    cdf_offset_field_t<version_t> UIRhead;
    unused_field<int32_t> rfuC;
    uint32_t LeapSecondLastUpdated;
    unused_field<int32_t> rfuE;
    table_field<uint32_t, 0> rDimSizes;

    std::size_t size(const table_field<uint32_t, 0>&) const
    {
        return this->rNumDims * sizeof(uint32_t);
    }
};

template <typename version_t>
struct cdf_ADR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::ADR> header;
    cdf_offset_field_t<version_t> ADRnext;
    cdf_offset_field_t<version_t> AgrEDRhead;
    cdf_attr_scope scope;
    int32_t num;
    int32_t NgrEntries;
    int32_t MAXgrEntries;
    unused_field<int32_t> rfuA;
    cdf_offset_field_t<version_t> AzEDRhead;
    int32_t NzEntries;
    int32_t MAXzEntries;
    unused_field<int32_t> rfuE;
    cdf_string_field_t<version_t, 256, 64> Name;
};

template <typename version_t>
struct cdf_AgrEDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::AgrEDR> header;
    cdf_offset_field_t<version_t> AEDRnext;
    int32_t AttrNum;
    CDF_Types DataType;
    int32_t Num;
    int32_t NumElements;
    int32_t NumStrings;
    unused_field<int32_t> rfB;
    unused_field<int32_t> rfC;
    unused_field<int32_t> rfD;
    unused_field<int32_t> rfE;
    // table_field<uint32_t> Values;

    /*std::size_t size(const table_field<uint32_t, 0>&) const
    {
        return this->rNumDims * sizeof(uint16_t);
    }*/
};

template <typename version_t>
struct cdf_AzEDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::AzEDR> header;
    cdf_offset_field_t<version_t> AEDRnext;
    int32_t AttrNum;
    CDF_Types DataType;
    int32_t Num;
    int32_t NumElements;
    int32_t NumStrings;
    unused_field<int32_t> rfB;
    unused_field<int32_t> rfC;
    unused_field<int32_t> rfD;
    unused_field<int32_t> rfE;
    // table_field<uint32_t> Values;

    /*std::size_t size(const table_field<uint32_t, 0>&) const
    {
        return this->rNumDims * sizeof(uint16_t);
    }*/
};

template <typename... T, typename U = cdf_DR_header<T...>>
constexpr std::size_t packed_size(const U& c)
{
    return sizeof(c.record_size) + sizeof(c.record_type);
}

template <typename T>
constexpr std::size_t packed_size(const cdf_AgrEDR_t<T>& c)
{
    return packed_size(c.header) + sizeof(c.AEDRnext) + sizeof(c.AttrNum) + sizeof(c.DataType)
        + sizeof(c.Num) + sizeof(c.NumElements) + sizeof(c.NumStrings) + sizeof(c.rfB)
        + sizeof(c.rfC) + sizeof(c.rfD) + sizeof(c.rfE);
}

template <typename T>
constexpr std::size_t packed_size(const cdf_AzEDR_t<T>& c)
{
    return packed_size(c.header) + sizeof(c.AEDRnext) + sizeof(c.AttrNum) + sizeof(c.DataType)
        + sizeof(c.Num) + sizeof(c.NumElements) + sizeof(c.NumStrings) + sizeof(c.rfB)
        + sizeof(c.rfC) + sizeof(c.rfD) + sizeof(c.rfE);
}

template <cdf_r_z>
struct cdf_rzVDR_t
{
};

template <>
struct cdf_rzVDR_t<cdf_r_z::r>
{
    uint32_t rNumDims;
};

template <typename version_t>
struct cdf_rVDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::rVDR> header;
    cdf_offset_field_t<version_t> VDRnext;
    CDF_Types DataType;
    int32_t MaxRec;
    cdf_offset_field_t<version_t> VXRhead;
    cdf_offset_field_t<version_t> VXRtail;
    int32_t Flags;
    int32_t SRecords;
    unused_field<int32_t> rfuB;
    unused_field<int32_t> rfuC;
    unused_field<std::conditional_t<is_v2_4_or_less_v<version_t>, table_field<char, 2>, int32_t>>
        rfuF;
    int32_t NumElems;
    int32_t Num;
    cdf_offset_field_t<version_t> CPRorSPRoffset;
    int32_t BlockingFactor;
    cdf_string_field_t<version_t, 256, 64> Name;

    table_field<int32_t, 0> DimVarys;
    table_field<int32_t, 1> PadValues;

    std::size_t size(const table_field<int32_t, 0>&, int32_t rNumDims = 0) const
    {
        return rNumDims * sizeof(int32_t);
    }

    constexpr std::size_t size(const table_field<int32_t, 1>&) const { return 0; }
    constexpr std::size_t size(const table_field<char, 2>&) const { return 132; }
};

template <typename version_t>
struct cdf_zVDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::zVDR> header;
    cdf_offset_field_t<version_t> VDRnext;
    CDF_Types DataType;
    int32_t MaxRec;
    cdf_offset_field_t<version_t> VXRhead;
    cdf_offset_field_t<version_t> VXRtail;
    int32_t Flags;
    int32_t SRecords;
    unused_field<int32_t> rfuB;
    unused_field<int32_t> rfuC;
    unused_field<std::conditional_t<is_v2_4_or_less_v<version_t>, table_field<char, 2>, int32_t>>
        rfuF;
    int32_t NumElems;
    int32_t Num;
    cdf_offset_field_t<version_t> CPRorSPRoffset;
    int32_t BlockingFactor;
    cdf_string_field_t<version_t, 256, 64> Name;
    int32_t zNumDims;
    table_field<int32_t, 0> zDimSizes;
    table_field<int32_t, 1> DimVarys;
    table_field<int32_t, 2> PadValues;

    std::size_t size(const table_field<int32_t, 0>&) const
    {
        return this->zNumDims * sizeof(int32_t);
    }
    std::size_t size(const table_field<int32_t, 1>&) const
    {
        return this->zNumDims * sizeof(int32_t);
    }

    constexpr std::size_t size(const table_field<int32_t, 2>&) const { return 0; }
    constexpr std::size_t size(const table_field<char, 2>&) const { return 132; }
};

template <cdf_r_z type, typename version_t>
using cdf_VDR_t
    = std::conditional_t<type == cdf_r_z::r, cdf_rVDR_t<version_t>, cdf_zVDR_t<version_t>>;

template <typename version_t>
struct cdf_VXR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::VXR> header;
    cdf_offset_field_t<version_t> VXRnext;
    uint32_t Nentries;
    uint32_t NusedEntries;
    table_field<uint32_t, 0> First;
    table_field<uint32_t, 1> Last;
    table_field<cdf_offset_field_t<version_t>, 2> Offset;

    std::size_t size(const table_field<uint32_t, 0>&) const
    {
        return this->Nentries * sizeof(uint32_t);
    }
    std::size_t size(const table_field<uint32_t, 1>&) const
    {
        return this->Nentries * sizeof(uint32_t);
    }
    std::size_t size(const table_field<cdf_offset_field_t<version_t>, 2>&) const
    {
        return this->Nentries * sizeof(cdf_offset_field_t<version_t>);
    }
};

template <typename version_t>
struct cdf_VVR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::VVR> header;

    constexpr std::size_t data_size() const
    {
        return this->header.record_size - sizeof(header.record_size) - sizeof(header.record_type);
    }
};

template <typename version_t>
struct cdf_CVVR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::CVVR> header;
    unused_field<uint32_t> rfuA;
    cdf_offset_field_t<version_t> cSize;
    table_field<char, 0> data;
    std::size_t size(const table_field<char, 0>&) const { return this->cSize; }
};

template <typename version_t>
struct cdf_CCR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::CCR> header;
    cdf_offset_field_t<version_t> CPRoffset;
    cdf_offset_field_t<version_t> uSize;
    uint32_t rfuA;
    table_field<char, 0> data;
    std::size_t size(const table_field<char, 0>&) const
    {
        return this->header.record_size - sizeof(header.record_size) - sizeof(header.record_type)
            - sizeof(CPRoffset) - sizeof(uSize) - sizeof(rfuA);
    }
};

template <typename version_t>
struct cdf_CPR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    cdf_DR_header<version_t, cdf_record_type::CPR> header;
    cdf_compression_type cType;
    unused_field<uint32_t> rfuA;
    uint32_t pCount;
    table_field<uint32_t> cParms;
    std::size_t size(const table_field<uint32_t, 0>&) const
    {
        return this->pCount * sizeof(uint32_t);
    }
};

template <typename T, typename = void>
struct is_record : std::false_type
{
};

template <typename T>
struct is_record<T,
    decltype(std::declval<T>().header.record_size, std::declval<T>().header.record_type, void())>
        : std::true_type
{
};

template <typename T>
static inline constexpr bool is_record_v
    = is_record<std::remove_cv_t<std::remove_reference_t<T>>>::value;


} // namespace cdf::io
