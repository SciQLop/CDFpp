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
#include "../cdf-endianness.hpp"
#include "../cdf-enums.hpp"
#include "../cdf-file.hpp"
#include "cdf-io-attribute.hpp"
#include "cdf-io-buffers.hpp"
#include "cdf-io-common.hpp"
#include "cdf-io-decompression.hpp"
#include "cdf-io-desc-records.hpp"
#include "cdf-io-variable.hpp"
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
        buffer.read(reinterpret_cast<char*>(magic),0,sizeof(magic));
        endianness::decode_v<endianness::big_endian_t>(magic,2);
        return {magic[0],magic[1]};
    }

    template <typename buffer_t, typename version_t>
    struct cdf_headers_t
    {
        inline static constexpr bool v3 = is_v3_v<version_t>;
        using version_tag = version_t;
        common::magic_numbers_t magic;
        buffer_t buffer;
        cdf_CDR_t<version_t, buffer_t> cdr;
        cdf_GDR_t<version_t, buffer_t> gdr;
        cdf_majority majority;
        std::tuple<uint32_t, uint32_t, uint32_t> distribution_version;
        cdf_compression_type compression_type;
        bool is_compressed;
        bool ok = false;
        cdf_headers_t(buffer_t&& buff, version_t, cdf_compression_type compression_type,
            std::size_t CDRoffset = 8)
                : buffer { std::move(buff) }
                , cdr { buffer }
                , gdr { buffer }
                , compression_type { compression_type }
        {
            magic = get_magic(this->buffer);
            if (common::is_cdf(magic) && cdr.load(CDRoffset) && gdr.load(cdr.GDRoffset.value))
            {
                ok = true;
                majority = common::majority(cdr);
                distribution_version = { cdr.Version, cdr.Release, cdr.Increment };
            }
        }
        inline cdf_encoding encoding() { return cdr.Encoding.value; }
    };

    // template <typename version_t, typename buffer_t>
    // cdf_headers_t(buffer_t&&, version_t) -> cdf_headers_t<buffer_t, version_t>;

    CDF from_repr(common::cdf_repr&& repr)
    {
        CDF cdf;
        cdf.majority = repr.majority;
        cdf.distribution_version = repr.distribution_version;
        cdf.attributes = std::move(repr.attributes);
        cdf.variables = std::move(repr.variables);
        cdf.lazy_loaded = repr.lazy;
        cdf.compression = repr.compression_type;
        return cdf;
    }

    template <bool iso_8859_1_to_utf8, typename cdf_headers_t>
    std::optional<CDF> impl_parse_cdf(cdf_headers_t& cdf_headers, bool lazy_load = false)
    {
        if (!cdf_headers.ok)
            return std::nullopt;
        common::cdf_repr repr { cdf_headers.gdr.NzVars.value + cdf_headers.gdr.NrVars.value };
        repr.majority = cdf_headers.majority;
        repr.distribution_version = { cdf_headers.distribution_version };
        repr.compression_type = cdf_headers.compression_type;
        if (!attribute::load_all<typename cdf_headers_t::version_tag, iso_8859_1_to_utf8>(
                cdf_headers, repr))
            return std::nullopt;
        if (!variable::load_all<typename cdf_headers_t::version_tag>(cdf_headers, repr, lazy_load))
            return std::nullopt;
        return from_repr(std::move(repr));
    }

    template <typename cdf_version_tag_t, typename iso_8859_1_to_utf8, typename buffer_t>
    std::optional<CDF> parse_cdf(
        buffer_t&& buffer, iso_8859_1_to_utf8, bool is_compressed = false, bool lazy_load = false)
    {
        if (is_compressed)
        {
            if (cdf_CCR_t<cdf_version_tag_t, buffer_t> CCR { buffer }; CCR.load(8UL))
            {
                cdf_CPR_t<cdf_version_tag_t, buffer_t> CPR { buffer };
                CPR.load(CCR.CPRoffset.value);
                no_init_vector<char> data(8UL + CCR.uSize);
                buffer.read(data.data(), 0, 8);
                if (CPR.cType.value == cdf_compression_type::gzip_compression)
                {
                    decompression::gzinflate(
                        CCR.data.value, data.data() + 8UL, std::size(data) - 8UL);
                    cdf_headers_t cdf_headers { buffers::make_shared_array_adapter(std::move(data)),
                        cdf_version_tag_t {}, cdf_compression_type::gzip_compression };
                    return impl_parse_cdf<common::with_iso_8859_1_to_utf8<iso_8859_1_to_utf8>>(
                        cdf_headers, lazy_load);
                }
                else if (CPR.cType.value == cdf_compression_type::rle_compression)
                {

                    decompression::rleinflate(
                        CCR.data.value, data.data() + 8UL, std::size(data) - 8UL);
                    cdf_headers_t cdf_headers { buffers::make_shared_array_adapter(std::move(data)),
                        cdf_version_tag_t {}, cdf_compression_type::rle_compression };
                    return impl_parse_cdf<common::with_iso_8859_1_to_utf8<iso_8859_1_to_utf8>>(
                        cdf_headers, lazy_load);
                }
            }
            return std::nullopt;
        }
        else
        {
            cdf_headers_t cdf_headers { std::move(buffer), cdf_version_tag_t {},
                cdf_compression_type::no_compression };
            return impl_parse_cdf<common::with_iso_8859_1_to_utf8<iso_8859_1_to_utf8>>(
                cdf_headers, lazy_load);
        }
    }

    template <typename buffer_t, typename iso_8859_1_to_utf8>
    auto _impl_load(
        buffer_t&& buffer, iso_8859_1_to_utf8 iso_8859_1_to_utf8_tag, bool lazy_load = false)
        -> decltype(buffer.read(std::declval<char*>(), 0UL, 0UL), std::optional<CDF> {})
    {
        auto magic = get_magic(buffer);
        if (common::is_v3x(magic))
        {
            return parse_cdf<v3x_tag>(
                std::move(buffer), iso_8859_1_to_utf8_tag, common::is_compressed(magic), lazy_load);
        }
        else
        {
            return parse_cdf<v2x_tag>(
                std::move(buffer), iso_8859_1_to_utf8_tag, common::is_compressed(magic), lazy_load);
        }
    }

    template <typename buffer_t>
    auto impl_load(buffer_t&& buffer, bool iso_8859_1_to_utf8, bool lazy_load = false)
    {
        if (iso_8859_1_to_utf8)
            return _impl_load(std::move(buffer), common::iso_8859_1_to_utf8_t {}, lazy_load);
        else
            return _impl_load(std::move(buffer), common::no_iso_8859_1_to_utf8_t {}, lazy_load);
    }
} // namespace


