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
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>
#include <zlib.h>

namespace cdf::io::zlib
{
namespace
{
    struct in
    {
    };
    struct de
    {
    };

    template <typename T>
    static constexpr bool is_in_v = std::is_same_v<T, in>;

    template <typename T>
    static constexpr bool is_de_v = std::is_same_v<T, de>;

    // Taken from:
    //   https://github.com/qpdf/qpdf/blob/master/libqpdf/Pl_Flate.cc

    template <typename in_de, typename T, typename U>
    std::vector<T> flate(std::vector<U>& input, int flush, int compression_lvl = Z_BEST_COMPRESSION)
    {
        std::size_t input_bytes_cnt = std::size(input) * sizeof(U);
        std::size_t chunk_sz = std::max(std::size(input), 512UL);
        char chunk[chunk_sz];
        std::vector<char> output;
        z_stream fstream;
        fstream.zalloc = Z_NULL;
        fstream.zfree = Z_NULL;
        fstream.opaque = Z_NULL;
        fstream.avail_in = 0;
        fstream.next_in = Z_NULL;

        auto ret = [&]() {
            if constexpr (is_in_v<in_de>)
                return inflateInit(&fstream);
            else
                return deflateInit(&fstream, compression_lvl);
        }();
        if (ret != Z_OK)
            return {};

        fstream.avail_in = input_bytes_cnt;
        fstream.next_in = reinterpret_cast<Bytef*>(input.data());
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
                fstream.next_in
                    = reinterpret_cast<Bytef*>(input.data()) + input_bytes_cnt - fstream.avail_in;
            switch (ret)
            {
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                case Z_BUF_ERROR:
                    done = true;
                    break;
                case Z_STREAM_END:
                    done = true;
                case Z_OK:
                {
                    if ((fstream.avail_in == 0) && (fstream.avail_out > 0))
                    {
                        done = true;
                    }
                    std::copy_n(chunk, chunk_sz - fstream.avail_out, std::back_inserter(output));
                }
                break;
            }
        }
        if constexpr (is_in_v<in_de>)
            inflateEnd(&fstream);
        else
            deflateEnd(&fstream);
        if constexpr (std::is_same_v<char, T>)
            return output;
        else
        {
            std::vector<T> result(std::size(output) / sizeof(T));
            std::copy(
                std::cbegin(output), std::cend(output), reinterpret_cast<char*>(result.data()));
            return result;
        }
    }

}

template <typename T, typename U>
std::vector<T> inflate(std::vector<U>& input)
{
    return flate<in, T>(input, Z_NO_FLUSH);
}


template <typename T>
std::vector<char> deflate(std::vector<T>& input)
{
    return flate<de, char>(input, Z_FINISH, Z_DEFAULT_COMPRESSION);
}
}
