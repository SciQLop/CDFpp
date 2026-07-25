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

#include "../../cdf-data.hpp"
#include "../../cdf-enums.hpp"
#include "../../cdf-repr.hpp"
#include "../desc-records.hpp"
#include "../endianness.hpp"
#include "record_stream.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fmt/format.h>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>

/*
 * Formatting building blocks that replicate NASA's cdfirsdump (-full -nopage
 * -nosummary) text output byte-for-byte, extracted directly from
 * src/tools/cdfirsdump.c in the reference CDF toolkit distribution (function
 * names in comments below refer to that source). This is intentionally a
 * separate, unrelated naming/formatting convention from CDFpp's own
 * cdf_type_str/cdf_encoding_str/cdf_attr_scope_str (cdf-enums.hpp) - those
 * exist for CDFpp's own debug/repr output and use different casing and rules
 * (e.g. Scope: cdfirsdump's is a plain `==1 ? Global : Variable`, CDFpp's own
 * is a 4-way switch with "(assumed)" suffixes) - conflating the two would
 * make either one wrong for its own purpose.
 */

namespace cdf::io::debug::nasa
{

// Deci64(OFF_T): snprintf(..., "%020lld", value) - width 20, zero-padded, sign
// (if any) consumes one of the 20 slots, exactly like C's printf. Every
// offset-shaped field in cdfirsdump (record offsets, *head/*next/*tail
// pointers, CPRorSPRoffset, VXR entry Offset column, ...) goes through this
// one function - there is no field-specific offset formatting anywhere.
inline std::string nasa_offset(int64_t value)
{
    return fmt::format("{:020d}", value);
}

// DataTypeToken(long): short NASA mnemonic, "?" for anything unrecognized.
inline std::string nasa_data_type_name(CDF_Types type) noexcept
{
    using enum CDF_Types;
    switch (type)
    {
        case CDF_NONE:
            return "?";
        case CDF_BYTE:
            return "BYTE";
        case CDF_INT1:
            return "INT1";
        case CDF_INT2:
            return "INT2";
        case CDF_INT4:
            return "INT4";
        case CDF_INT8:
            return "INT8";
        case CDF_UINT1:
            return "UINT1";
        case CDF_UINT2:
            return "UINT2";
        case CDF_UINT4:
            return "UINT4";
        case CDF_REAL4:
            return "REAL4";
        case CDF_REAL8:
            return "REAL8";
        case CDF_FLOAT:
            return "FLOAT";
        case CDF_DOUBLE:
            return "DOUBLE";
        case CDF_EPOCH:
            return "EPOCH";
        case CDF_EPOCH16:
            return "EPOCH16";
        case CDF_TIME_TT2000:
            return "TT2000";
        case CDF_CHAR:
            return "CHAR";
        case CDF_UCHAR:
            return "UCHAR";
    }
    return "?";
}

// EncodingToken(long): NASA's own casing, which diverges from CDFpp's own
// cdf_encoding_str (e.g. "NETWORK"/"DECSTATION" vs "network"/"decstation").
inline std::string nasa_encoding_name(cdf_encoding encoding) noexcept
{
    using enum cdf_encoding;
    switch (encoding)
    {
        case network:
            return "NETWORK";
        case SUN:
            return "SUN";
        case VAX:
            return "VAX";
        case decstation:
            return "DECSTATION";
        case SGi:
            return "SGi";
        case IBMPC:
            return "IBMPC";
        case IBMRS:
            return "IBMRS";
        case PPC:
            return "PPC";
        case HP:
            return "HP";
        case NeXT:
            return "NeXT";
        case ALPHAOSF1:
            return "ALPHAOSF1";
        case ALPHAVMSd:
            return "ALPHAVMSd";
        case ALPHAVMSg:
            return "ALPHAVMSg";
        case ALPHAVMSi:
            return "ALPHAVMSi";
        case ARM_LITTLE:
            return "ARM_LITTLE";
        case ARM_BIG:
            return "ARM_BIG";
        case IA64VMSi:
            return "IA64VMSi";
        case IA64VMSd:
            return "IA64VMSd";
        case IA64VMSg:
            return "IA64VMSg";
    }
    return "?";
}

// cdfirsdump.c: `int32==1?"Global":"Variable"` - a strict binary rule, not a
// 4-way switch. Takes the raw on-disk int rather than cdf::cdf_attr_scope so
// it faithfully reproduces "anything other than exactly 1 renders Variable",
// including the real VARIABLE_SCOPE=2 and any stray/assumed-scope value.
inline std::string nasa_scope_name(int32_t scope) noexcept
{
    return scope == 1 ? "Global" : "Variable";
}

// cdfirsdump.c: nested ternary, 4 buckets, last one a catch-all.
inline std::string nasa_identifier_name(int32_t identifier) noexcept
{
    if (identifier == -1)
        return "CDFcore";
    if (identifier == 1)
        return "JAVA";
    if (identifier == 2)
        return "Python";
    return "Unknown";
}

// cdfirsdump.c: inline BOO chain, 3 buckets, last one an unconditional
// catch-all (any value other than 0 or 1, including negative sentinels).
inline std::string nasa_srecords_name(int32_t value) noexcept
{
    if (value == 0)
        return "No-Sparse Record";
    if (value == 1)
        return "sRecords.PAD";
    return "sRecords.PREV";
}

namespace details
{
    constexpr bool bit_set(int32_t flags, int bit) noexcept
    {
        return (flags & (int32_t { 1 } << bit)) != 0;
    }
}

// CDR Flags decode (cdfirsdump.c): "0x%lX (" + [checksum token,] + Row/Column,
// + Single/Multi + ")". The checksum token only ever appears for CDFs at
// version/release/increment >= 3.2.0 with the checksum bit set - no fixture
// in this project's test corpus exercises checksummed CDFs, so the version
// gate is accepted as a parameter defaulted to a real fixture's own
// version/release (3.9.0), rather than guessed at.
inline std::string nasa_cdr_flags_str(
    int32_t flags, int32_t version = 3, int32_t release = 9, int32_t increment = 0)
{
    using details::bit_set;
    constexpr int CDR_MAJORITY_BIT = 0;
    constexpr int CDR_FORMAT_BIT = 1;
    constexpr int CDR_CHECKSUM_BIT = 2;
    constexpr int CDR_CHECKSUM_MD5_BIT = 3;
    constexpr int CDR_CHECKSUM_OTHER_BIT = 4;

    const bool at_least_3_2_0
        = std::tie(version, release, increment) >= std::tuple { 3, 2, 0 };

    std::string s = fmt::format("0x{:x} (", flags);
    if (at_least_3_2_0 && bit_set(flags, CDR_CHECKSUM_BIT))
    {
        if (bit_set(flags, CDR_CHECKSUM_MD5_BIT))
            s += "MD5,";
        else if (bit_set(flags, CDR_CHECKSUM_OTHER_BIT))
            s += "OTHER,";
    }
    s += bit_set(flags, CDR_MAJORITY_BIT) ? "Row," : "Column,";
    s += bit_set(flags, CDR_FORMAT_BIT) ? "Single)" : "Multi)";
    return s;
}

// rVDR/zVDR Flags decode (cdfirsdump.c): all four tokens are unconditionally
// emitted, in this fixed print order (VARY, PadValue, SparseArrays,
// Compression) - which is NOT bit order (VARY=0, PadValue=1, Compression=2,
// SparseArrays=3). Each slot is a single name with a "No"/"NO" prefix glued
// on when its bit is clear.
inline std::string nasa_vdr_flags_str(int32_t flags)
{
    using details::bit_set;
    constexpr int VDR_RECVARY_BIT = 0;
    constexpr int VDR_PADVALUE_BIT = 1;
    constexpr int VDR_COMPRESSION_BIT = 2;
    constexpr int VDR_SPARSEARRAYS_BIT = 3;

    std::string s = fmt::format("0x{:x} (", flags);
    s += bit_set(flags, VDR_RECVARY_BIT) ? "VARY," : "NOVARY,";
    s += bit_set(flags, VDR_PADVALUE_BIT) ? "PadValue," : "NoPadValue,";
    s += bit_set(flags, VDR_SPARSEARRAYS_BIT) ? "SparseArrays," : "NoSparseArrays,";
    s += bit_set(flags, VDR_COMPRESSION_BIT) ? "Compression)" : "NoCompression)";
    return s;
}

// EncodeValue's float/double rendering (toolbox1.c): plain "%g", then:
//  - NaN/Inf are special-cased to bare "nan"/"inf"/"-inf" (no further edit).
//  - otherwise, if the %g output has neither 'e' nor '.', append ".0" (this
//    is also what naturally turns "-0" into "-0.0", matching cdfirsdump's
//    separately-documented negative-zero literal without needing its own
//    branch).
//  - otherwise, if it has 'e' but no '.', splice ".0" in right before the
//    'e' (so raw %g's "-1e+30" becomes "-1.0e+30").
inline std::string nasa_format_g(double value)
{
    if (std::isnan(value))
        return "nan";
    if (std::isinf(value))
        return value < 0 ? "-inf" : "inf";

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", value);
    std::string s(buf);

    if (s.find('.') != std::string::npos)
        return s;
    if (auto e_pos = s.find('e'); e_pos != std::string::npos)
        s.insert(e_pos, ".0");
    else
        s += ".0";
    return s;
}

// cdfirsdump.c: `WriteOut(OUTfp, "\nScanning records...\n\n")` then the two
// Hex32("0x%08X")-formatted magic words - unconditional preamble printed
// before the first record, once per dump (not gated on -offset, see the
// per-record header's own separate leading blank line below). This is the
// library-level text only: the "Dumping \"<path>\"\n" line cdfirsdump also
// prints is written straight to stdout (not this same output stream) by the
// tool's outer driver, so it's a CLI-level concern, not reproduced here.
template <typename buffer_t>
inline void print_nasa_magic_preamble(std::ostream& os, buffer_t& buffer)
{
    auto magic = cdf::io::debug::details::peek_magic(buffer);
    os << "\nScanning records...\n\n";
    os << fmt::format("Magic number (1): 0x{:08X}\n", magic.first);
    os << fmt::format("Magic number (2): 0x{:08X}\n", magic.second);
}

// Every record's dump begins with this pair of lines (cdfirsdump.c:
// "\nRecordSize: %lld (@ %s)\n" then "RecordType: %d (%s)\n") - the leading
// blank line is baked into the first format string, which is what produces
// exactly one blank line before every record, uniformly, including before
// the very first one (right after the magic-number preamble above).
inline void print_nasa_header(
    std::ostream& os, std::int64_t record_size, std::size_t offset, cdf::cdf_record_type type)
{
    os << '\n'
       << "RecordSize: " << record_size << " (@ " << nasa_offset(static_cast<int64_t>(offset))
       << ")\n";
    os << "RecordType: " << static_cast<int32_t>(type) << " (" << cdf::cdf_record_type_str(type)
       << ")\n";
}

template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_CDR_t<version_t>& r)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::CDR);
    os << "GDRoffset: " << nasa_offset(static_cast<int64_t>(r.GDRoffset)) << '\n';
    os << "Version: " << r.Version << '\n';
    os << "Release: " << r.Release << '\n';
    os << "Encoding: " << static_cast<int32_t>(r.Encoding) << " (" << nasa_encoding_name(r.Encoding)
       << ")\n";
    os << "Flags: " << nasa_cdr_flags_str(r.Flags, r.Version, r.Release, r.Increment) << '\n';
    // r.rfuA/rfuB/rfuE are cpp_utils::serde::unused<int32_t> - by design, the real
    // on-disk byte is discarded at decode time (always reads back as 0), regardless of
    // what the file actually holds. These three constants are each CDF's own spec-fixed
    // sentinel for that field - verified stable (0, 0, -1 respectively) across every
    // real CDR in this project's whole fixture corpus, both NASA-tool-written and
    // CDFpp-written - so hardcoding them here reproduces cdfirsdump's real output
    // faithfully without changing unused<T>'s discard-on-load behavior, which every
    // other reader/writer in this codebase relies on.
    os << "rfuA: " << 0 << '\n';
    os << "rfuB: " << 0 << '\n';
    os << "Increment: " << r.Increment << '\n';
    os << "Identifier: " << r.Identifier << "(" << nasa_identifier_name(r.Identifier) << ")\n";
    os << "rfuE: " << -1 << '\n';
    // Real, well-formed CDFs always NUL-pad the copyright field - cdfirsdump's own
    // "all NULs" check degrades to an unconditional TRUE when the field has no
    // embedded NUL at all (see nasa_compat_repr research notes), so unconditionally
    // printing this trailer matches every real fixture without re-deriving that
    // (mis-)logic from the raw on-disk bytes.
    os << "copyright...\n" << r.copyright.value << "...followed by all NULs.\n";
}

