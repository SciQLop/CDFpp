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
#include "cdfpp/no_init_vector.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
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

template <typename T>
inline void _deflate_copy(const T* input, std::size_t count, no_init_vector<char>& dest)
{
    if (count)
    {
        const auto sz = std::size(dest);
        dest.resize(sz + count);
        std::memcpy(dest.data() + sz, input, count);
    }
}

template <typename T>
inline no_init_vector<char> deflate(const T& input)
{
    no_init_vector<char> result;
    result.reserve(std::size(input));
    auto input_cursor = std::cbegin(input);
    auto last_copy_cursor = std::cbegin(input);
    while (input_cursor != std::cend(input))
    {
        auto value = *input_cursor;
        if (value == 0)
        {
            _deflate_copy(input.data() + (last_copy_cursor - std::cbegin(input)),
                input_cursor - last_copy_cursor, result);
            auto z_count = 0UL;
            do
            {
                input_cursor++;
                z_count++;
            } while (input_cursor != std::cend(input) and *input_cursor == 0);
            last_copy_cursor = input_cursor;
            result.push_back(0);
            result.push_back(z_count - 1);
        }
        else
        {
            input_cursor++;
        }
    }
    _deflate_copy(input.data() + (last_copy_cursor - std::cbegin(input)),
        input_cursor - last_copy_cursor, result);
    return result;
}

}
