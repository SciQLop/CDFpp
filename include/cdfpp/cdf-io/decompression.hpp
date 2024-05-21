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
#include "../cdf-enums.hpp"
#include <cdfpp_config.h>
#include <stdexcept>
#include <vector>
#ifdef CDFpp_USE_LIBDEFLATE
#include "./libdeflate.hpp"
#else
#include "./zlib.hpp"
#endif
#ifdef CDFPP_USE_ZSTD
#include "./zstd.hpp"
#endif

#include "./rle.hpp"

namespace cdf::io::decompression
{

template <typename T>
inline std::size_t rleinflate(const T& input, char* output, const std::size_t output_size)
{
    return rle::inflate(input, output, output_size);
}

template <typename T>
std::size_t gzinflate(const T& input, char* output, const std::size_t output_size)
{
#ifdef CDFpp_USE_LIBDEFLATE
    return libdeflate::gzinflate(input, output, output_size);
#else
    return zlib::gzinflate(input, output, output_size);
#endif
}

template <cdf_compression_type type, typename T>
std::size_t inflate(const T& input, char* output, const std::size_t output_size)
{
    if constexpr (type == cdf_compression_type::gzip_compression)
        return gzinflate(input, output, output_size);
    if constexpr (type == cdf_compression_type::rle_compression)
        return rleinflate(input, output, output_size);
#ifdef CDFPP_USE_ZSTD
    if constexpr (type == cdf_compression_type::zstd_compression)
        return zstd::inflate(input, output, output_size);
#endif
    throw std::runtime_error("Unknown compression type.");
}

template <typename T>
std::size_t inflate(
    cdf_compression_type type, const T& input, char* output, const std::size_t output_size)
{
    if (type == cdf_compression_type::gzip_compression)
        return gzinflate(input, output, output_size);
    if (type == cdf_compression_type::rle_compression)
        return rleinflate(input, output, output_size);
#ifdef CDFPP_USE_ZSTD
    if (type == cdf_compression_type::zstd_compression)
        return zstd::inflate(input, output, output_size);
#endif
    throw std::runtime_error("Unknown compression type.");
}

}
