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
#include <memory>
#include <string.h>
#include <vector>
#if __has_include(<sys/mman.h>)
#include <stdlib.h>
#include <sys/mman.h>
#endif

/*
 * taken from:
 *  https://stackoverflow.com/questions/21028299/is-this-behavior-of-vectorresizesize-type-n-under-c11-and-boost-container/21028912#21028912
 *  and
 *  https://stackoverflow.com/questions/2340311/posix-memalign-for-stdvector
 */

template <typename T, typename A = std::allocator<T>>
class default_init_allocator : public A
{
    typedef std::allocator_traits<A> a_t;
    static inline constexpr std::size_t page_size = 1 << 21;
    static inline constexpr bool is_trivial_and_nothrow_default_constructible
        = std::is_trivially_constructible_v<T> and std::is_nothrow_default_constructible_v<T>;

public:
    template <typename U>
    struct rebind
    {
        using other = default_init_allocator<U, typename a_t::template rebind_alloc<U>>;
    };

    using A::A;

    template <typename U>
    void construct(U* ptr) noexcept(std::is_nothrow_default_constructible_v<U>)
    {
        ::new (static_cast<void*>(ptr)) U;
    }
    template <typename U, typename... Args>
    void construct(U* ptr, Args&&... args)
    {
        a_t::construct(static_cast<A&>(*this), ptr, std::forward<Args>(args)...);
    }

#if __has_include(<sys/mman.h>)
    template <bool U = is_trivial_and_nothrow_default_constructible>
    T* allocate(std::size_t pCount, const T* = 0, typename std::enable_if<U>::type* = 0)
    {
        void* mem = 0;
        auto bytes = sizeof(T) * pCount;
        if (bytes >= 2 * page_size)
        {
            if (::posix_memalign(&mem, page_size, sizeof(T) * pCount) != 0)
            {
                throw std::bad_alloc(); // or something
            }
#ifdef MADV_HUGEPAGE
            ::madvise(mem, pCount * sizeof(T), MADV_HUGEPAGE);
#endif
        }
        else
        {
            mem = ::malloc(bytes);
        }
        //::memset(reinterpret_cast<char*>(mem),0x9e,bytes);
        return reinterpret_cast<T*>(mem);
    }

    template <bool U = is_trivial_and_nothrow_default_constructible>
    T* allocate(std::size_t pCount, const T* ptr = 0, typename std::enable_if<!U>::type* = 0)
    {
        return A::allocate(pCount, ptr);
    }

    template <bool U = is_trivial_and_nothrow_default_constructible>
    void deallocate(T* ptr, std::size_t, typename std::enable_if<U>::type* = 0) noexcept(
        std::is_nothrow_default_constructible<T>::value)
    {
        ::free(ptr);
    }

    template <bool U = is_trivial_and_nothrow_default_constructible>
    void deallocate(T* ptr, std::size_t sz, typename std::enable_if<!U>::type* = 0) noexcept(
        std::is_nothrow_default_constructible<T>::value)
    {
        A::deallocate(ptr, sz);
    }
#endif
};

template <typename T>
using no_init_vector = std::vector<T, default_init_allocator<T>>;

template <typename T>
[[nodiscard]] bool operator==(
    const std::vector<T, default_init_allocator<T>>& v1, const std::vector<T>& v2) noexcept
{
    const auto s1 = std::size(v1);
    if (s1 == std::size(v2))
    {
        for (auto i = 0UL; i < s1; i++)
        {
            if (v1[i] != v2[i])
                return false;
        }
        return true;
    }
    return false;
}

template <typename T>
[[nodiscard]] bool operator==(
    const std::vector<T>& v2, const std::vector<T, default_init_allocator<T>>& v1) noexcept
{
    return v1 == v2;
}

namespace std
{
template <typename T>
[[nodiscard]] size_t size(const no_init_vector<T>& v) noexcept
{
    return v.size();
}
}
