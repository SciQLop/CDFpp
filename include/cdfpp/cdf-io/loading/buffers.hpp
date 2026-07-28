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

#include <cpp_utils/containers/no_init_vector.hpp>
using cpp_utils::containers::no_init_vector;
#include <cpp_utils/io/buffer_view.hpp>
#include <cpp_utils/io/memory_mapped_file.hpp>
#include <cpp_utils/io/owned_buffer.hpp>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace cdf::io::buffers
{

using cpp_utils::io::buffer_view;
using cpp_utils::io::memory_mapped_file;
using cpp_utils::io::owned_buffer;
using cpp_utils::io::owned_buffer_t;

/** Adds shared-ownership (cheap-copy, stable-address) semantics on top of a
 * random_access_buffer, so a single mmap'd file / owned vector can be referenced
 * from a parsing_context_t and every blk_iterator/context derived from it. Also
 * forwards begin/end/data/size, so it's directly span-constructible — that's what
 * lets cpp_utils::serde::deserialize read from it without any further plumbing. */
template <typename buffer_t>
struct shared_buffer_t
{
    shared_buffer_t() = delete;

    shared_buffer_t(std::shared_ptr<buffer_t>&& buffer) : p_buffer { std::move(buffer) } { }

    shared_buffer_t(const shared_buffer_t& other) = default;
    shared_buffer_t(shared_buffer_t&& other) noexcept = default;
    shared_buffer_t& operator=(const shared_buffer_t&) = default;
    shared_buffer_t& operator=(shared_buffer_t&&) noexcept = default;

    auto begin() const { return p_buffer->begin(); }
    auto end() const { return p_buffer->end(); }
    auto data() const { return p_buffer->data(); }
    std::size_t size() const { return p_buffer->size(); }

    void read(char* dest, std::size_t offset, std::size_t size) const
    {
        p_buffer->read(dest, offset, size);
    }

    auto view(std::size_t offset) const { return p_buffer->view(offset); }

    bool is_valid() const { return p_buffer->is_valid(); }

private:
    std::shared_ptr<buffer_t> p_buffer;
};

inline auto make_shared_array_adapter(no_init_vector<char>&& array)
{
    return shared_buffer_t<owned_buffer> { std::make_shared<owned_buffer>(std::move(array)) };
}

// A caller-supplied std::vector<char> rvalue must be genuinely owned, not viewed — the
// caller's vector is typically a temporary that dies at the end of this call. Wrapping
// std::vector<char> directly (rather than converting to no_init_vector<char> first)
// keeps this a real move: no_init_vector uses a different allocator, so there is no
// implicit conversion between the two, only an O(n) copy.
inline auto make_shared_array_adapter(std::vector<char>&& array)
{
    return shared_buffer_t<owned_buffer_t<std::vector<char>>> {
        std::make_shared<owned_buffer_t<std::vector<char>>>(std::move(array))
    };
}

inline auto make_shared_array_adapter(const std::vector<char>& array)
{
    return shared_buffer_t<buffer_view> { std::make_shared<buffer_view>(
        std::span<const char> { array.data(), array.size() }) };
}

inline auto make_shared_array_adapter(const char* const data, std::size_t size)
{
    return shared_buffer_t<buffer_view> { std::make_shared<buffer_view>(
        std::span<const char> { data, size }) };
}

inline auto make_shared_file_adapter(const std::string& path)
{
    return shared_buffer_t<memory_mapped_file> { std::make_shared<memory_mapped_file>(path) };
}

}
