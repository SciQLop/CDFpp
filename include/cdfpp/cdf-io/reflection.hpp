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

template <class T, std::size_t Cap = 32>
using construct_airity = details::maximize<0, Cap, details::construct_searcher<T>::template result>;

}

template <typename T>
inline constexpr std::size_t count_members
    = details::construct_airity<std::remove_cv_t<std::remove_reference_t<T>>>::value;

#define SPLIT_FIELDS_FW_DECL(return_type, name, const_struct)                                      \
    template <typename T, typename... Args>                                                        \
    return_type name(const_struct T& structure, Args&&... args);


#define SPLIT_FIELDS(return_type, name, function, const_struct)                                    \
    /* this looks quite ugly bit it is worth it!*/                                                 \
    template <typename T, typename... Args>                                                        \
    return_type name(const_struct T& structure, Args&&... args)                                    \
    {                                                                                              \
        constexpr std::size_t count = count_members<T>;                                            \
        static_assert(count <= 31);                                                                \
                                                                                                   \
        if constexpr (count == 1)                                                                  \
        {                                                                                          \
            auto& [_0] = structure;                                                                \
            return function(structure, args..., _0);                                               \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 2)                                                                  \
        {                                                                                          \
            auto& [_0, _1] = structure;                                                            \
            return function(structure, args..., _0, _1);                                           \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 3)                                                                  \
        {                                                                                          \
            auto& [_0, _1, _2] = structure;                                                        \
            return function(structure, args..., _0, _1, _2);                                       \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 4)                                                                  \
        {                                                                                          \
            auto& [_0, _1, _2, _3] = structure;                                                    \
            return function(structure, args..., _0, _1, _2, _3);                                   \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 5)                                                                  \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4] = structure;                                                \
            return function(structure, args..., _0, _1, _2, _3, _4);                               \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 6)                                                                  \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5] = structure;                                            \
            return function(structure, args..., _0, _1, _2, _3, _4, _5);                           \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 7)                                                                  \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6] = structure;                                        \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6);                       \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 8)                                                                  \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7] = structure;                                    \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7);                   \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 9)                                                                  \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8] = structure;                                \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8);               \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 10)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9] = structure;                            \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9);           \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 11)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10] = structure;                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10);      \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 12)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11] = structure;                  \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11); \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 13)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12] = structure;             \
            return function(                                                                       \
                structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12);        \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 14)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13] = structure;        \
            return function(                                                                       \
                structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13);   \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 15)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14] = structure;   \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14);                                                                    \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 16)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15]           \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15);                                                               \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 17)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16]      \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16);                                                          \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 18)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17] \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17);                                                     \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 19)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18]                                                                               \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18);                                                \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 20)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19]                                                                          \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19);                                           \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 21)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20]                                                                     \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20);                                      \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 22)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20, _21]                                                                \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20, _21);                                 \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 23)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20, _21, _22]                                                           \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22);                            \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 24)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20, _21, _22, _23]                                                      \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23);                       \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 25)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20, _21, _22, _23, _24]                                                 \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24);                  \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 26)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20, _21, _22, _23, _24, _25]                                            \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25);             \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 27)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20, _21, _22, _23, _24, _25, _26]                                       \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26);        \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 28)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20, _21, _22, _23, _24, _25, _26, _27]                                  \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27);   \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 29)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28]                             \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27,    \
                _28);                                                                              \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 30)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29]                        \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27,    \
                _28, _29);                                                                         \
        }                                                                                          \
                                                                                                   \
        if constexpr (count == 31)                                                                 \
        {                                                                                          \
            auto& [_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, \
                _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30]                   \
                = structure;                                                                       \
            return function(structure, args..., _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11,  \
                _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27,    \
                _28, _29, _30);                                                                    \
        }                                                                                          \
    }
