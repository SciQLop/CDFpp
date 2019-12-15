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
#include "cdf-endianness.hpp"
#include "cdf-file.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <utility>

namespace cdf::io
{
namespace
{
enum class cdf_record_type:int32_t
{
    CDR = 1,
    GDR = 2,
    rVDR = 3,
    ADR = 4,
    AgrEDR = 5,
    VXR = 6,
    VVR = 7,
    zVDR = 8,
    AzEDR = 9,
    CCR = 10,
    CPR = 11,
    SPR = 12,
    CVVR = 13,
    UIR = -1,
};

struct cdf_parsing_t
{
    bool is_compressed;
    struct {
        uint8_t major;
        uint8_t minor;
    }version;
    std::array<char,256> copyright;
};
    using magic_numbers_t = std::pair<uint32_t, uint32_t>;

    bool is_cdf(const magic_numbers_t& magic_numbers) noexcept
    {
        return (magic_numbers.first & 0xfff00000) == 0xCDF00000
            && (magic_numbers.second == 0xCCCC0001 || magic_numbers.second == 0x0000FFFF);
    }

    template <typename streamT>
    magic_numbers_t get_magic(streamT& stream)
    {
        stream.seekg(std::ios::beg);
        char buffer[8];
        stream.read(buffer, 8);
        uint32_t magic1 = cdf::endianness::read<uint32_t>(buffer);
        uint32_t magic2 = cdf::endianness::read<uint32_t>(buffer + 4);
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

    template <typename streamT>
    bool parse_CCR(streamT& stream, cdf_parsing_t& context)
    {
        char buffer[312];
        stream.seekg(8);
        stream.read(buffer,312);
        if(endianness::read<cdf_record_type>(buffer+8)==cdf_record_type::CDR)
        {
            std::copy(buffer+56,buffer+312, std::begin(context.copyright));
        }
    }
}
std::optional<CDF> load(const std::string& path)
{
    if (std::filesystem::exists(path))
    {
        std::fstream cdf_file(path, std::ios::in | std::ios::binary);
        auto length = get_file_len(cdf_file);
        auto magic_numbers = get_magic(cdf_file);
        if (is_cdf(magic_numbers))
        {
            return CDF {};
        }
    }
    return std::nullopt;
}
} // namespace cdf::io
