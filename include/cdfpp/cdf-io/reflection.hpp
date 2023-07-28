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
#include <type_traits>
#include <utility>

/*
 * see https://stackoverflow.com/questions/39768517/structured-bindings-width/39779537#39779537
*/

namespace details
{
struct anything
{
    template <class T>
    operator T() const;
};


template <class T, class Is, class = void>
struct _can_construct_with_N : std::false_type
{
};

template <class T, std::size_t... Is>
struct _can_construct_with_N<T, std::index_sequence<Is...>,
    std::void_t<decltype(T { (void(Is), anything {})... })>> : std::true_type
{
};

template <class T, std::size_t N>
using can_construct_with_N = _can_construct_with_N<T, std::make_index_sequence<N>>;


template <std::size_t Min, std::size_t Range, template <std::size_t N> class target>
struct maximize
        : std::conditional_t<maximize<Min, Range / 2, target> {} == (Min + Range / 2) - 1,
              maximize<Min + Range / 2, (Range + 1) / 2, target>, maximize<Min, Range / 2, target>>
{
};
template <std::size_t Min, template <std::size_t N> class target>
struct maximize<Min, 1, target>
        : std::conditional_t<target<Min> {}, std::integral_constant<std::size_t, Min>,
              std::integral_constant<std::size_t, Min - 1>>
{
};
template <std::size_t Min, template <std::size_t N> class target>
struct maximize<Min, 0, target> : std::integral_constant<std::size_t, Min - 1>
{
};

template <class T>
struct construct_searcher
{
    template <std::size_t N>
    using result = can_construct_with_N<T, N>;
};

template <class T, std::size_t Cap = 20>
using construct_airity = details::maximize<0, Cap, details::construct_searcher<T>::template result>;

}

template <typename T>
inline constexpr std::size_t count_members = details::construct_airity<std::remove_cv_t<std::remove_reference_t<T>>>::value;
