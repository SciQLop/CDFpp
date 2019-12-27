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
#include "cdf-desc-records.hpp"
#include "cdf-endianness.hpp"
#include "cdf-enums.hpp"
#include "cdf-file.hpp"
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

    template <typename version_t>
    struct cdf_parsing_t
    {
        inline static constexpr bool v3 = is_v3_v<version_t>;
        magic_numbers_t magic;
        bool is_compressed;
        cdf_CDR_t<version_t> cdr;
        cdf_GDR_t<version_t> gdr;
    };

    bool is_cdf(const magic_numbers_t& magic_numbers) noexcept
    {
        return (magic_numbers.first & 0xfff00000) == 0xCDF00000
            && (magic_numbers.second == 0xCCCC0001 || magic_numbers.second == 0x0000FFFF);
    }

    template <typename context_t>
    bool is_cdf(const context_t& context) noexcept
    {
        return is_cdf(context.magic);
    }

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

    template <typename streamT>
    std::size_t get_file_len(streamT& stream)
    {
        auto pos = stream.tellg();
        stream.seekg(std::ios::end);
        auto length = stream.tellg();
        stream.seekg(pos);
        return length;
    }

    bool is_compressed(const magic_numbers_t& magic_numbers) noexcept
    {
        return magic_numbers.second == 0xCCCC0001;
    }

    bool is_v3x(const magic_numbers_t& magic) { return ((magic.first >> 12) & 0xff) >= 0x30; }

    template <typename cdf_version_tag_t, typename buffer_t>
    cdf_record_type record_type(buffer_t&& buffer)
    {
        if constexpr (std::is_same_v<cdf_version_tag_t, v3x_tag>)
            return endianness::decode<cdf_record_type>(buffer + 8);
        else
            return endianness::decode<cdf_record_type>(buffer + 4);
    }

    template <typename cdf_version_tag_t, typename streamT, typename context_t>
    bool parse_CDR(streamT& stream, context_t& context)
    {
        if (!context.cdr.load(stream))
            return false;
        return true;
    }

    template <typename cdf_version_tag_t, typename streamT, typename context_t>
    bool parse_GDR(streamT& stream, context_t& context)
    {
        if (!context.gdr.load(stream, context.cdr.GDRoffset.value))
            return false;
        return true;
    }

    template <typename cdf_version_tag_t, typename streamT>
    Attribute::attr_data_t load_attribute_data(
        std::size_t offset, streamT& stream, std::size_t AEDRCount)
    {
        cdf_AEDR_t<cdf_version_tag_t> AEDR;
        if (AEDR.load(stream, offset))
        {
            Attribute::attr_data_t values;
            while (AEDRCount--)
            {
                std::size_t element_size = cdf_type_size(CDF_Types { AEDR.DataType.value });
                auto buffer = read_buffer<std::vector<char>>(
                    stream, offset + AEDR.Values.offset, AEDR.NumElements * element_size);
                values.emplace_back(load_values(
                    buffer.data(), std::size(buffer), AEDR.DataType.value, cdf_encoding::IBMPC));
                offset = AEDR.AEDRnext.value;
                AEDR.load(stream, offset);
            }
            return values;
        }
        return {};
    }

    template <typename cdf_version_tag_t, typename streamT>
    std::size_t load_attribute(std::size_t offset, streamT& stream, CDF& cdf)
    {
        cdf_ADR_t<cdf_version_tag_t> ADR;
        if (ADR.load(stream, offset))
        {
            Attribute::attr_data_t data;
            if (ADR.AzEDRhead != 0)
                data = load_attribute_data<cdf_version_tag_t>(ADR.AzEDRhead, stream, ADR.NzEntries);
            else if (ADR.AgrEDRhead != 0)
                data = load_attribute_data<cdf_version_tag_t>(
                    ADR.AgrEDRhead, stream, ADR.NgrEntries);
            if(ADR.scope==cdf_attr_scope::global || ADR.scope == cdf_attr_scope::global_assumed)
                add_attribute(cdf, ADR.Name.value, std::move(data));
            else if(ADR.scope==cdf_attr_scope::variable || ADR.scope == cdf_attr_scope::variable_assumed)
                add_var_attribute(cdf, ADR.num.value, ADR.Name.value, std::move(data));
            return ADR.ADRnext;
        }
        return 0;
    }

    template <typename cdf_version_tag_t, typename streamT, typename context_t>
    bool load_attributes(streamT& stream, context_t& context, CDF& cdf)
    {
        auto attr_count = context.gdr.NumAttr.value;
        auto next_attr = context.gdr.ADRhead.value;
        while ((attr_count > 0) and (next_attr != 0ul))
        {
            next_attr = load_attribute<cdf_version_tag_t>(next_attr, stream, cdf);
            --attr_count;
        }
        return true;
    }

    template <typename cdf_version_tag_t, typename streamT, typename context_t>
    bool load_zVars(streamT& stream, context_t& context, CDF& cdf)
    {
        return true;
    }

    template <typename cdf_version_tag_t, typename streamT, typename context_t>
    bool load_rVars(streamT& stream, context_t& context, CDF& cdf)
    {
        return true;
    }

    template <typename cdf_version_tag_t, typename stream_t>
    std::optional<CDF> parse_cdf(stream_t& cdf_file)
    {
        cdf_parsing_t<cdf_version_tag_t> context;
        context.magic = get_magic(cdf_file);
        if (!is_cdf(context))
            return std::nullopt;
        if (!parse_CDR<cdf_version_tag_t>(cdf_file, context))
            return std::nullopt;
        if (!parse_GDR<cdf_version_tag_t>(cdf_file, context))
            return std::nullopt;
        CDF cdf;
        if (!load_attributes<cdf_version_tag_t>(cdf_file, context, cdf))
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
        if (is_v3x(magic))
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
