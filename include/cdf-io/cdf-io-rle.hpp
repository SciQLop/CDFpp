#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2022, Plasma Physics Laboratory - CNRS
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

namespace cdf::io::rle
{
namespace _internal
{

}

inline bool deflate(const std::vector<char>& input, std::vector<char>& result)
{
    result.reserve(std::size(input));
    auto cursor = std::cbegin(input);
    while (cursor != std::cend(input))
    {
        auto value = *cursor;
        if (value == 0)
        {
            cursor++;
            std::size_t count = static_cast<unsigned char>(*cursor)+ 1;
            std::generate_n(std::back_inserter(result), count, []()constexpr{return 0;});
        }
        else
        {
            result.push_back(value);
        }
        cursor++;
    }
    return true;
}


}
