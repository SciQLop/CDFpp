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
#include <vector>
#define ZLIB_CONST
#include <zlib.h>


namespace cdf::io::zlib
{
namespace _internal
{

    // Taken from:
    //   https://github.com/qpdf/qpdf/blob/master/libqpdf/Pl_Flate.cc
    template <typename T>
    CDF_WARN_UNUSED_RESULT std::size_t impl_inflate(
        const T& input, char* output, const std::size_t output_size)
    {

        z_stream fstream;
        fstream.zalloc = Z_NULL;
        fstream.zfree = Z_NULL;
        fstream.opaque = Z_NULL;
        fstream.avail_in = std::size(input);
        fstream.next_in = reinterpret_cast<const Bytef*>(input.data());
        fstream.avail_out = output_size;
        fstream.next_out = reinterpret_cast<Bytef*>(output);

        if (Z_OK != inflateInit2(&fstream, 32 + MAX_WBITS))
            return 0;

        auto ret = inflate(&fstream, Z_FINISH);
        inflateEnd(&fstream);

        if (ret == Z_STREAM_END)
            return output_size - fstream.avail_out;
        else
            return 0;
    }

    template <typename T>
    CDF_WARN_UNUSED_RESULT no_init_vector<char> impl_deflate(const T& input)
    {
        no_init_vector<char> result(std::max(std::size(input), 16 * 1024UL));
        z_stream fstream;
        fstream.zalloc = Z_NULL;
        fstream.zfree = Z_NULL;
        fstream.opaque = Z_NULL;
        fstream.avail_in = std::size(input);
        fstream.next_in = reinterpret_cast<const Bytef*>(input.data());
        fstream.avail_out = std::size(result);
        fstream.next_out = reinterpret_cast<Bytef*>(result.data());
        if (Z_OK
            != deflateInit2(
                &fstream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 6, Z_DEFAULT_STRATEGY))
            return {};
        auto ret = deflate(&fstream, Z_FINISH);
        deflateEnd(&fstream);
        if (ret == Z_STREAM_END)
        {
            result.resize(fstream.total_out);
            result.shrink_to_fit();
            return result;
        }
        else
            return {};
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