std::optional<CDF> load(
    const std::string& path, bool iso_8859_1_to_utf8 = false, bool lazy_load = true)
{
    auto buffer = buffers::make_shared_file_adapter(path);
    if (buffer.is_valid())
    {
        return impl_load(std::move(buffer), iso_8859_1_to_utf8, lazy_load);
    }
    return std::nullopt;
}

std::optional<CDF> load(
    const std::vector<char>& data, bool iso_8859_1_to_utf8 = false, bool lazy_load = false)
{
    if (std::size(data))
    {
        return impl_load(buffers::make_shared_array_adapter(data), iso_8859_1_to_utf8, lazy_load);
    }
    return std::nullopt;
}

std::optional<CDF> load(
    const std::vector<char>&& data, bool iso_8859_1_to_utf8 = false, bool lazy_load = true)
{
    if (std::size(data))
    {
        return impl_load(
            buffers::make_shared_array_adapter(std::move(data)), iso_8859_1_to_utf8, lazy_load);
    }
    return std::nullopt;
}

std::optional<CDF> load(
    const char* data, std::size_t size, bool iso_8859_1_to_utf8 = false, bool lazy_load = false)
{
    if (size != 0 && data != nullptr)
    {
        return impl_load(
            buffers::make_shared_array_adapter(data, size), iso_8859_1_to_utf8, lazy_load);
    }
    return std::nullopt;
}

} // namespace cdf::io
