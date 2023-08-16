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
#ifdef CDFpp_USE_LIBDEFLATE
#include "./libdeflate.hpp"
#else
#include "./zlib.hpp"
#endif

#include "./rle.hpp"

namespace cdf::io::compression
{

template <typename T>
inline no_init_vector<char> rledeflate(const T& input)
{
    return rle::deflate(input);
}

template <typename T>
no_init_vector<char> gzdeflate(const T& input)
{
#ifdef CDFpp_USE_LIBDEFLATE
    return libdeflate::gzdeflate(input);
#else
    return zlib::gzdeflate(input);
#endif
}

template <cdf_compression_type type, typename T>
no_init_vector<char> deflate(const T& input)
{
    if constexpr (type == cdf_compression_type::gzip_compression)
        return gzdeflate(input);
    if constexpr (type == cdf_compression_type::rle_compression)
        return rledeflate(input);
}

template <typename T>
no_init_vector<char> deflate(
    cdf_compression_type type, const T& input)
{
    if (type == cdf_compression_type::gzip_compression)
        return gzdeflate(input);
    if (type == cdf_compression_type::rle_compression)
        return rledeflate(input);
    return {};
}

}