// Not verified against a real fixture: every GDR in this project's test corpus has
// rNumDims==0 (only legacy rVariables carry non-zero rNumDims, and the one rVariable
// fixture available, tests/resources/rvariable.cdf, happens to be a scalar). cdfirsdump
// likely prints an "rDimSizes[i]:"-style entry per dimension when rNumDims>0, mirroring
// zVDR's zDimSizes - but with no real output to confirm the exact placement/format
// against, rDimSizes is deliberately left unprinted here rather than guessed at.
template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_GDR_t<version_t>& r)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::GDR);
    os << "rVDRhead: " << nasa_offset(static_cast<int64_t>(r.rVDRhead)) << '\n';
    os << "zVDRhead: " << nasa_offset(static_cast<int64_t>(r.zVDRhead)) << '\n';
    os << "ADRhead: " << nasa_offset(static_cast<int64_t>(r.ADRhead)) << '\n';
    os << "eof: " << nasa_offset(static_cast<int64_t>(r.eof)) << '\n';
    os << "NumRvars: " << r.NrVars << '\n';
    os << "NumAttr: " << r.NumAttr << '\n';
    os << "rMaxRec: " << r.rMaxRec << '\n';
    os << "rNumDims: " << r.rNumDims << '\n';
    os << "NumZvars: " << r.NzVars << '\n';
    os << "UIRhead: " << nasa_offset(static_cast<int64_t>(r.UIRhead)) << '\n';
    os << "rfuC: " << 0 << '\n'; // see the CDR rfu* comment above: unused<T> always
                                 // decodes to 0, and 0 is this field's verified sentinel too
    os << "LeapSecondLastUpdated: " << r.LeapSecondLastUpdated << '\n';
    os << "rfuE: " << -1 << '\n'; // verified sentinel, see the CDR rfu* comment above
}

