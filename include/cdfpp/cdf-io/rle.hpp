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

#include <algorithm>
#include <cstdint>
#include <vector>

namespace cdf::io::rle
{
namespace _internal
{

}

template <typename T>
inline std::size_t inflate(const T& input, char* output, const std::size_t)
{
    auto output_cursor = output;
    auto input_cursor = std::cbegin(input);
    while (input_cursor != std::cend(input))
    {
        auto value = *input_cursor;
        if (value == 0)
        {
            input_cursor++;
            std::size_t count = static_cast<unsigned char>(*input_cursor) + 1;
            std::generate_n(output_cursor, count, []() constexpr { return 0; });
            output_cursor += count;
        }
        else
        {
            *output_cursor = value;
            output_cursor++;
        }
        input_cursor++;
    }
    return output_cursor - output;
}


}
