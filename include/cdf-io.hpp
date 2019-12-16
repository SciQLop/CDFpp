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
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <utility>

namespace cdf::io
{
namespace
{
    enum class cdf_record_type : int32_t
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

    enum class cdf_encoding : int32_t
    {
        network = 1,
        SUN = 2,
        VAX = 3,
        decstation = 4,
        SGi = 5,
        IBMPC = 6,
        IBMRS = 7,
        PPC = 9,
        HP = 11,
        NeXT = 12,
        ALPHAOSF1 = 13,
        ALPHAVMSd = 14,
        ALPHAVMSg = 15,
        ALPHAVMSi = 16,
        ARM_LITTLE = 17,
        ARM_BIG = 18,
        IA64VMSi = 19,
        IA64VMSd = 20,
        IA64VMSg = 21
    };

    bool is_big_endian(cdf_encoding encoding)
    {
        return encoding == cdf_encoding::network || encoding == cdf_encoding::SUN
            || encoding == cdf_encoding::NeXT || encoding == cdf_encoding::PPC
            || encoding == cdf_encoding::SGi || encoding == cdf_encoding::IBMRS
            || encoding == cdf_encoding::ARM_BIG;
    }
    bool is_little_endian(cdf_encoding encoding) { return !is_big_endian(encoding); }

    using magic_numbers_t = std::pair<uint32_t, uint32_t>;

    template <std::size_t offset_param, typename T>
    struct field_t
    {
        using type = T;
        inline static constexpr std::size_t offset = offset_param;
        T value;
        operator T() { return value; }
        field_t& operator=(const T& v)
        {
            this->value = v;
            return *this;
        }
    };

    struct v3x_tag
    {
    };

    struct v2x_tag
    {
    };

    template <typename version_t>
    struct cdf_parsing_t
    {
        inline static constexpr bool v3 = std::is_same_v<version_t, v3x_tag>;
        magic_numbers_t magic;
        bool is_compressed;
        std::conditional_t<v3, field_t<28, cdf_encoding>, field_t<20, cdf_encoding>> encoding;
        std::conditional_t<v3, field_t<12, uint64_t>, field_t<8, uint32_t>> GDR_offset;
        std::conditional_t<v3, field_t<28, uint64_t>, field_t<16, uint32_t>> first_ADR_offset;
        std::conditional_t<v3, field_t<12, uint64_t>, field_t<8, uint32_t>> first_rVDR_offset;
        std::conditional_t<v3, field_t<20, uint64_t>, field_t<12, uint32_t>> first_zVDR_offset;
        std::conditional_t<v3, field_t<36, uint64_t>, field_t<20, uint32_t>> eof;
        std::conditional_t<v3, field_t<44, uint32_t>, field_t<24, uint32_t>> rVars_count;
        std::conditional_t<v3, field_t<60, uint32_t>, field_t<40, uint32_t>> zVars_count;
        std::conditional_t<v3, field_t<48, uint32_t>, field_t<28, uint32_t>> attributes_count;
        std::vector<std::size_t> rDim_sizes;
        struct
        {
            uint32_t release;
            uint32_t increment;
        } version;
        std::array<char, 256> copyright;
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
        uint32_t magic1 = cdf::endianness::read<uint32_t>(buffer);
        uint32_t magic2 = cdf::endianness::read<uint32_t>(buffer + 4);
        return { magic1, magic2 };
    }
    template <typename buffer_t, typename T>
    void extract_field(buffer_t buffer, T& field)
    {
        field = endianness::read<typename T::type>(buffer + T::offset);
    }

    template <typename buffer_t, typename... Ts>
    void extract_fields(buffer_t buffer, Ts&&... fields)
    {
        (extract_field(buffer, std::forward<Ts>(fields)), ...);
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
            return endianness::read<cdf_record_type>(buffer + 8);
        else
            return endianness::read<cdf_record_type>(buffer + 4);
    }

    template <typename cdf_version_tag_t, typename streamT, typename context_t>
    bool parse_CDR(streamT& stream, context_t& context)
    {
        char buffer[312];
        stream.seekg(8);
        stream.read(buffer, 312);
        if (record_type<cdf_version_tag_t>(buffer) != cdf_record_type::CDR)
            return false;
        extract_fields(buffer, context.encoding, context.GDR_offset);
        std::copy(buffer + 56, buffer + 312, std::begin(context.copyright));
        return true;
    }
    template <typename cdf_version_tag_t, typename streamT, typename context_t>
    bool parse_GDR(streamT& stream, context_t& context)
    {
        char buffer[84];
        stream.seekg(context.GDR_offset.value);
        stream.read(buffer, 84);
        if (record_type<cdf_version_tag_t>(buffer) != cdf_record_type::GDR)
            return false;
        extract_fields(buffer, context.first_rVDR_offset, context.first_zVDR_offset,
            context.first_ADR_offset, context.eof, context.rVars_count);
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
        return CDF {};
    }
}


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
