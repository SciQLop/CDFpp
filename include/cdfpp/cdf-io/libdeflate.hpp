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
