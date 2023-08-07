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
#include "./buffers.hpp"
#include "./records-saving.hpp"
#include "cdfpp/cdf-enums.hpp"
#include "cdfpp/cdf-file.hpp"
#include "cdfpp/no_init_vector.hpp"
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
    std::size_t estimate_size(const CDF& cdf)
    {
        auto var_size = std::accumulate(std::cbegin(cdf.variables), std::cend(cdf.variables), 0UL,
            [](const Variable& v, std::size_t sz) { return sz + v.bytes(); });
        return var_size + (1 << 16);
    }

    template <typename T>
    [[nodiscard]] bool impl_save(const CDF& cdf, T& writer)
    {
        common::magic_numbers_t magic { 0xCDF30001,
            cdf.compression == cdf_compression_type::no_compression ? 0x0000FFFF : 0xCCCC0001 };
        save_record(magic, writer);
        return true;
    }

} // namespace


[[nodiscard]] bool save(const CDF& cdf, const std::string& path)
{
    buffers::file_writer writer { path };
    return impl_save(cdf, writer);
}

[[nodiscard]] no_init_vector<char> save(const CDF& cdf)
{
    no_init_vector<char> data;
    data.reserve(estimate_size(cdf));
    buffers::in_memory_writer writer { data };
    if (impl_save(cdf, writer))
        return data;
    return {};
}

}
