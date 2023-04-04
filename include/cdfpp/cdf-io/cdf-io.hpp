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
#include "cdf-io-desc-records.hpp"
#include "cdf-io-rle.hpp"
#include "cdf-io-variable.hpp"
#include "cdf-io-zlib.hpp"
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
        auto data = buffer.template read<8>(0);
        uint32_t magic1 = cdf::endianness::decode<endianness::big_endian_t, uint32_t>(data.data());
        uint32_t magic2
            = cdf::endianness::decode<endianness::big_endian_t, uint32_t>(data.data() + 4);
        return { magic1, magic2 };
    }

    template <typename version_t, typename buffer_t>
    struct cdf_headers_t
    {
        inline static constexpr bool v3 = is_v3_v<version_t>;
        using version_tag = version_t;
        common::magic_numbers_t magic;
        cdf_CDR_t<version_t, buffer_t> cdr;
        cdf_GDR_t<version_t, buffer_t> gdr;
        cdf_majority majority;
        std::tuple<uint32_t, uint32_t, uint32_t> distribution_version;

        buffer_t& buffer;
        bool is_compressed;
        bool ok = false;
        cdf_headers_t(buffer_t& buffer, std::size_t CDRoffset = 8)
                : cdr { buffer }, gdr { buffer }, buffer { buffer }
        {
            magic = get_magic(buffer);
            if (common::is_cdf(magic) && cdr.load(CDRoffset) && gdr.load(cdr.GDRoffset.value))
            {
                ok = true;
                majority = common::majority(cdr);
                distribution_version = { cdr.Version, cdr.Release, cdr.Increment };
            }
        }
        inline cdf_encoding encoding() { return cdr.Encoding.value; }
    };

    CDF from_repr(common::cdf_repr&& repr)
    {
        CDF cdf;
        cdf.majority = repr.majority;
        cdf.distribution_version = repr.distribution_version;
        cdf.attributes = std::move(repr.attributes);
        cdf.variables = std::move(repr.variables);
        return cdf;
    }

    template <bool iso_8859_1_to_utf8, typename cdf_headers_t>
    std::optional<CDF> impl_parse_cdf(cdf_headers_t& cdf_headers)
    {
        common::cdf_repr repr;
        repr.majority = cdf_headers.majority;
        repr.distribution_version = { cdf_headers.distribution_version };
        if (!cdf_headers.ok)
            return std::nullopt;
        if (!attribute::load_all<typename cdf_headers_t::version_tag, iso_8859_1_to_utf8>(
                cdf_headers, repr))
            return std::nullopt;
        if (!variable::load_all<typename cdf_headers_t::version_tag>(cdf_headers, repr))
            return std::nullopt;
        return from_repr(std::move(repr));
    }

    template <typename cdf_version_tag_t, bool iso_8859_1_to_utf8, typename buffer_t>
    std::optional<CDF> parse_cdf(buffer_t&& buffer, bool is_compressed = false)
    {
        if (is_compressed)
        {
            if (cdf_CCR_t<cdf_version_tag_t, buffer_t> CCR { buffer }; CCR.load(8UL))
            {
                cdf_CPR_t<cdf_version_tag_t, buffer_t> CPR { buffer };
                CPR.load(CCR.CPRoffset.value);
                if (CPR.cType.value == cdf_compression_type::gzip_compression)
                {
                    std::vector<char> data(8UL);
                    buffer.read(data.data(), 0, 8);
                    zlib::gzinflate(CCR.data.value, data);
                    buffers::array_adapter decompressed_buffer(data);
                    cdf_headers_t<cdf_version_tag_t, decltype(decompressed_buffer)> cdf_headers {
                        decompressed_buffer
                    };
                    return impl_parse_cdf<iso_8859_1_to_utf8>(cdf_headers);
                }
                else if (CPR.cType.value == cdf_compression_type::rle_compression)
                {
                    std::vector<char> data(8UL);
                    buffer.read(data.data(), 0, 8);
                    rle::deflate(CCR.data.value, data);
                    buffers::array_adapter decompressed_buffer(data);
                    cdf_headers_t<cdf_version_tag_t, decltype(decompressed_buffer)> cdf_headers {
                        decompressed_buffer
                    };
                    return impl_parse_cdf<iso_8859_1_to_utf8>(cdf_headers);
                }
            }
            return std::nullopt;
        }
        else
        {
            cdf_headers_t<cdf_version_tag_t, buffer_t> cdf_headers { buffer };
            return impl_parse_cdf<iso_8859_1_to_utf8>(cdf_headers);
        }
    }

    template <typename buffer_t, bool iso_8859_1_to_utf8>
    auto _impl_load(buffer_t&& buffer) -> decltype(buffer.read(0UL, 0UL), std::optional<CDF> {})
    {
        auto magic = get_magic(buffer);
        if (common::is_v3x(magic))
        {
            return parse_cdf<v3x_tag, iso_8859_1_to_utf8>(
                std::forward<buffer_t>(buffer), common::is_compressed(magic));
        }
        else
        {
            return parse_cdf<v2x_tag, iso_8859_1_to_utf8>(
                std::forward<buffer_t>(buffer), common::is_compressed(magic));
        }
    }

    template <typename buffer_t>
    auto impl_load(buffer_t&& buffer, bool iso_8859_1_to_utf8)
    {
        if (iso_8859_1_to_utf8)
            return _impl_load<buffer_t, true>(std::forward<buffer_t>(buffer));
        else
            return _impl_load<buffer_t, false>(std::forward<buffer_t>(buffer));
    }
} // namespace


std::optional<CDF> load(const std::string& path, bool iso_8859_1_to_utf8 = false)
{
    auto buffer = buffers::make_file_adapter(path);
    if (buffer.is_valid())
    {
        return impl_load(std::move(buffer), iso_8859_1_to_utf8);
    }
    return std::nullopt;
}

std::optional<CDF> load(const std::vector<char>& data, bool iso_8859_1_to_utf8 = false)
{
    if (std::size(data))
    {
        return impl_load(buffers::array_adapter { data }, iso_8859_1_to_utf8);
    }
    return std::nullopt;
}
std::optional<CDF> load(const char* data, std::size_t size, bool iso_8859_1_to_utf8 = false)
{
    if (size != 0 && data != nullptr)
    {
        return impl_load(buffers::array_adapter { data, size }, iso_8859_1_to_utf8);
    }
    return std::nullopt;
}
} // namespace cdf::io
