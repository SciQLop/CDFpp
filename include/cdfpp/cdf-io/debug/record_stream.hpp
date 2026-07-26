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

#include "../common.hpp"
#include "../decompression.hpp"
#include "../desc-records.hpp"
#include "../loading/buffers.hpp"
#include "../loading/records-loading.hpp"
#include <cdfpp/no_init_vector.hpp>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

/*
 * A stream parser: walks a CDF file's records in *physical* disk order (record_size
 * stepping from one header to the next) rather than by following the ADRhead/VDRhead/
 * VXRnext pointer chains that reconstruct the logical variable/attribute graph (see
 * records-loading.hpp's begin_ADR/begin_VDR/begin_VXR for that). CDF records are laid
 * out back-to-back with no gaps between them, and every record starts with the same
 * cdf_DR_header (record_size then record_type), so this needs no knowledge of which
 * record belongs to which variable - just "read the next self-declared record_size
 * bytes, dispatch on record_type, repeat". That also means it surfaces records the
 * semantic loader silently discards (UIR - freed space CDF leaves behind rather than
 * compacting) and tolerates record types CDFpp doesn't model yet (SPR) by skipping
 * them via their declared size rather than refusing to progress.
 */

namespace cdf::io::debug
{

enum class corruption_kind
{
    undersized_record, // record_size smaller than a bare record header (can't be real)
    invalid_record_size, // record_size would run past the end of the record region
    unknown_record_type, // record_type isn't a value cdf_record_type defines at all
};

struct corruption_report
{
    std::size_t offset;
    corruption_kind kind;
    std::string detail;
};

struct recovery_action
{
    enum class kind_t
    {
        abort, // stop the walk right here
        skip_bytes // advance skip_count bytes from the corrupted record's offset, then resume
    };
    kind_t kind = kind_t::abort;
    std::size_t skip_count = 0;
};

struct default_corruption_handler
{
    recovery_action operator()(const corruption_report& report) const
    {
        std::cerr << "cdf::io::debug: corrupted record at offset " << report.offset << ": "
                  << report.detail << '\n';
        return { recovery_action::kind_t::abort };
    }
};

// CDFpp doesn't model SPR (rarely-used sparseness parameters) as a full record - this
// carries just enough to report and skip over it. UIR (freed/unused space) has its own
// real struct (cdf_UIR_t, desc-records.hpp) and is decoded properly below.
struct undecoded_record_t
{
    cdf_record_type type;
    std::size_t record_size;
};

template <typename version_t>
using record_variant = std::variant<cdf_CDR_t<version_t>, cdf_GDR_t<version_t>,
    cdf_ADR_t<version_t>, cdf_AgrEDR_t<version_t>, cdf_AzEDR_t<version_t>, cdf_rVDR_t<version_t>,
    cdf_zVDR_t<version_t>, cdf_VXR_t<version_t>, cdf_VVR_t<version_t>, cdf_CVVR_t<version_t>,
    cdf_CCR_t<version_t>, cdf_CPR_t<version_t>, cdf_UIR_t<version_t>, undecoded_record_t>;

namespace details
{
    template <typename version_t>
    struct record_header_peek_t
    {
        using endianness = cdf_record_endianness;
        cdf_offset_field_t<version_t> record_size;
        cdf_record_type record_type;
    };

    constexpr bool is_known_record_type(cdf_record_type t)
    {
        using enum cdf_record_type;
        switch (t)
        {
            case CDR:
            case GDR:
            case rVDR:
            case ADR:
            case AgrEDR:
            case VXR:
            case VVR:
            case zVDR:
            case AzEDR:
            case CCR:
            case CPR:
            case SPR:
            case CVVR:
            case UIR:
                return true;
            default:
                return false;
        }
    }

