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
#include "cdf-io-common.hpp"
#include "cdf-io-desc-records.hpp"
#include "cdf-io-variable.hpp"
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
    template <typename streamT>
    magic_numbers_t get_magic(streamT& stream)
    {
        stream.seekg(std::ios::beg);
        char buffer[8];
        stream.read(buffer, 8);
        uint32_t magic1 = cdf::endianness::decode<uint32_t>(buffer);
        uint32_t magic2 = cdf::endianness::decode<uint32_t>(buffer + 4);
        return { magic1, magic2 };
    }

    template <typename version_t, typename stream_t>
    struct cdf_headers_t
    {
        inline static constexpr bool v3 = is_v3_v<version_t>;
        magic_numbers_t magic;
        bool is_compressed;
        bool ok = false;
        cdf_CDR_t<version_t, stream_t> cdr;
        cdf_GDR_t<version_t, stream_t> gdr;
        cdf_headers_t(stream_t& stream) : cdr { stream }, gdr { stream }
        {
            magic = get_magic(stream);
            if (common::is_cdf(magic) && cdr.load() && gdr.load(cdr.GDRoffset.value))
                ok = true;
        }
    };

    template <typename streamT>
    std::size_t get_file_len(streamT& stream)
    {
        auto pos = stream.tellg();
        stream.seekg(std::ios::end);
        auto length = stream.tellg();
        stream.seekg(pos);
        return length;
    }


    template <typename cdf_version_tag_t, typename buffer_t>
    cdf_record_type record_type(buffer_t&& buffer)
    {
        if constexpr (std::is_same_v<cdf_version_tag_t, v3x_tag>)
            return endianness::decode<cdf_record_type>(buffer + 8);
        else
            return endianness::decode<cdf_record_type>(buffer + 4);
    }

    template <typename cdf_version_tag_t, typename stream_t>
    std::optional<CDF> parse_cdf(stream_t& cdf_file)
    {
        cdf_headers_t<cdf_version_tag_t, stream_t> cdf_headers { cdf_file };
        if (!cdf_headers.ok)
            return std::nullopt;
        CDF cdf;
        if (!attribute::load_all<cdf_version_tag_t>(cdf_file, cdf_headers, cdf))
            return std::nullopt;
        if (!variable::load_all<cdf_version_tag_t>(cdf_file, cdf_headers, cdf))
            return std::nullopt;
        return cdf;
    }
} // namespace

std::optional<CDF> load(const std::string& path)
{
    if (std::filesystem::exists(path))
    {
        std::fstream cdf_file(path, std::ios::in | std::ios::binary);
        auto length = get_file_len(cdf_file);
        auto magic = get_magic(cdf_file);
        if (common::is_v3x(magic))
        {
            return parse_cdf<v3x_tag>(cdf_file);
        }
        else
        {
            return parse_cdf<v2x_tag>(cdf_file);
        }
    }
    return std::nullopt;
}
} // namespace cdf::io
