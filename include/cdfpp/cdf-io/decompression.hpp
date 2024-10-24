/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2024, Laboratory of Plasma Physics- CNRS
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the “Software”), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is furnished to do
-- so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
-- INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
-- PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
-- HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
-- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
-- SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#pragma once
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
