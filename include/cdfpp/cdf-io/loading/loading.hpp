#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#include "../common.hpp"
#include "../decompression.hpp"
#include "../desc-records.hpp"
#include "../endianness.hpp"
#include "./attribute.hpp"
#include "./buffers.hpp"
#include "./records-loading.hpp"
#include "./variable.hpp"
#include "cdfpp/cdf-enums.hpp"
#include "cdfpp/cdf-file.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <utility>

namespace cdf::io
{

namespace
{
    template <typename buffer_t>
    common::magic_numbers_t get_magic(buffer_t& buffer)
    {
        uint32_t magic[2];
        buffer.read(reinterpret_cast<char*>(magic), 0, sizeof(magic));
        endianness::decode_v<endianness::big_endian_t>(magic, 2);
        return { magic[0], magic[1] };
    }


    template <typename version_t, typename buffer_t>
    auto make_parsing_context(version_t, buffer_t&& buff, cdf_compression_type compression_type)
    {
        parsing_context_t<buffer_t, version_t> ctx { std::move(buff), compression_type };
        load_record(ctx.cdr, ctx.buffer, 8);
        load_record(ctx.gdr, ctx.buffer, ctx.cdr.GDRoffset);
        ctx.majority = common::majority(ctx.cdr);
        return ctx;
    }

    CDF from_repr(common::cdf_repr&& repr)
    {
        CDF cdf;
        cdf.majority = repr.majority;
        cdf.distribution_version = repr.distribution_version;
        cdf.attributes = std::move(repr.attributes);
        cdf.variables = std::move(repr.variables);
        cdf.lazy_loaded = repr.lazy;
        cdf.compression = repr.compression_type;
        // cdf.leap_second_last_updated = repr.leap_second_last_updated;
        return cdf;
    }

    template <bool iso_8859_1_to_utf8, typename parsing_context_t>
    [[nodiscard]] std::optional<CDF> impl_parse_cdf(
        parsing_context_t& parsing_context, bool lazy_load = false)
    {
        common::cdf_repr repr { parsing_context.gdr.NzVars + parsing_context.gdr.NrVars };
        repr.majority = parsing_context.majority;
        repr.distribution_version = parsing_context.distribution_version();
        repr.compression_type = parsing_context.compression_type;
        repr.lazy = lazy_load;
        if (!attribute::load_all<typename parsing_context_t::version_tag, iso_8859_1_to_utf8>(
                parsing_context, repr))
            return std::nullopt;
        if (!variable::load_all<typename parsing_context_t::version_tag, iso_8859_1_to_utf8>(
                parsing_context, repr, lazy_load))
            return std::nullopt;
        return from_repr(std::move(repr));
    }

    template <typename cdf_version_tag_t, typename iso_8859_1_to_utf8, typename buffer_t>
    [[nodiscard]] std::optional<CDF> parse_cdf(
        buffer_t&& buffer, iso_8859_1_to_utf8, bool is_compressed = false, bool lazy_load = false)
    {
        if (is_compressed)
        {
            if (cdf_CCR_t<cdf_version_tag_t> CCR {}; load_record(CCR, buffer, 8))
            {
                cdf_CPR_t<cdf_version_tag_t> CPR;
                load_record(CPR, buffer, CCR.CPRoffset);
                no_init_vector<char> data(8UL + CCR.uSize);
                buffer.read(data.data(), 0, 8);
                decompression::inflate(
                    CPR.cType, CCR.data.values, data.data() + 8UL, std::size(data) - 8UL);
                auto parsing_ctx = make_parsing_context(cdf_version_tag_t {},
                    buffers::make_shared_array_adapter(std::move(data)), CPR.cType);
                return impl_parse_cdf<common::with_iso_8859_1_to_utf8<iso_8859_1_to_utf8>>(
                    parsing_ctx, lazy_load);
            }
            return std::nullopt;
        }
        else // Compression was introduced in CDF V2.6
        {
            auto parsing_ctx = make_parsing_context(
                cdf_version_tag_t {}, std::move(buffer), cdf_compression_type::no_compression);
            if constexpr (!is_v3_v<cdf_version_tag_t>)
            {
                if (parsing_ctx.cdr.Release >= 5)
                {
                    auto new_ctx = make_parsing_context(v2_5_or_more_tag {},
                        std::move(parsing_ctx.buffer), cdf_compression_type::no_compression);
                    return impl_parse_cdf<common::with_iso_8859_1_to_utf8<iso_8859_1_to_utf8>>(
                        new_ctx, lazy_load);
                }
                else
                {
                    auto new_ctx = make_parsing_context(v2_4_or_less_tag {},
                        std::move(parsing_ctx.buffer), cdf_compression_type::no_compression);
                    return impl_parse_cdf<common::with_iso_8859_1_to_utf8<iso_8859_1_to_utf8>>(
                        new_ctx, lazy_load);
                }
            }
            else
            {
                return impl_parse_cdf<common::with_iso_8859_1_to_utf8<iso_8859_1_to_utf8>>(
                    parsing_ctx, lazy_load);
            }
        }
    }

    template <typename buffer_t, typename iso_8859_1_to_utf8>
    [[nodiscard]] auto _impl_load(buffer_t&& buffer, iso_8859_1_to_utf8 iso_8859_1_to_utf8_tag,
        bool lazy_load = false) -> decltype(buffer.read(std::declval<char*>(), 0UL, 0UL),
                                    std::optional<CDF> {})
    {
        auto magic = get_magic(buffer);
        if (common::is_cdf(magic))
        {
            if (common::is_v3x(magic))
            {
                return parse_cdf<v3x_tag>(std::move(buffer), iso_8859_1_to_utf8_tag,
                    common::is_compressed(magic), lazy_load);
            }
            else
            {
                return parse_cdf<v2x_tag>(std::move(buffer), iso_8859_1_to_utf8_tag,
                    common::is_compressed(magic), lazy_load);
            }
        }
        return std::nullopt;
    }

    template <typename buffer_t>
    [[nodiscard]] auto impl_load(buffer_t&& buffer, bool iso_8859_1_to_utf8, bool lazy_load = false)
    {
        if (iso_8859_1_to_utf8)
            return _impl_load(std::move(buffer), common::iso_8859_1_to_utf8_t {}, lazy_load);
        else
            return _impl_load(std::move(buffer), common::no_iso_8859_1_to_utf8_t {}, lazy_load);
    }
} // namespace


[[nodiscard]] std::optional<CDF> load(
    const std::string& path, bool iso_8859_1_to_utf8 = false, bool lazy_load = true)
{
    auto buffer = buffers::make_shared_file_adapter(path);
    if (buffer.is_valid())
    {
        return impl_load(std::move(buffer), iso_8859_1_to_utf8, lazy_load);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<CDF> load(
    const std::vector<char>& data, bool iso_8859_1_to_utf8 = false, bool lazy_load = false)
{
    if (std::size(data))
    {
        return impl_load(buffers::make_shared_array_adapter(data), iso_8859_1_to_utf8, lazy_load);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<CDF> load(
    const std::vector<char>&& data, bool iso_8859_1_to_utf8 = false, bool lazy_load = true)
{
    if (std::size(data))
    {
        return impl_load(
            buffers::make_shared_array_adapter(std::move(data)), iso_8859_1_to_utf8, lazy_load);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<CDF> load(
    const char* data, std::size_t size, bool iso_8859_1_to_utf8 = false, bool lazy_load = false)
{
    if (size != 0 && data != nullptr)
    {
        return impl_load(
            buffers::make_shared_array_adapter(data, size), iso_8859_1_to_utf8, lazy_load);
    }
    return std::nullopt;
}
}