template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_ADR_t<version_t>& r)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::ADR);
    os << "ADRnext: " << nasa_offset(static_cast<int64_t>(r.ADRnext)) << '\n';
    os << "AgrEDRhead: " << nasa_offset(static_cast<int64_t>(r.AgrEDRhead)) << '\n';
    os << "Scope: " << static_cast<int32_t>(r.scope) << " (" << nasa_scope_name(static_cast<int32_t>(r.scope))
       << ")\n";
    os << "Num: " << r.num << '\n';
    os << "NumRentries: " << r.NgrEntries << '\n';
    os << "MaxRentry: " << r.MAXgrEntries << '\n';
    os << "rfuA: " << 0 << '\n'; // verified sentinel, see the CDR rfu* comment above
    os << "AzEDRhead: " << nasa_offset(static_cast<int64_t>(r.AzEDRhead)) << '\n';
    os << "NumZentries: " << r.NzEntries << '\n';
    os << "MaxZentry: " << r.MAXzEntries << '\n';
    os << "rfuE: " << -1 << '\n'; // verified sentinel, see the CDR rfu* comment above
    os << "Name: \"" << r.Name.value << "\"\n";
}

namespace details
{
    inline cpp_utils::endianness::Endianness nasa_runtime_endianness(cdf_encoding encoding)
    {
        return cdf::endianness::is_big_endian_encoding(encoding)
            ? cpp_utils::endianness::Endianness::big
            : cpp_utils::endianness::Endianness::little;
    }