    template <typename buffer_t>
    common::magic_numbers_t peek_magic(buffer_t& buffer)
    {
        common::magic_numbers_t magic {};
        load_record(magic, buffer, 0);
        return magic;
    }
}

// Core walker: `buffer` must already hold the record region proper (right after the
// 8-byte magic, uncompressed - for a compressed file that means the decompressed
// bytes, not the original file). Walks [start_offset, end_offset), reporting every
// record via on_record and every structural anomaly via on_corruption.
template <typename version_t, typename buffer_t, typename on_record_t,
    typename on_corruption_t = default_corruption_handler>
void for_each_record(buffer_t& buffer, version_t, std::size_t start_offset, std::size_t end_offset,
    on_record_t&& on_record, on_corruption_t&& on_corruption = {})
{
    using enum cdf_record_type;
    constexpr std::size_t header_size = sizeof(cdf_offset_field_t<version_t>) + sizeof(int32_t);

    std::optional<cdf_GDR_t<version_t>> gdr_ctx;
    std::size_t offset = start_offset;
    while (offset < end_offset)
    {
        details::record_header_peek_t<version_t> peek {};
        load_record(peek, buffer, offset);
        const std::size_t record_size = static_cast<std::size_t>(peek.record_size);

        auto report = [&](corruption_kind kind, std::string detail)
        {
            recovery_action action
                = on_corruption(corruption_report { offset, kind, std::move(detail) });
            if (action.kind == recovery_action::kind_t::abort)
                return false;
            offset += action.skip_count;
            return true;
        };

        if (record_size < header_size)
        {
            if (!report(corruption_kind::undersized_record,
                    "record_size=" + std::to_string(record_size)
                        + ", must be >= " + std::to_string(header_size)))
                return;
            continue;
        }
        if (record_size > end_offset - offset) // no addition: offset <= end_offset is a loop
                                               // invariant, so this can't overflow, unlike
                                               // `offset + record_size > end_offset` would for
                                               // a huge/corrupted record_size
        {
            if (!report(corruption_kind::invalid_record_size,
                    "record_size=" + std::to_string(record_size) + " at offset "
                        + std::to_string(offset) + " would run past end of record region ("
                        + std::to_string(end_offset) + ")"))
                return;
            continue;
        }
        if (!details::is_known_record_type(peek.record_type))
        {
            if (!report(corruption_kind::unknown_record_type,
                    "record_type=" + std::to_string(static_cast<int32_t>(peek.record_type))))
                return;
            continue;
        }

        switch (peek.record_type)
        {
            case CDR:
            {
                cdf_CDR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case GDR:
            {
                cdf_GDR_t<version_t> r {};
                load_record(r, buffer, offset);
                gdr_ctx = r;
                on_record(offset, r);
                break;
            }
            case ADR:
            {
                cdf_ADR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case AgrEDR:
            {
                cdf_AgrEDR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case AzEDR:
            {
                cdf_AzEDR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case rVDR:
            {
                cdf_rVDR_t<version_t> r {};
                vdr_context_t<version_t> ctx { gdr_ctx ? gdr_ctx->rNumDims : 0 };
                cpp_utils::serde::deserialize(r, buffer, offset, ctx);
                on_record(offset, r);
                break;
            }
            case zVDR:
            {
                cdf_zVDR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case VXR:
            {
                cdf_VXR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case VVR:
            {
                cdf_VVR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case CVVR:
            {
                cdf_CVVR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case CCR:
            {
                cdf_CCR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case CPR:
            {
                cdf_CPR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case UIR:
            {
                cdf_UIR_t<version_t> r {};
                load_record(r, buffer, offset);
                on_record(offset, r);
                break;
            }
            case SPR:
            default:
                on_record(offset, undecoded_record_t { peek.record_type, record_size });
                break;
        }
        offset += record_size;
    }
}

namespace details
{
    // Mirrors loading.hpp's parse_cdf: for compressed files, the entire GDR-onward
    // record region (including a re-synthesized CDR at its original offset) is one
    // compressed blob attached to the CCR; decompression is a one-shot whole-buffer
    // operation (cpp_utils/CDFpp have no incremental inflate today), so the *contents*
    // of that blob can only be walked after decompressing, not as a true single-pass
    // stream. The CCR and CPR themselves are real, physical records at their own real
    // file offsets though (not merely internal decompression plumbing), so they are
    // reported via on_record like everything else, before the decompressed walk
    // begins. The 8 magic bytes are copied ahead of the decompressed payload so
    // absolute offsets recorded elsewhere in the file (GDRoffset, ADRhead, ...) stay
    // valid against this reconstructed buffer.
    template <typename version_t, typename buffer_t, typename on_record_t, typename on_corruption_t>
    void run_compressed(buffer_t& buffer, std::size_t start_offset, on_record_t&& on_record,
        on_corruption_t&& on_corruption)
    {
        cdf_CCR_t<version_t> ccr {};
        load_record(ccr, buffer, 8);
        cdf_CPR_t<version_t> cpr {};
        load_record(cpr, buffer, ccr.CPRoffset);
        on_record(8UL, ccr);
        on_record(static_cast<std::size_t>(ccr.CPRoffset), cpr);
        no_init_vector<char> data(8UL + ccr.uSize);
        buffer.read(data.data(), 0, 8);
        decompression::inflate(cpr.cType, ccr.data, data.data() + 8UL, std::size(data) - 8UL);
        auto decompressed = buffers::make_shared_array_adapter(std::move(data));
        for_each_record(decompressed, version_t {}, start_offset, decompressed.size(),
            std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
    }

    template <typename version_t, typename buffer_t, typename on_record_t, typename on_corruption_t>
    void run(buffer_t& buffer, bool is_compressed, std::size_t start_offset,
        on_record_t&& on_record, on_corruption_t&& on_corruption)
    {
        if (is_compressed)
            run_compressed<version_t>(buffer, start_offset, std::forward<on_record_t>(on_record),
                std::forward<on_corruption_t>(on_corruption));
        else
            for_each_record(buffer, version_t {}, start_offset, buffer.size(),
                std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
    }
}

// Same auto-detecting entry point as the 2-arg overload below, but starts the walk at an
// arbitrary byte offset instead of right after the 8-byte magic. For a compressed file,
// start_offset is interpreted in the *decompressed* reconstructed buffer's own offset space
// (the same space GDRoffset/ADRhead/... already point into - see run_compressed's own
// comment) - the CCR/CPR themselves are still always reported first regardless of
// start_offset, since they're needed to even decompress the rest.
template <typename buffer_t, typename on_record_t,
    typename on_corruption_t = default_corruption_handler>
void for_each_record(buffer_t&& buffer, std::size_t start_offset, on_record_t&& on_record,
    on_corruption_t&& on_corruption = {})
{
    auto magic = details::peek_magic(buffer);
    if (!common::is_cdf(magic))
        throw std::runtime_error("cdf::io::debug::for_each_record: not a CDF file");

    const bool compressed = common::is_compressed(magic);
    if (common::is_v3x(magic))
    {
        details::run<v3x_tag>(buffer, compressed, start_offset,
            std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
    }
    else if (compressed)
    {
        // Mirrors loading.hpp: a compressed v2.x file uses the coarse v2x_tag
        // throughout (the finer v2.4-or-less/v2.5-or-more split below only applies to
        // the uncompressed path there too).
        details::run<v2x_tag>(buffer, true, start_offset, std::forward<on_record_t>(on_record),
            std::forward<on_corruption_t>(on_corruption));
    }
    else
    {
        cdf_CDR_t<v2x_tag> cdr_probe {};
        load_record(cdr_probe, buffer, 8);
        if (cdr_probe.Release >= 5)
            details::run<v2_5_or_more_tag>(buffer, false, start_offset,
                std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
        else
            details::run<v2_4_or_less_tag>(buffer, false, start_offset,
                std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
    }
}

// Top-level entry: walks a whole CDF file/buffer (anything cdf::io::load() itself
// accepts) - detects magic/version/compression and, for compressed files, decompresses
// once, then walks the resulting record stream.
template <typename buffer_t, typename on_record_t,
    typename on_corruption_t = default_corruption_handler>
void for_each_record(
    buffer_t&& buffer, on_record_t&& on_record, on_corruption_t&& on_corruption = {})
{
    for_each_record(buffer, 8UL, std::forward<on_record_t>(on_record),
        std::forward<on_corruption_t>(on_corruption));
}

template <typename on_record_t, typename on_corruption_t = default_corruption_handler>
void for_each_record(
    const std::string& path, on_record_t&& on_record, on_corruption_t&& on_corruption = {})
{
    auto buffer = buffers::make_shared_file_adapter(path);
    for_each_record(
        buffer, std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
}

template <typename on_record_t, typename on_corruption_t = default_corruption_handler>
void for_each_record(
    const std::vector<char>& data, on_record_t&& on_record, on_corruption_t&& on_corruption = {})
{
    auto buffer = buffers::make_shared_array_adapter(data);
    for_each_record(
        buffer, std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
}

template <typename on_record_t, typename on_corruption_t = default_corruption_handler>
void for_each_record(
    std::vector<char>&& data, on_record_t&& on_record, on_corruption_t&& on_corruption = {})
{
    auto buffer = buffers::make_shared_array_adapter(std::move(data));
    for_each_record(
        buffer, std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
}

} // namespace cdf::io::debug
