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
#include "cdf-io-variable.hpp"
#include "cdf-io-zlib.hpp"
#include <algorithm>
#include <filesystem>
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
        buffer_t& buffer;
        bool is_compressed;
        bool ok = false;
        cdf_headers_t(buffer_t& buffer, std::size_t CDRoffset = 8)
                : cdr { buffer }, gdr { buffer }, buffer { buffer }
        {
            magic = get_magic(buffer);
            if (common::is_cdf(magic) && cdr.load(CDRoffset) && gdr.load(cdr.GDRoffset.value))
                ok = true;
        }
    };

    CDF from_repr(common::cdf_repr&& repr)
    {
        CDF cdf;
        cdf.attributes = std::move(repr.attributes);
        cdf.variables = std::move(repr.variables);
        return cdf;
    }

    template <typename cdf_headers_t>
    std::optional<CDF> impl_parse_cdf(cdf_headers_t& cdf_headers)
    {
        common::cdf_repr repr;
        if (!cdf_headers.ok)
            return std::nullopt;
        if (!attribute::load_all<typename cdf_headers_t::version_tag>(cdf_headers, repr))
            return std::nullopt;
        if (!variable::load_all<typename cdf_headers_t::version_tag>(cdf_headers, repr))
            return std::nullopt;
        return from_repr(std::move(repr));
    }

    template <typename cdf_version_tag_t, typename buffer_t>
    std::optional<CDF> parse_cdf(buffer_t& buffer, bool is_compressed = false)
    {
        if (is_compressed)
        {
            if (cdf_CCR_t<cdf_version_tag_t, buffer_t> CCR { buffer }; CCR.load(8UL))
            {
                if (cdf_CPR_t<cdf_version_tag_t, buffer_t> CPR { buffer };
                    CPR.load(CCR.CPRoffset.value)
                    && CPR.cType.value == cdf_compression_type::gzip_compression)
                {
                    auto data = buffer.read(0, 8);
                    zlib::gzinflate(CCR.data.value, data);
                    buffers::array_adapter decompressed_buffer(data);
                    cdf_headers_t<cdf_version_tag_t, decltype(decompressed_buffer)> cdf_headers {
                        decompressed_buffer
                    };
                    return impl_parse_cdf(cdf_headers);
                }
            }
            return std::nullopt;
        }
        else
        {
            cdf_headers_t<cdf_version_tag_t, buffer_t> cdf_headers { buffer };
            return impl_parse_cdf(cdf_headers);
        }
    }

    template <typename buffer_t>
    auto impl_load(buffer_t&& buffer) -> decltype(buffer.read(0UL, 0UL), std::optional<CDF> {})
    {
        auto magic = get_magic(buffer);
        if (common::is_v3x(magic))
        {
            return parse_cdf<v3x_tag>(buffer, common::is_compressed(magic));
        }
        else
        {
            return parse_cdf<v2x_tag>(buffer, common::is_compressed(magic));
        }
    }
} // namespace


std::optional<CDF> load(const std::string& path)
{
    if (std::filesystem::exists(path))
    {
        buffers::stream_adapter buffer { std::fstream { path, std::ios::in | std::ios::binary } };
        return impl_load(buffer);
    }
    return std::nullopt;
}

std::optional<CDF> load(const std::vector<char>& data)
{
    if (std::size(data))
    {
        buffers::array_adapter buffer { data };
        return impl_load(buffer);
    }
    return std::nullopt;
}
std::optional<CDF> load(char* data, std::size_t size)
{
    if (size != 0 && data != nullptr)
    {
        buffers::array_adapter buffer { data, size };
        return impl_load(buffer);
    }
    return std::nullopt;
}
} // namespace cdf::io
