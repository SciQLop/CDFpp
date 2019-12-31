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
#include "../cdf-endianness.hpp"
#include <functional>
#include <tuple>
#include <type_traits>
#include <vector>

namespace cdf::io::common
{
using magic_numbers_t = std::pair<uint32_t, uint32_t>;

bool is_v3x(const magic_numbers_t& magic)
{
    return ((magic.first >> 12) & 0xff) >= 0x30;
}

bool is_cdf(const magic_numbers_t& magic_numbers) noexcept
{
    return (magic_numbers.first & 0xfff00000) == 0xCDF00000
        && (magic_numbers.second == 0xCCCC0001 || magic_numbers.second == 0x0000FFFF);
}

template <typename context_t>
bool is_cdf(const context_t& context) noexcept
{
    return is_cdf(context.magic);
}

bool is_compressed(const magic_numbers_t& magic_numbers) noexcept
{
    return magic_numbers.second == 0xCCCC0001;
}

template <typename stream_t>
void read_buffer(stream_t&& stream, char* buffer, std::size_t pos, std::size_t size)
{
    stream.seekg(pos);
    stream.read(buffer, size);
}

template <typename T, typename stream_t>
T read_buffer(stream_t&& stream, std::size_t pos, std::size_t size)
{
    T buffer(size);
    stream.seekg(pos);
    stream.read(buffer.data(), size);
    return buffer;
}

template <typename src_endianess_t, typename stream_t, typename container_t>
void load_values(stream_t& stream, std::size_t offset, container_t& output)
{
    auto buffer = read_buffer<std::vector<char>>(
        stream, offset, std::size(output) * sizeof(typename container_t::value_type));
    endianness::decode_v<src_endianess_t>(buffer.data(), std::size(buffer), output.data());
}

template <typename value_t, typename stream_t>
struct blk_iterator
{
    using iterator_category = std::forward_iterator_tag;
    using value_type = value_t;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = value_type&;

    std::size_t offset;
    value_t block;
    stream_t& stream;
    std::function<std::size_t(value_t&)> next;

    blk_iterator(std::size_t offset, stream_t& stream, std::function<std::size_t(value_t&)>&& next)
            : offset { offset }, block { stream }, stream { stream }, next { std::move(next) }
    {
        if (offset != 0)
            block.load(offset);
    }

    auto operator==(const blk_iterator& other) { return other.offset == offset; }

    bool operator!=(const blk_iterator& other) { return not(*this == other); }

    blk_iterator& operator+(int n)
    {
        step_forward(n);
        return *this;
    }

    blk_iterator& operator+=(int n)
    {
        step_forward(n);
        return *this;
    }

    blk_iterator& operator++(int n)
    {
        step_forward(n);
        return *this;
    }

    blk_iterator& operator++()
    {
        step_forward();
        return *this;
    }

    void step_forward(int n = 1)
    {
        while (n > 0)
        {
            n--;
            offset = next(block);
            if (offset != 0)
            {
                block.load(offset);
            }
        }
    }

    const value_type* operator->() const { return &block; }
    const value_type& operator*() const { return block; }
    value_type* operator->() { return &block; }
    value_type& operator*() { return block; }
};


}