    // Mirrors ConvertBuffer + the host-decoded scalar cdfirsdump reads before formatting an
    // attribute entry Value (unlike PadValue, which hex-dumps the *raw* undecoded bytes - see
    // the PadValue-vs-Value byte-order note once rVDR/zVDR printing is added).
    template <typename T>
    inline T nasa_decode_scalar(const char* bytes, cdf_encoding encoding)
    {
        T tmp;
        std::memcpy(&tmp, bytes, sizeof(T));
        return cpp_utils::endianness::decode<T>(nasa_runtime_endianness(encoding), &tmp);
    }

    inline std::string nasa_decode_numeric_element(
        CDF_Types type, const char* bytes, cdf_encoding encoding)
    {
        using enum CDF_Types;
        switch (type)
        {
            case CDF_INT1:
            case CDF_BYTE:
                return std::to_string(
                    static_cast<int>(nasa_decode_scalar<int8_t>(bytes, encoding)));
            case CDF_INT2:
                return std::to_string(nasa_decode_scalar<int16_t>(bytes, encoding));
            case CDF_INT4:
                return std::to_string(nasa_decode_scalar<int32_t>(bytes, encoding));
            case CDF_INT8:
                return std::to_string(nasa_decode_scalar<int64_t>(bytes, encoding));
            case CDF_UINT1:
                return std::to_string(
                    static_cast<unsigned>(nasa_decode_scalar<uint8_t>(bytes, encoding)));
            case CDF_UINT2:
                return std::to_string(nasa_decode_scalar<uint16_t>(bytes, encoding));
            case CDF_UINT4:
                return std::to_string(nasa_decode_scalar<uint32_t>(bytes, encoding));
            case CDF_REAL4:
            case CDF_FLOAT:
                return nasa_format_g(
                    static_cast<double>(nasa_decode_scalar<float>(bytes, encoding)));
            case CDF_REAL8:
            case CDF_DOUBLE:
                return nasa_format_g(nasa_decode_scalar<double>(bytes, encoding));
            default:
                return "?";
        }
    }

