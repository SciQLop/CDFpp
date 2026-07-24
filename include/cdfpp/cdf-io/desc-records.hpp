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
#include <cpp_utils/endianness/endianness.hpp>
#include <cpp_utils/serde/serde.hpp>
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace cdf::io
{

// All CDF internal metadata records (CDR, GDR, ADR, xDR, VXR, VVR/CVVR/CCR/CPR
// headers, ...) are stored on disk in big-endian ("network") byte order, always —
// independent of the CDR's Encoding field, which only governs the byte order of
// actual variable *data* values inside VVR/CVVR payloads. Verified against a real
// NASA-generated fixture (tests/resources/a_cdf.cdf): its CDR's record_size field
// reads as big-endian bytes `00 00 00 00 00 00 01 38` (=312, the expected v3 CDR
// size). Every record struct below that owns at least one directly-serialized
// fundamental/enum field must declare this marker — cpp_utils::serde resolves
// endianness per the *immediate* enclosing composite of a field, not the outermost
// record, so cdf_DR_header needs its own marker too (its record_size/record_type
// fields are processed with cdf_DR_header itself as the parent composite).
using cdf_record_endianness = cpp_utils::endianness::big_endian_t;

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
using cdf_string_field_t = std::conditional_t<is_v3_v<version_t>,
    cpp_utils::serde::bounded_string<v3size>, cpp_utils::serde::bounded_string<v2size>>;

template <typename version_t, cdf_record_type record_t>
struct cdf_DR_header
{
    using cdf_version_t = version_t;
    using endianness = cdf_record_endianness;
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
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::CDR> header;
    cdf_offset_field_t<version_t> GDRoffset;
    int32_t Version;
    int32_t Release;
    cdf_encoding Encoding;
    int32_t Flags;
    cpp_utils::serde::unused<int32_t> rfuA;
    cpp_utils::serde::unused<int32_t> rfuB;
    int32_t Increment;
    int32_t Identifier;
    cpp_utils::serde::unused<int32_t> rfuE;
    std::conditional_t<is_v2_4_or_less_v<version_t>, cpp_utils::serde::bounded_string<1945>,
        cpp_utils::serde::bounded_string<256>>
        copyright;
};

template <typename version_t>
struct cdf_GDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::GDR> header;
    cdf_offset_field_t<version_t> rVDRhead;
    cdf_offset_field_t<version_t> zVDRhead;
    cdf_offset_field_t<version_t> ADRhead;
    cdf_offset_field_t<version_t> eof;
    int32_t NrVars;
    int32_t NumAttr;
    int32_t rMaxRec;
    int32_t rNumDims;
    int32_t NzVars;
    cdf_offset_field_t<version_t> UIRhead;
    cpp_utils::serde::unused<int32_t> rfuC;
    int32_t LeapSecondLastUpdated;
    cpp_utils::serde::unused<int32_t> rfuE;
    cpp_utils::serde::dynamic_array<0, int32_t> rDimSizes;

    std::size_t field_size(const cpp_utils::serde::dynamic_array<0, int32_t>&) const
    {
        return this->rNumDims;
    }
};

template <typename version_t>
struct cdf_ADR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::ADR> header;
    cdf_offset_field_t<version_t> ADRnext;
    cdf_offset_field_t<version_t> AgrEDRhead;
    cdf_attr_scope scope;
    int32_t num;
    int32_t NgrEntries;
    int32_t MAXgrEntries;
    cpp_utils::serde::unused<int32_t> rfuA;
    cdf_offset_field_t<version_t> AzEDRhead;
    int32_t NzEntries;
    int32_t MAXzEntries;
    cpp_utils::serde::unused<int32_t> rfuE;
    cdf_string_field_t<version_t, 256, 64> Name;
};

template <typename version_t>
struct cdf_AgrEDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::AgrEDR> header;
    cdf_offset_field_t<version_t> AEDRnext;
    int32_t AttrNum;
    CDF_Types DataType;
    int32_t Num;
    int32_t NumElements;
    int32_t NumStrings;
    cpp_utils::serde::unused<int32_t> rfB;
    cpp_utils::serde::unused<int32_t> rfC;
    cpp_utils::serde::unused<int32_t> rfD;
    cpp_utils::serde::unused<int32_t> rfE;
};

template <typename version_t>
struct cdf_AzEDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::AzEDR> header;
    cdf_offset_field_t<version_t> AEDRnext;
    int32_t AttrNum;
    CDF_Types DataType;
    int32_t Num;
    int32_t NumElements;
    int32_t NumStrings;
    cpp_utils::serde::unused<int32_t> rfB;
    cpp_utils::serde::unused<int32_t> rfC;
    cpp_utils::serde::unused<int32_t> rfD;
    cpp_utils::serde::unused<int32_t> rfE;
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

// Only cdf_rVDR_t's DimVarys needs state external to the record itself (the GDR's
// rNumDims) to resolve its size — every other dynamic-size field below sizes itself
// from its own fields, so a single-argument field_size(field) covers them.
template <typename version_t>
struct vdr_context_t
{
    int32_t rNumDims;
};

