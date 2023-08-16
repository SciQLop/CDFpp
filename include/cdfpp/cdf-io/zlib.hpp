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
