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
#include <algorithm>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace cdf
{

namespace helpers
{
    template <typename... Ts>
    struct Visitor : Ts...
    {
        using Ts::operator()...;
    };

    consteval bool is_in(const auto& value, const auto&... values)
    {
        return ((value == values) || ...);
    }

    constexpr bool rt_is_in(const auto& value, const auto&... values)
    {
        return ((value == values) || ...);
    }

    template <typename ref_type, typename... types>
    struct is_any_of : std::integral_constant<bool,(std::is_same_v<ref_type, types> || ...)>{};

    template <typename ref_type, typename... types>
    using is_any_of_t = typename is_any_of<ref_type,types...>::type;

    template <typename ref_type, typename... types>
    inline constexpr bool is_any_of_v = is_any_of<ref_type,types...>::value;

    constexpr bool contains(const std::string& str, const auto& substr)
    {
        return str.find(substr) != std::string::npos;
    }
}
}