    // EncodeValue's epoch-family rendering (toolbox1.c): the raw floating/integer
    // component(s) via bare "%g"/"%lld" (no nasa_format_g splice-rule post-processing -
    // that only applies to PadValue's own float rendering), then a literal " ==> ", then
    // the same ISO string cdf-repr.hpp's own epoch/epoch16/tt2000_t operator<< already
    // produces (verified identical, including the fill-sentinel special cases). Only the
    // first element is rendered when NumElems > 1 (real fixtures with e.g. 11-element
    // date attributes show cdfirsdump's own MAX_SCREENLINE_LEN truncation kick in after
    // element 1 every time observed - the exact byte-budget arithmetic for partially-fitting
    // cases isn't independently re-derived here, so this simplification may diverge from
    // NASA's tool for an in-between element count that would only partially truncate).
    inline std::string nasa_epoch_family_value_str(
        CDF_Types type, int32_t num_elements, cdf_encoding encoding, const char* raw)
    {
        std::ostringstream oss;
        if (type == CDF_Types::CDF_EPOCH)
        {
            double v = nasa_decode_scalar<double>(raw, encoding);
            char g[64];
            std::snprintf(g, sizeof(g), "%g", v);
            std::ostringstream iso_oss;
            iso_oss << cdf::epoch { v };
            // cdf::epoch's operator<< goes through to_time_point for any non-sentinel
            // value, whose underlying time_point has nanosecond duration resolution -
            // 9 fractional digits, wider than encodeEPOCH4's fixed 3-digit millisecond
            // width cdfirsdump always uses. Both of operator<<'s own hardcoded sentinel
            // strings are already exactly 3 digits, so unconditionally keeping only the
            // first 23 characters ("yyyy-mm-ddThh:mm:ss.mmm") is a no-op for those and
            // the needed truncation for everything else.
            oss << g << " ==> " << iso_oss.str().substr(0, 23);
        }
        else if (type == CDF_Types::CDF_EPOCH16)
        {
            double s = nasa_decode_scalar<double>(raw, encoding);
            double p = nasa_decode_scalar<double>(raw + sizeof(double), encoding);
            char gs[64];
            char gp[64];
            std::snprintf(gs, sizeof(gs), "%g", s);
            std::snprintf(gp, sizeof(gp), "%g", p);
            oss << gs << ' ' << gp << " ==> " << cdf::epoch16 { s, p };
            // cdf::epoch16's operator<< goes through to_time_point, which folds
            // picoseconds down to nanoseconds (ns = picoseconds/1000, truncated) before
            // formatting - correct for CDFpp's own repr purposes, but it only ever
            // prints 9 sub-second digits for a non-sentinel value, 3 short of
            // cdfirsdump's fixed 12-digit (ms+us+ns+ps) EPOCH16 width. The two
            // special-cased sentinel strings (all-zero, fill -1e31/-1e31) are already
            // 12 digits, so only append the missing "sub-nanosecond" remainder
            // (picoseconds mod 1000) for everything else, rather than reimplementing
            // the date math independently here.
            if (!((s == -1e31 && p == -1e31) || (s == 0 && p == 0)))
                oss << fmt::format("{:03}", static_cast<int64_t>(p) % 1000);
        }
        else
        {
            int64_t v = nasa_decode_scalar<int64_t>(raw, encoding);
            oss << v << " ==> " << cdf::tt2000_t { v };
        }
        if (num_elements > 1)
            oss << ", ...";
        return oss.str();
    }
}

// EncodeValue/EncodeString's attribute-entry Value rendering (cdfirsdump.c): CHAR/UCHAR
// gets a quoted string, plus a forward-order hex dump when NumElems<25 (unlike PadValue's
// reversed-order hex - here the hex runs over the already-ConvertBuffer'd bytes, and for
// CHAR/UCHAR that conversion is a byte-order no-op anyway, so forward-vs-reversed is only
// externally visible for non-string types, which never get a hex block at all). Epoch-family
// types get the special raw+arrow+ISO rendering. Everything else is a plain comma-separated
// decoded numeric list, no hex, no arrow.
inline std::string nasa_attribute_value_str(CDF_Types type, int32_t num_elements,
    cdf_encoding encoding, const char* raw, std::size_t raw_size)
{
    using enum CDF_Types;
    if (type == CDF_CHAR || type == CDF_UCHAR)
    {
        std::string full(raw, raw_size);
        std::string out = "\"" + full.substr(0, full.find('\0')) + "\"";
        if (num_elements < 25)
        {
            out += " (";
            for (unsigned char c : full)
                out += fmt::format("{:02X}", static_cast<unsigned>(c));
            out += ")";
        }
        return out;
    }
    if (type == CDF_EPOCH || type == CDF_EPOCH16 || type == CDF_TIME_TT2000)
        return details::nasa_epoch_family_value_str(type, num_elements, encoding, raw);

    const auto element_size = cdf_type_size(type);
    if (element_size == 0 || raw_size < element_size)
        return "?";
    const auto count = raw_size / element_size;
    std::string out;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (i)
            out += ", ";
        out += details::nasa_decode_numeric_element(type, raw + i * element_size, encoding);
    }
    return out;
}

