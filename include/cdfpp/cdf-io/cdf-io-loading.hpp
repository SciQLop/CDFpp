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
#include "cdf-io-buffers.hpp"
#include "reflection.hpp"
#include <string>
namespace cdf::io
{


template <typename buffer_t, typename T>
inline void load_field(buffer_t buffer, std::size_t offset, T& field)
{
    /*if constexpr (std::is_same_v<std::string, typename T::type>)
    {
        std::size_t size = 0;
        for (; size < T::len; size++)
        {
            if (buffer[T::offset - offset + size] == '\0')
                break;
        }
        field = std::string { buffers::get_data_ptr(buffer) + offset, size };
    }
    else*/
        field = cdf::endianness::decode<endianness::big_endian_t, T>(
            buffers::get_data_ptr(buffer) + offset);
}

template <typename buffer_t, typename T>
inline void load_fields(buffer_t buffer, [[maybe_unused]] std::size_t offset, T&& field)
{
        load_field(buffer, offset, std::forward<T>(field));
}

template <typename buffer_t, typename T, typename... Ts>
inline void load_fields(buffer_t buffer, [[maybe_unused]] std::size_t offset, T&& field, Ts&&... fields)
{
        load_field(buffer, offset, std::forward<T>(field));
        load_fields(buffer, offset+sizeof(T), std::forward<Ts>(fields)...);
}

template <typename T, typename U>
void load_record(T&& s, U&& buffer, std::size_t offset)
{
    static constexpr std::size_t count = count_members<T>;
    if constexpr (count == 1)
    {
        auto& [a] = s;
        load_fields(buffer, offset, a);
    }
    if constexpr (count == 2)
    {
        auto& [a, b] = s;
        load_fields(buffer, offset, a, b);
    }
    if constexpr (count == 3)
    {
        auto& [a, b, c] = s;
        load_fields(buffer, offset, a, b, c);
    }
    if constexpr (count == 4)
    {
        auto& [a, b, c, d] = s;
        load_fields(buffer, offset, a, b, c, d);
    }
    if constexpr (count == 5)
    {
        auto& [a, b, c, d, e] = s;
        load_fields(buffer, offset, a, b, c, d, e);
    }
    if constexpr (count == 6)
    {
        auto& [a, b, c, d, e, f] = s;
        load_fields(buffer, offset, a, b, c, d, e, f);
    }
    if constexpr (count == 7)
    {
        auto& [a, b, c, d, e, f, g] = s;
        load_fields(buffer, offset, a, b, c, d, e, f, g);
    }
    if constexpr (count == 8)
    {
        auto& [a, b, c, d, e, f, g, h] = s;
        load_fields(buffer, offset, a, b, c, d, e, f, g, h);
    }
    if constexpr (count == 9)
    {
        auto& [a, b, c, d, e, f, g, h, i] = s;
        load_fields(buffer, offset, a, b, c, d, e, f, g, h, i);
    }
    if constexpr (count == 10)
    {
        auto& [a, b, c, d, e, f, g, h, i, j] = s;
        load_fields(buffer, offset, a, b, c, d, e, f, g, h, i, j);
    }
    if constexpr (count == 11)
    {
        auto& [a, b, c, d, e, f, g, h, i, j, k] = s;
        load_fields(buffer, offset, a, b, c, d, e, f, g, h, i, j, k);
    }
}

}