template <typename version_t>
struct cdf_rVDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::rVDR> header;
    cdf_offset_field_t<version_t> VDRnext;
    CDF_Types DataType;
    int32_t MaxRec;
    cdf_offset_field_t<version_t> VXRhead;
    cdf_offset_field_t<version_t> VXRtail;
    int32_t Flags;
    int32_t SRecords;
    cpp_utils::serde::unused<int32_t> rfuB;
    cpp_utils::serde::unused<int32_t> rfuC;
    cpp_utils::serde::unused<std::conditional_t<is_v2_4_or_less_v<version_t>,
        cpp_utils::serde::dynamic_array<9, char>, int32_t>>
        rfuF;
    int32_t NumElems;
    int32_t Num;
    cdf_offset_field_t<version_t> CPRorSPRoffset;
    int32_t BlockingFactor;
    cdf_string_field_t<version_t, 256, 64> Name;

    cpp_utils::serde::dynamic_array<0, int32_t> DimVarys;
    cpp_utils::serde::dynamic_array_bytes<1, char> PadValues;

    std::size_t field_size(const cpp_utils::serde::dynamic_array<0, int32_t>&,
        const vdr_context_t<version_t>& ctx) const
    {
        return ctx.rNumDims;
    }
    constexpr std::size_t field_size(const cpp_utils::serde::dynamic_array_bytes<1, char>&) const
    {
        return (Flags & 2) ? cdf_type_size(DataType) * NumElems : 0;
    }
    constexpr std::size_t field_size(const cpp_utils::serde::dynamic_array<9, char>&) const
    {
        return 132;
    }
};

template <typename version_t>
struct cdf_zVDR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::zVDR> header;
    cdf_offset_field_t<version_t> VDRnext;
    CDF_Types DataType;
    int32_t MaxRec;
    cdf_offset_field_t<version_t> VXRhead;
    cdf_offset_field_t<version_t> VXRtail;
    int32_t Flags;
    int32_t SRecords;
    cpp_utils::serde::unused<int32_t> rfuB;
    cpp_utils::serde::unused<int32_t> rfuC;
    cpp_utils::serde::unused<std::conditional_t<is_v2_4_or_less_v<version_t>,
        cpp_utils::serde::dynamic_array<9, char>, int32_t>>
        rfuF;
    int32_t NumElems;
    int32_t Num;
    cdf_offset_field_t<version_t> CPRorSPRoffset;
    int32_t BlockingFactor;
    cdf_string_field_t<version_t, 256, 64> Name;
    int32_t zNumDims;
    cpp_utils::serde::dynamic_array<0, int32_t> zDimSizes;
    cpp_utils::serde::dynamic_array<1, int32_t> DimVarys;
    cpp_utils::serde::dynamic_array_bytes<2, char> PadValues;

    std::size_t field_size(const cpp_utils::serde::dynamic_array<0, int32_t>&) const
    {
        return this->zNumDims;
    }
    std::size_t field_size(const cpp_utils::serde::dynamic_array<1, int32_t>&) const
    {
        return this->zNumDims;
    }
    constexpr std::size_t field_size(const cpp_utils::serde::dynamic_array_bytes<2, char>&) const
    {
        return (Flags & 2) ? cdf_type_size(DataType) * NumElems : 0;
    }
    constexpr std::size_t field_size(const cpp_utils::serde::dynamic_array<9, char>&) const
    {
        return 132;
    }
};

template <cdf_r_z type, typename version_t>
using cdf_VDR_t
    = std::conditional_t<type == cdf_r_z::r, cdf_rVDR_t<version_t>, cdf_zVDR_t<version_t>>;

template <typename version_t>
struct cdf_VXR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::VXR> header;
    cdf_offset_field_t<version_t> VXRnext;
    int32_t Nentries;
    int32_t NusedEntries;
    cpp_utils::serde::dynamic_array<0, int32_t> First;
    cpp_utils::serde::dynamic_array<1, int32_t> Last;
    cpp_utils::serde::dynamic_array<2, cdf_offset_field_t<version_t>> Offset;

    std::size_t field_size(const cpp_utils::serde::dynamic_array<0, int32_t>&) const
    {
        return this->Nentries;
    }
    std::size_t field_size(const cpp_utils::serde::dynamic_array<1, int32_t>&) const
    {
        return this->Nentries;
    }
    std::size_t field_size(
        const cpp_utils::serde::dynamic_array<2, cdf_offset_field_t<version_t>>&) const
    {
        return this->Nentries;
    }
};

template <typename version_t>
struct cdf_VVR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using endianness = cdf_record_endianness;
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
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::CVVR> header;
    cpp_utils::serde::unused<int32_t> rfuA;
    cdf_offset_field_t<version_t> cSize;
    cpp_utils::serde::dynamic_array_bytes<0, char> data;

    std::size_t field_size(const cpp_utils::serde::dynamic_array_bytes<0, char>&) const
    {
        return this->cSize;
    }
};

template <typename version_t>
struct cdf_CCR_t
{
    using cdf_version_t = version_t;
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::CCR> header;
    cdf_offset_field_t<version_t> CPRoffset;
    cdf_offset_field_t<version_t> uSize;
    int32_t rfuA;
    cpp_utils::serde::dynamic_array_bytes<0, char> data;

    std::size_t field_size(const cpp_utils::serde::dynamic_array_bytes<0, char>&) const
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
    using endianness = cdf_record_endianness;
    cdf_DR_header<version_t, cdf_record_type::CPR> header;
    cdf_compression_type cType;
    cpp_utils::serde::unused<int32_t> rfuA;
    int32_t pCount;
    cpp_utils::serde::dynamic_array<0, int32_t> cParms;

    std::size_t field_size(const cpp_utils::serde::dynamic_array<0, int32_t>&) const
    {
        return this->pCount;
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