namespace details
{
    // Shared by AgrEDR/AzEDR: identical field layout and printing, except the *label* of
    // the leading next-record-pointer field ("AgrEDRnext" vs "AzEDRnext" - the underlying
    // struct field is named AEDRnext in both). NumStrings/rfuA are the same physical
    // on-disk field, real (not unused<T>-wrapped) data either way - cdfirsdump just labels
    // it "NumStrings" for string-typed entries and "rfuA" otherwise, so this always prints
    // the real r.NumStrings value, only the label varies.
    template <typename record_t>
    inline void print_nasa_aedr_body(
        std::ostream& os, const record_t& r, const char* next_label, cdf_encoding encoding)
    {
        os << next_label << ": " << nasa_offset(static_cast<int64_t>(r.AEDRnext)) << '\n';
        os << "AttrNum: " << r.AttrNum << '\n';
        os << "DataType: " << static_cast<int32_t>(r.DataType) << " ("
           << nasa_data_type_name(r.DataType) << ")\n";
        os << "Num: " << r.Num << '\n';
        os << "NumElems: " << r.NumElements << '\n';
        os << (cdf::is_string(r.DataType) ? "NumStrings: " : "rfuA: ") << r.NumStrings << '\n';
        os << "rfuB: " << 0 << '\n'; // verified sentinel, see the CDR rfu* comment above
        os << "rfuC: " << 0 << '\n'; // verified sentinel, see the CDR rfu* comment above
        os << "rfuD: " << -1 << '\n'; // verified sentinel, see the CDR rfu* comment above
        os << "rfuE: " << -1 << '\n'; // verified sentinel, see the CDR rfu* comment above
        os << "Value: "
           << nasa_attribute_value_str(
                  r.DataType, r.NumElements, encoding, r.Value.data(), r.Value.size())
           << '\n';
    }
}

template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset,
    const cdf::io::cdf_AgrEDR_t<version_t>& r, cdf_encoding encoding)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::AgrEDR);
    details::print_nasa_aedr_body(os, r, "AgrEDRnext", encoding);
}

template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset,
    const cdf::io::cdf_AzEDR_t<version_t>& r, cdf_encoding encoding)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::AzEDR);
    details::print_nasa_aedr_body(os, r, "AzEDRnext", encoding);
}

namespace details
{
    inline std::string nasa_hex_bytes(const char* raw, std::size_t n, bool reversed)
    {
        std::string out;
        if (reversed)
            for (std::size_t i = n; i-- > 0;)
                out += fmt::format("{:02X}", static_cast<unsigned char>(raw[i]));
        else
            for (std::size_t i = 0; i < n; ++i)
                out += fmt::format("{:02X}", static_cast<unsigned char>(raw[i]));
        return out;
    }

    // The decoded portion of a PadValue - same per-type decode as an attribute Value
    // (nasa_attribute_value_str), but a single value only (no comma list, no raw+"==>"
    // prefix - PadValues in every real fixture examined have NumElems==1 for every
    // non-CHAR type, CHAR being the one case with NumElems>1, where it's a string
    // width rather than a repeat count).
    inline std::string nasa_decode_single_value(
        CDF_Types type, cdf_encoding encoding, const char* raw, std::size_t raw_size)
    {
        using enum CDF_Types;
        if (type == CDF_CHAR || type == CDF_UCHAR)
        {
            std::string full(raw, raw_size);
            return "\"" + full.substr(0, full.find('\0')) + "\"";
        }
        if (type == CDF_EPOCH)
        {
            double v = nasa_decode_scalar<double>(raw, encoding);
            std::ostringstream oss;
            oss << cdf::epoch { v };
            return oss.str().substr(0, 23);
        }
        if (type == CDF_EPOCH16)
        {
            double s = nasa_decode_scalar<double>(raw, encoding);
            double p = nasa_decode_scalar<double>(raw + sizeof(double), encoding);
            std::ostringstream oss;
            oss << cdf::epoch16 { s, p };
            std::string iso = oss.str();
            if (!((s == -1e31 && p == -1e31) || (s == 0 && p == 0)))
                iso += fmt::format("{:03}", static_cast<int64_t>(p) % 1000);
            return iso;
        }
        if (type == CDF_TIME_TT2000)
        {
            int64_t v = nasa_decode_scalar<int64_t>(raw, encoding);
            std::ostringstream oss;
            oss << cdf::tt2000_t { v };
            return oss.str();
        }
        return nasa_decode_numeric_element(type, raw, encoding);
    }
}

// rVDR/zVDR PadValue rendering (cdfirsdump.c): "PadValue: " + (unless CDF_EPOCH16)
// " (0x<hex>) " + decoded-value. The hex is over the *raw*, pre-ConvertBuffer bytes,
// reversed-byte-order for every type except CHAR/UCHAR (forward) - the opposite
// convention from an attribute Value's hex (always forward, over *converted* bytes) -
// and unlike Value, PadValue's hex block has no NumElems<25 gate, it's unconditional
// (only CDF_EPOCH16 skips it entirely). "PadValue: " plus the hex block's own leading
// space is what produces the real tool's double-space ("PadValue:  (0x...")); the
// EPOCH16 case naturally keeps a single space since it returns straight to the decoded
// text with no leading space of its own.
inline std::string nasa_pad_value_str(
    CDF_Types type, cdf_encoding encoding, const char* raw, std::size_t raw_size)
{
    std::string decoded = details::nasa_decode_single_value(type, encoding, raw, raw_size);
    if (type == CDF_Types::CDF_EPOCH16)
        return decoded;
    const bool is_string = type == CDF_Types::CDF_CHAR || type == CDF_Types::CDF_UCHAR;
    return " (0x" + details::nasa_hex_bytes(raw, raw_size, /*reversed=*/!is_string) + ") " + decoded;
}

