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
#include "attribute.hpp"
#include "cdf-data.hpp"
#include "cdf-enums.hpp"
#include <cstdint>
#include <vector>

namespace cdf::io::buffers
{

template <typename stream_t>
struct stream_adapter
{
    stream_t stream;
    stream_adapter(stream_t&& stream) : stream { std::move(stream) } {}

    template <typename T>
    auto impl_read(T& array, std::size_t offset, std::size_t size) -> decltype(array.data(), void())
    {
        stream.seekg(offset);
        stream.read(array.data(), size);
    }

    template <typename T>
    auto impl_read(T& array, std::size_t offset, std::size_t size) -> decltype(*array, void())
    {
        stream.seekg(offset);
        stream.read(array, size);
    }

    std::vector<char> read(std::size_t offset, std::size_t size)
    {
        std::vector<char> buffer(size);
        impl_read(buffer, offset, size);
        return buffer;
    }

    template <std::size_t size>
    std::array<char, size> read(std::size_t offset)
    {
        std::array<char, size> buffer;
        impl_read(buffer, offset, size);
        return buffer;
    }

    void read(char* data, std::size_t offset, std::size_t size) { impl_read(data, offset, size); }
};

template <typename array_t>
struct array_adapter
{
    const array_t& array;
    array_adapter(const array_t& array) : array { array } {}

    template <typename T>
    void impl_read(T& output_array, std::size_t offset, std::size_t size)
    {
        if constexpr(std::is_pointer_v<std::remove_reference_t<T>>)
            std::copy_n(std::cbegin(array) + offset, size, output_array);
        else
            std::copy_n(std::cbegin(array) + offset, size, std::begin(output_array));
    }

    std::vector<char> read(std::size_t offset, std::size_t size)
    {
        std::vector<char> buffer(size);
        impl_read(buffer, offset, size);
        return buffer;
    }

    template <std::size_t size>
    std::array<char, size> read(std::size_t offset)
    {
        std::array<char, size> buffer;
        impl_read(buffer, offset, size);
        return buffer;
    }

    void read(char* data, std::size_t offset, std::size_t size) { impl_read(data, offset, size); }
};

}
