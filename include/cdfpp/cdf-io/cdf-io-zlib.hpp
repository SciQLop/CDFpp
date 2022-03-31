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

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>
#define ZLIB_CONST
#include <zlib.h>

namespace cdf::io::zlib
{
namespace _internal
{
    struct in
    {
    };
    struct de
    {
    };

    struct gzip_t
    {
    };
    struct zlib_t
    {
    };

    template <typename T>
    static constexpr bool is_in_v = std::is_same_v<T, in>;

    template <typename T>
    static constexpr bool is_de_v = std::is_same_v<T, de>;

    template <typename T>
    static constexpr bool is_gzip_v = std::is_same_v<T, gzip_t>;

    template <typename T>
    static constexpr bool is_zlib_v = std::is_same_v<T, zlib_t>;

    // Taken from:
    //   https://github.com/qpdf/qpdf/blob/master/libqpdf/Pl_Flate.cc

    template <typename in_de, typename zlib_or_gzip_t, typename T, typename U>
    CDF_WARN_UNUSED_RESULT bool impl_flate(
        U& input, std::vector<T>& output, int flush, int compression_lvl = Z_BEST_COMPRESSION)
    {
        std::size_t input_bytes_cnt = std::size(input) * sizeof(typename U::value_type);
        std::size_t output_byte_pos = std::size(output) * sizeof(T);
        constexpr std::size_t chunk_sz = 65536UL;
        char chunk[chunk_sz];
        z_stream fstream;
        fstream.zalloc = Z_NULL;
        fstream.zfree = Z_NULL;
        fstream.opaque = Z_NULL;
        fstream.avail_in = 0;
        fstream.next_in = Z_NULL;

        auto ret = [&]() {
            if constexpr (is_in_v<in_de>)
            {
                // to support gzip format
                return inflateInit2(&fstream, 32 + MAX_WBITS);
            }
            else
            {
                if constexpr (is_gzip_v<zlib_or_gzip_t>)
                    return deflateInit2(
                        &fstream, compression_lvl, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
                else
                    return deflateInit(&fstream, compression_lvl);
            }
        }();
        if (ret != Z_OK)
            return {};

        fstream.avail_in = input_bytes_cnt;
        fstream.next_in = reinterpret_cast<const Bytef*>(input.data());
        bool done = false;
        while (!done)
        {
            fstream.avail_out = chunk_sz;
            fstream.next_out = reinterpret_cast<Bytef*>(chunk);
            ret = [&]() {
                if constexpr (is_in_v<in_de>)
                    return inflate(&fstream, flush);
                else
                    return deflate(&fstream, flush);
            }();
            if (fstream.avail_in)
                fstream.next_in = reinterpret_cast<const Bytef*>(input.data()) + input_bytes_cnt
                    - fstream.avail_in;
            switch (ret)
            {
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                case Z_BUF_ERROR:
                    done = true;
                    break;
                case Z_STREAM_END:
                    done = true;
                    [[fallthrough]];
                case Z_OK:
                {
                    if ((fstream.avail_in == 0) && (fstream.avail_out > 0))
                    {
                        done = true;
                    }
                    std::size_t new_data = chunk_sz - fstream.avail_out;
                    if (new_data)
                    {
                        output.resize((output_byte_pos + new_data) / sizeof(T) + 1);
                        std::copy_n(chunk, new_data,
                            reinterpret_cast<char*>(output.data()) + output_byte_pos);
                        output_byte_pos += new_data;
                    }
                }
                break;
            }
        }
        output.resize((output_byte_pos) / sizeof(T));
        if constexpr (is_in_v<in_de>)
            inflateEnd(&fstream);
        else
            deflateEnd(&fstream);
        return true;
    }


    template <typename in_de, typename zlib_or_gzip_t, typename T, typename U>
    std::vector<T> flate(U& input, int flush, int compression_lvl = Z_BEST_COMPRESSION)
    {
        std::vector<T> result;
        if(impl_flate<in_de, zlib_or_gzip_t>(input, result, flush, compression_lvl))
            return result;
        else
            throw std::runtime_error{"Failed to decompress data"};
    }

    template <typename in_de, typename zlib_or_gzip_t, typename T, typename U>
    CDF_WARN_UNUSED_RESULT bool flate(
        U& input, std::vector<T>& output, int flush, int compression_lvl = Z_BEST_COMPRESSION)
    {
        return impl_flate<in_de, zlib_or_gzip_t>(input, output, flush, compression_lvl);
    }
}


template <typename T, typename U>
std::vector<T> inflate(U& input)
{
    using namespace _internal;
    return flate<in, zlib_t, T>(input, Z_NO_FLUSH);
}

template <typename T, typename U>
CDF_WARN_UNUSED_RESULT bool inflate(U& input, std::vector<T>& output)
{
    using namespace _internal;
    return flate<in, zlib_t>(input, output, Z_NO_FLUSH);
}

template <typename T>
std::vector<char> deflate(T& input)
{
    using namespace _internal;
    return flate<de, zlib_t, char>(input, Z_FINISH, Z_DEFAULT_COMPRESSION);
}

template <typename T>
CDF_WARN_UNUSED_RESULT bool deflate(T& input, std::vector<char>& output)
{
    using namespace _internal;
    return flate<de, zlib_t>(input, output, Z_FINISH, Z_DEFAULT_COMPRESSION);
}

template <typename T, typename U>
std::vector<T> gzinflate(U& input)
{
    using namespace _internal;
    return flate<in, gzip_t, T>(input, Z_NO_FLUSH);
}

template <typename T, typename U>
bool gzinflate(U& input, std::vector<T>& output)
{
    using namespace _internal;
    return flate<in, gzip_t>(input, output, Z_NO_FLUSH);
}


template <typename T>
std::vector<char> gzdeflate(T& input)
{
    using namespace _internal;
    return flate<de, gzip_t, char>(input, Z_FINISH, Z_DEFAULT_COMPRESSION);
}

template <typename T>
CDF_WARN_UNUSED_RESULT bool gzdeflate(T& input, std::vector<char>& output)
{
    using namespace _internal;
    return flate<de, gzip_t, char>(input, output, Z_FINISH, Z_DEFAULT_COMPRESSION);
}
}
