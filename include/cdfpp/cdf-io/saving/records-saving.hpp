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

#include "../desc-records.hpp"
#include "../endianness.hpp"
#include "../reflection.hpp"
#include "../special-fields.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <utility>

namespace cdf::io
{

template <typename T>
[[nodiscard]] constexpr std::size_t record_size(const T& s);

template <typename record_t, typename T>
[[nodiscard]] constexpr std::size_t field_size(const record_t& s, T& field)
{
    if constexpr (is_string_field_v<T>)
    {
        return T::max_len;
    }
    else
    {
        if constexpr (is_table_field_v<T>)
        {
            return s.size(field);
        }
        else
        {
            return sizeof(T);
        }
    }
}

template <typename record_t, typename T>
[[nodiscard]] constexpr inline std::size_t fields_size(const record_t& s, T&& field)
{
    using Field_t = std::remove_cv_t<std::remove_reference_t<T>>;
    constexpr std::size_t count = count_members<Field_t>;
    if constexpr (std::is_compound_v<Field_t> && (count > 1)
        && (not is_string_field_v<Field_t>)&&(not is_table_field_v<Field_t>))
        return record_size(field);
    else
        return field_size(s, std::forward<T>(field));
}

template <typename record_t, typename T, typename... Ts>
[[nodiscard]] constexpr inline std::size_t fields_size(const record_t& s, T&& field, Ts&&... fields)
{
    return fields_size(s, std::forward<T>(field)) + fields_size(s, std::forward<Ts>(fields)...);
}

template <typename T>
[[nodiscard]] constexpr std::size_t record_size(const T& s)
{
    constexpr std::size_t count = count_members<T>;
    static_assert(count <= 31);
    if constexpr (count == 1)
    {
        auto& [_0] = s;
        return fields_size(s, _0);
    }

    if constexpr (count == 2)
    {
        auto& [_0, _1] = s;
        return fields_size(s, _0, _1);
    }

    if constexpr (count == 3)
    {
        auto& [_0, _1, _2] = s;
        return fields_size(s, _0, _1, _2);
    }

    if constexpr (count == 4)
    {
        auto& [_0, _1, _2, _3] = s;
        return fields_size(s, _0, _1, _2, _3);
    }

    if constexpr (count == 5)
    {
        auto& [_0, _1, _2, _3, _4] = s;
        return fields_size(s, _0, _1, _2, _3, _4);
    }

    if constexpr (count == 6)
    {
        auto& [_0, _1, _2, _3, _4, _5] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5);
    }

    if constexpr (count == 7)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6);
    }

    if constexpr (count == 8)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7);
    }

    if constexpr (count == 9)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8);
    }

    if constexpr (count == 10)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9);
    }

    if constexpr (count == 11)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10);
    }

    if constexpr (count == 12)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11);
    }

    if constexpr (count == 13)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12);
    }

    if constexpr (count == 14)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13);
    }

    if constexpr (count == 15)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14);
    }

    if constexpr (count == 16)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15] = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15);
    }

    if constexpr (count == 17)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16] = s;
        return fields_size(
            _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16);
    }

    if constexpr (count == 18)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17] = s;
        return fields_size(
            _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17);
    }

    if constexpr (count == 19)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18]
            = s;
        return fields_size(
            _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18);
    }

    if constexpr (count == 20)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19);
    }

    if constexpr (count == 21)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20);
    }

    if constexpr (count == 22)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20, _21);
    }

    if constexpr (count == 23)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20, _21, _22);
    }

    if constexpr (count == 24)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20, _21, _22, _23);
    }

    if constexpr (count == 25)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20, _21, _22, _23, _24);
    }

    if constexpr (count == 26)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20, _21, _22, _23, _24, _25);
    }

    if constexpr (count == 27)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25, _26]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26);
    }

    if constexpr (count == 28)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25, _26, _27]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27);
    }

    if constexpr (count == 29)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25, _26, _27, _28]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28);
    }

    if constexpr (count == 30)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29);
    }

    if constexpr (count == 31)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30]
            = s;
        return fields_size(s, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15,
            _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30);
    }
}

template <typename T, typename U>
std::size_t save_record(T& s, U& writer);

