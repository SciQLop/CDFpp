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
#include "../cdf-endianness.hpp"
#include "cdf-io-buffers.hpp"
#include "cdf-io-special-fields.hpp"
#include "reflection.hpp"
#include <string>
namespace cdf::io
{

template <typename T, typename U>
inline constexpr std::size_t offset_of(const T&, const U&)
{
    using value_type = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (is_string_field_v<value_type>)
    {
        return value_type::max_len;
    }
    else
    {
        return sizeof(value_type);
    }
}

template <typename T, typename U>
inline std::size_t offset_of(const table_field<T>& table, const U& record)
{
    return record.size(table);
}

template <typename buffer_t, typename record_t, typename T>
inline void load_field(buffer_t buffer, const record_t& r, std::size_t offset, T& field)
{
    if constexpr (is_string_field_v<T>)
    {
        std::size_t size = 0;
        for (; size < T::max_len; size++)
        {
            if (buffers::get_data_ptr(buffer)[offset + size] == '\0')
                break;
        }
        field.value = std::string { buffers::get_data_ptr(buffer) + offset, size };
    }
    else
    {
        if constexpr (is_table_field_v<T>)
        {
            const auto bytes = r.size(field);
            field.values.resize(bytes / sizeof(typename T::value_type));
            std::memcpy(field.values.data(), buffers::get_data_ptr(buffer) + offset, bytes);
            cdf::endianness::decode_v<endianness::big_endian_t>(field.values.data(), bytes / sizeof(typename T::value_type));
        }
        else
            field = cdf::endianness::decode<endianness::big_endian_t, T>(
                buffers::get_data_ptr(buffer) + offset);
    }
}

template <typename buffer_t, typename record_t, typename T>
inline void load_fields(
    buffer_t buffer, const record_t& r, [[maybe_unused]] std::size_t offset, T&& field)
{
    load_field(buffer, r, offset, std::forward<T>(field));
}

template <typename buffer_t, typename record_t, typename T, typename... Ts>
inline void load_fields(buffer_t buffer, const record_t& r, [[maybe_unused]] std::size_t offset,
    T&& field, Ts&&... fields)
{
    load_field(buffer, r, offset, std::forward<T>(field));
    load_fields(buffer, r, offset + offset_of(field, r), std::forward<Ts>(fields)...);
}

// this looks quite ugly bit it is worth it!
template <typename T, typename U>
void load_record(T& s, const U& buffer, std::size_t offset)
{
    static constexpr std::size_t count = count_members<T>;
    if constexpr (count == 1)
    {
        auto& [a] = s;
        load_fields(buffer, s, offset, a);
    }
    if constexpr (count == 2)
    {
        auto& [a, b] = s;
        load_fields(buffer, s, offset, a, b);
    }
    if constexpr (count == 3)
    {
        auto& [a, b, c] = s;
        load_fields(buffer, s, offset, a, b, c);
    }
    if constexpr (count == 4)
    {
        auto& [a, b, c, d] = s;
        load_fields(buffer, s, offset, a, b, c, d);
    }
    if constexpr (count == 5)
    {
        auto& [a, b, c, d, e] = s;
        load_fields(buffer, s, offset, a, b, c, d, e);
    }
    if constexpr (count == 6)
    {
        auto& [a, b, c, d, e, f] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f);
    }
    if constexpr (count == 7)
    {
        auto& [a, b, c, d, e, f, g] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g);
    }
    if constexpr (count == 8)
    {
        auto& [a, b, c, d, e, f, g, h] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g, h);
    }
    if constexpr (count == 9)
    {
        auto& [a, b, c, d, e, f, g, h, i] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g, h, i);
    }
    if constexpr (count == 10)
    {
        auto& [a, b, c, d, e, f, g, h, i, j] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g, h, i, j);
    }
    if constexpr (count == 11)
    {
        auto& [a, b, c, d, e, f, g, h, i, j, k] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g, h, i, j, k);
    }
    if constexpr (count == 12)
    {
        auto& [a, b, c, d, e, f, g, h, i, j, k, l] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g, h, i, j, k, l);
    }
    if constexpr (count == 13)
    {
        auto& [a, b, c, d, e, f, g, h, i, j, k, l, m] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g, h, i, j, k, l, m);
    }
    if constexpr (count == 14)
    {
        auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g, h, i, j, k, l, m, n);
    }
    if constexpr (count == 15)
    {
        auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o);
    }
    if constexpr (count == 16)
    {
        auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p);
    }
    if constexpr (count == 17)
    {
        auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q] = s;
        load_fields(buffer, s, offset, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q);
    }
}

}
