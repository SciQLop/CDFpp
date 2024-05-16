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
#include "../no_init_vector.hpp"
#include <cstddef>
#include <string>
#include <vector>

template <std::size_t _max_len>
struct string_field
{
    static inline constexpr std::size_t max_len = _max_len;
    std::string value;
};

template <typename T, typename = void>
struct is_string_field : std::false_type
{
};

template <typename T>
struct is_string_field<T, decltype(std::is_same_v<string_field<T::max_len>, T>, void())>
        : std::is_same<string_field<T::max_len>, T>
{
};

template <typename T>
static inline constexpr bool is_string_field_v
    = is_string_field<std::remove_cv_t<std::remove_reference_t<T>>>::value;


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


template <typename T, typename = void>
struct is_table_field : std::false_type
{
};

template <typename T>
struct is_table_field<T,
    decltype(std::is_same_v<table_field<typename T::value_type, T::index>, T>, void())>
        : std::is_same<table_field<typename T::value_type, T::index>, T>
{
};

template <typename T>
static inline constexpr bool is_table_field_v
    = is_table_field<std::remove_cv_t<std::remove_reference_t<T>>>::value;