namespace details
{
    // Shared by rVDR/zVDR up to and including Name - identical layout apart from the
    // leading next-record-pointer label ("rVDRnext" vs "zVDRnext", underlying field
    // VDRnext in both).
    template <typename record_t>
    inline void print_nasa_vdr_body(
        std::ostream& os, const record_t& r, const char* next_label, cdf_encoding encoding)
    {
        (void)encoding;
        os << next_label << ": " << nasa_offset(static_cast<int64_t>(r.VDRnext)) << '\n';
        os << "DataType: " << static_cast<int32_t>(r.DataType) << " ("
           << nasa_data_type_name(r.DataType) << ")\n";
        os << "MaxRec: " << r.MaxRec << '\n';
        os << "VXRhead: " << nasa_offset(static_cast<int64_t>(r.VXRhead)) << '\n';
        os << "VXRtail: " << nasa_offset(static_cast<int64_t>(r.VXRtail)) << '\n';
        os << "Flags: " << nasa_vdr_flags_str(r.Flags) << '\n';
        os << "sRecords: " << r.SRecords << " (" << nasa_srecords_name(r.SRecords) << ")\n";
        os << "rfuB: " << 0 << '\n'; // verified sentinel, see the CDR rfu* comment above
        os << "rfuC: " << -1 << '\n'; // verified sentinel, see the CDR rfu* comment above
        os << "rfuF: " << -1 << '\n'; // verified sentinel, see the CDR rfu* comment above
        os << "NumElems: " << r.NumElems << '\n';
        os << "Num: " << r.Num << '\n';
        os << "CPRorSPRoffset: " << nasa_offset(static_cast<int64_t>(r.CPRorSPRoffset)) << '\n';
        os << "BlockingFactor: " << r.BlockingFactor << '\n';
        os << "Name: \"" << r.Name.value << "\"\n";
    }

    // zVDR-only: zNumDims plus a per-dimension zDimSizes[i]/DimVarys[i] pair. rVDR has
    // no zNumDims field of its own - its dims come from the GDR's rNumDims, already
    // folded into r.DimVarys's real element count via the walker's vdr_context_t
    // threading - and no real fixture shows a header count line for it either. Every
    // rVariable fixture in this project's corpus happens to be a 0-dim scalar, so the
    // rVDR (DimVarys-only, no zDimSizes) path below isn't independently verified
    // against a real non-empty case, only the zVDR path is.
    template <typename record_t>
    inline void print_nasa_vdr_dims(std::ostream& os, const record_t& r)
    {
        if constexpr (requires { r.zDimSizes; })
        {
            os << "zNumDims: " << r.zNumDims << '\n';
            for (std::size_t i = 0; i < r.zDimSizes.size(); ++i)
                os << " zDimSizes[" << i << "]: " << r.zDimSizes[i] << '\n';
        }
        for (std::size_t i = 0; i < r.DimVarys.size(); ++i)
            os << " DimVarys[" << i << "]: " << r.DimVarys[i] << " ("
               << (r.DimVarys[i] != 0 ? "T" : "F") << ")\n";
    }

    template <typename record_t>
    inline void print_nasa_vdr_padvalue(std::ostream& os, const record_t& r, cdf_encoding encoding)
    {
        if (r.Flags & 2)
            os << "PadValue: "
               << nasa_pad_value_str(
                      r.DataType, encoding, r.PadValues.data(), r.PadValues.size())
               << '\n';
    }
}

template <typename version_t>
inline void print_nasa(
    std::ostream& os, std::size_t offset, const cdf::io::cdf_zVDR_t<version_t>& r, cdf_encoding encoding)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::zVDR);
    details::print_nasa_vdr_body(os, r, "zVDRnext", encoding);
    details::print_nasa_vdr_dims(os, r);
    details::print_nasa_vdr_padvalue(os, r, encoding);
}

template <typename version_t>
inline void print_nasa(
    std::ostream& os, std::size_t offset, const cdf::io::cdf_rVDR_t<version_t>& r, cdf_encoding encoding)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::rVDR);
    details::print_nasa_vdr_body(os, r, "rVDRnext", encoding);
    details::print_nasa_vdr_dims(os, r);
    details::print_nasa_vdr_padvalue(os, r, encoding);
}

