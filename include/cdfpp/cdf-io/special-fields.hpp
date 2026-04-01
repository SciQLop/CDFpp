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
#include "../no_init_vector.hpp"
#include <concepts>
#include <cstddef>
#include <string>
#include <vector>

template <std::size_t _max_len>
struct string_field
{
    static inline constexpr std::size_t max_len = _max_len;
    std::string value;
};

template <typename T>
concept string_field_c = requires {
    { T::max_len } -> std::convertible_to<std::size_t>;
    requires std::same_as<T, string_field<T::max_len>>;
};

template <typename T>
static inline constexpr bool is_string_field_v
    = string_field_c<std::remove_cv_t<std::remove_reference_t<T>>>;


template <typename T, std::size_t _index = 0>
struct table_field
{
    using value_type = T;
    static constexpr std::size_t index = _index;
    no_init_vector<T> values;
};

template <typename T>
struct unused_field
{
    T value;
};

template <typename T>
concept table_field_c = requires {
    typename T::value_type;
    { T::index } -> std::convertible_to<std::size_t>;
    requires std::same_as<T, table_field<typename T::value_type, T::index>>;
};

template <typename T>
static inline constexpr bool is_table_field_v
    = table_field_c<std::remove_cv_t<std::remove_reference_t<T>>>;