template <typename writer_t, typename T>
inline std::size_t save_field(writer_t& writer, T& field)
{
    if constexpr (is_string_field_v<T>)
    {
        writer.write(field.value.data(), field.value.length());
        return writer.fill('\0', T::max_len - field.value.length());
    }
    else
    {
        if constexpr (is_table_field_v<T>)
        {
            return 0;
        }
        else
        {
            auto v = endianness::decode<endianness::big_endian_t, T>(&field);
            return writer.write(reinterpret_cast<char*>(&v), sizeof(T));
        }
    }
}

template <typename writer_t, typename T>
inline std::size_t save_fields(writer_t& writer, T&& field)
{
    using Field_t = std::remove_cv_t<std::remove_reference_t<T>>;
    static constexpr std::size_t count = count_members<Field_t>;
    if constexpr (std::is_compound_v<Field_t> && (count > 1)
        && (not is_string_field_v<Field_t>)&&(not is_table_field_v<Field_t>))
        return save_record(field, writer);
    else
        return save_field(writer, std::forward<T>(field));
}

template <typename writer_t, typename T, typename... Ts>
inline std::size_t save_fields(writer_t& writer, T&& field, Ts&&... fields)
{
    save_fields(writer, std::forward<T>(field));
    return save_fields(writer, std::forward<Ts>(fields)...);
}

template <typename T>
auto set_headers(T& s) -> decltype(s.header.record_size, s.header.record_type, void())
{
    s.header.record_size = record_size(s);
    s.header.record_type = decltype(s.header)::expected_record_type;
}


// this looks quite ugly bit it is worth it!
template <typename T, typename U>
[[nodiscard]] std::size_t save_record(T& s, U& writer)
{
    static constexpr std::size_t count = count_members<T>;
    static_assert(count <= 31);

    if constexpr (is_record_v<T>)
        set_headers(s);

    if constexpr (count == 1)
    {
        auto& [_0] = s;
        return save_fields(writer, _0);
    }

    if constexpr (count == 2)
    {
        auto& [_0, _1] = s;
        return save_fields(writer, _0, _1);
    }

    if constexpr (count == 3)
    {
        auto& [_0, _1, _2] = s;
        return save_fields(writer, _0, _1, _2);
    }

    if constexpr (count == 4)
    {
        auto& [_0, _1, _2, _3] = s;
        return save_fields(writer, _0, _1, _2, _3);
    }

    if constexpr (count == 5)
    {
        auto& [_0, _1, _2, _3, _4] = s;
        return save_fields(writer, _0, _1, _2, _3, _4);
    }

    if constexpr (count == 6)
    {
        auto& [_0, _1, _2, _3, _4, _5] = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5);
    }

    if constexpr (count == 7)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6] = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6);
    }

    if constexpr (count == 8)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7] = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7);
    }

    if constexpr (count == 9)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8] = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8);
    }

    if constexpr (count == 10)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9] = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9);
    }

    if constexpr (count == 11)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10] = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10);
    }

    if constexpr (count == 12)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11] = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11);
    }

    if constexpr (count == 13)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12] = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12);
    }

    if constexpr (count == 14)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13] = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13);
    }

    if constexpr (count == 15)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14] = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14);
    }

    if constexpr (count == 16)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15] = s;
        return save_fields(
            writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15);
    }

    if constexpr (count == 17)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16] = s;
        return save_fields(
            writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16);
    }

    if constexpr (count == 18)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17] = s;
        return save_fields(
            writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17);
    }

    if constexpr (count == 19)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18);
    }

    if constexpr (count == 20)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19);
    }

    if constexpr (count == 21)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20);
    }

    if constexpr (count == 22)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20, _21);
    }

    if constexpr (count == 23)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20, _21, _22);
    }

    if constexpr (count == 24)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20, _21, _22, _23);
    }

    if constexpr (count == 25)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20, _21, _22, _23, _24);
    }

    if constexpr (count == 26)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25);
    }

    if constexpr (count == 27)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25, _26]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26);
    }

    if constexpr (count == 28)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25, _26, _27]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27);
    }

    if constexpr (count == 29)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25, _26, _27, _28]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28);
    }

    if constexpr (count == 30)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29);
    }

    if constexpr (count == 31)
    {
        auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18,
            _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30]
            = s;
        return save_fields(writer, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14,
            _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30);
    }
}


}