template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_VXR_t<version_t>& r)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::VXR);
    os << "VXRnext: " << nasa_offset(static_cast<int64_t>(r.VXRnext)) << '\n';
    os << "Nentries: " << r.Nentries << '\n';
    os << "NusedEntries: " << r.NusedEntries << '\n';
    os << '\n';
    os << "  Entry  FirstRec  LastRec           Offset\n";
    // cdfirsdump clamps its own display loop to MAX_VXR_ENTRIES=10 regardless of the
    // raw Nentries value (no fixture in this project's corpus has more than 7, the
    // usual default table size, so the clamp itself isn't independently exercised,
    // just applied defensively to match the documented behavior).
    const auto n = std::min(r.First.size(), std::size_t { 10 });
    for (std::size_t i = 0; i < n; ++i)
    {
        os << fmt::format("{:>7}{:>10}{:>9}      {}\n", i, r.First[i], r.Last[i],
            nasa_offset(static_cast<int64_t>(r.Offset[i])));
    }
}

template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_VVR_t<version_t>& r)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::VVR);
    os << "uSize: " << r.data_size() << '\n';
}

template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_CVVR_t<version_t>& r)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::CVVR);
    os << "cSize: " << r.cSize << '\n';
    // CVVR is the one record type whose own format ends with an unconditional trailing
    // blank line (cdfirsdump.c's own WriteOut(OUTfp,"\n") after cSize, independent of
    // -data) - every other record type here relies solely on the *next* record's
    // leading blank line for separation. Verified against a real fixture: two blank
    // lines appear between a CVVR and whatever follows it, not one.
    os << '\n';
}

template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_CPR_t<version_t>& r)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::CPR);
    os << "cType: " << static_cast<int32_t>(r.cType) << '\n';
    os << "rfuA: " << 0 << '\n'; // verified sentinel, see the CDR rfu* comment above
    os << "pCount: " << r.pCount << '\n';
    for (std::size_t i = 0; i < r.cParms.size(); ++i)
        os << "  cParms[" << i << "]: " << r.cParms[i] << '\n';
}

template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_CCR_t<version_t>& r)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::CCR);
    os << "CPRoffset: " << nasa_offset(static_cast<int64_t>(r.CPRoffset)) << '\n';
    os << "uSize: " << r.uSize << '\n'; // not offset-formatted, unlike CPRoffset - verified
                                        // against a real fixture ("uSize: 123062", no
                                        // zero-padding)
    os << "rfuA: " << 0 << '\n'; // verified sentinel, see the CDR rfu* comment above
    // Unconditional at -full/-most level regardless of -data (cdfirsdump.c) - the CCR
    // payload is a raw deflate stream the tool never attempts to inflate/dump inline.
    os << "Skipping compressed IRs...\n";
}

template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_UIR_t<version_t>& r)
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::UIR);
    os << "Next: " << nasa_offset(static_cast<int64_t>(r.Next)) << '\n';
    os << "Prev: " << nasa_offset(static_cast<int64_t>(r.Prev)) << '\n';
}

// SPR (sparse-arrays parameters) is the one record type this project has no struct for
// at all (CDFpp doesn't support sparse arrays - see the project's own finding on this),
// so it always arrives here as the generic undecoded_record_t placeholder, same as the
// plain record_repr.hpp printer's own "[not decoded]" convention.
inline void print_nasa(
    std::ostream& os, std::size_t offset, const cdf::io::debug::undecoded_record_t& r)
{
    print_nasa_header(os, static_cast<std::int64_t>(r.record_size), offset, r.type);
    os << "[not decoded - CDFpp doesn't model this record type]\n";
}

// Top-level entry point: reproduces a full `cdfirsdump -full -nopage -nosummary` run
// against `path`, byte-for-byte (verified against a real captured reference dump of
// tests/resources/a_cdf.cdf - see tests/nasa_compat_repr). Only the CLI-level
// "Dumping \"<path>\"\n" line is not reproduced here (see print_nasa_magic_preamble's
// own comment - that line goes to a different output stream in the real tool, so it's
// a caller's concern, not this library's).
inline void dump(std::ostream& os, const std::string& path)
{
    auto buffer = cdf::io::buffers::make_shared_file_adapter(path);
    print_nasa_magic_preamble(os, buffer);
    cdf_encoding encoding = cdf_encoding::network;
    for_each_record(buffer,
        [&](std::size_t offset, const auto& record)
        {
            if constexpr (requires { record.Encoding; })
                encoding = record.Encoding;
            if constexpr (requires { print_nasa(os, offset, record, encoding); })
                print_nasa(os, offset, record, encoding);
            else
                print_nasa(os, offset, record);
        });
}

inline std::string dump(const std::string& path)
{
    std::ostringstream oss;
    dump(oss, path);
    return oss.str();
}

} // namespace cdf::io::debug::nasa
