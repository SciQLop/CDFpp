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
#include "../cdf-debug.hpp"
#include "cdfpp/no_init_vector.hpp"

#include <cstddef>
#include <libdeflate.h>
#include <vector>

namespace cdf::io::libdeflate
{
namespace _internal
{
    template <typename T>
    CDF_WARN_UNUSED_RESULT std::size_t impl_inflate(
        const T& input, char* output, const std::size_t output_size)
    {

        auto decompressor = libdeflate_alloc_decompressor();
        std::size_t length;
        auto result = libdeflate_gzip_decompress(
            decompressor, input.data(), std::size(input), output, output_size, &length);
        libdeflate_free_decompressor(decompressor);
        if (result == LIBDEFLATE_SUCCESS)
        {
            return length;
        }
        else
        {
            return 0;
        }
    }

    template <typename T>
    CDF_WARN_UNUSED_RESULT no_init_vector<char> impl_deflate(const T& input)
    {
        no_init_vector<char> result(std::max(std::size(input), std::size_t { 16 * 1024UL }));
        auto compressor = libdeflate_alloc_compressor(6);
        auto compressed_size = libdeflate_gzip_compress(
            compressor, input.data(), std::size(input), result.data(), std::size(result));
        libdeflate_free_compressor(compressor);
        if (compressed_size > 0)
        {
            result.resize(compressed_size);
            result.shrink_to_fit();
            return result;
        }
        else
        {
            return {};
        }
    }
}

template <typename T>
std::size_t gzinflate(const T& input, char* output, const std::size_t output_size)
{
    using namespace _internal;
    return impl_inflate(input, output, output_size);
}

template <typename T>
no_init_vector<char> gzdeflate(const T& input)
{
    using namespace _internal;
    return impl_deflate(input);
}

}
