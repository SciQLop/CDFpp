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
#include <cpp_utils/io/range_reader.hpp>
#ifndef __EMSCRIPTEN__
#include <cpp_utils/io/remote_buffer.hpp>
#endif
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace cdf::io
{
// Set on threads that decode prefetched data: there, a missing byte is a bug to report, not
// something to fetch (a Python reader can't run on threads Python doesn't know).
inline thread_local bool no_fetch_thread = false;

// Tests only: drop the last block from the data prefetch, to prove decoding never fetches.
inline thread_local bool testing_skip_last_block_prefetch = false;

struct no_fetch_scope
{
    bool previous = std::exchange(no_fetch_thread, true);
    no_fetch_scope() = default;
    no_fetch_scope(const no_fetch_scope&) = delete;
    no_fetch_scope& operator=(const no_fetch_scope&) = delete;
    ~no_fetch_scope() { no_fetch_thread = previous; }
};
}

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
        if constexpr (requires(const buffer_t& b) { b.check_present(offset, size); })
            if (no_fetch_thread)
                p_buffer->check_present(offset, size);
        p_buffer->read(dest, offset, size);
    }

    auto view(std::size_t offset) const { return p_buffer->view(offset); }

    bool is_valid() const { return p_buffer->is_valid(); }

    // Remote buffers only: fetch bytes before they are read in place, or check they were.
    template <typename B = buffer_t>
        requires requires(const B& b, std::size_t n) { b.ensure(n, n); }
    void ensure(std::size_t offset, std::size_t size) const
    {
        if (no_fetch_thread)
            p_buffer->check_present(offset, size);
        else
            p_buffer->ensure(offset, size);
    }

    template <typename range_t, typename B = buffer_t>
        requires requires(const B& b, std::span<const range_t> ranges) { b.ensure(ranges); }
    void ensure(std::span<const range_t> ranges) const
    {
        if (!no_fetch_thread)
            return p_buffer->ensure(ranges);
        for (const auto& range : ranges)
            p_buffer->check_present(range.offset, range.size);
    }

    template <typename B = buffer_t>
        requires requires(const B& b, std::size_t n) { b.check_present(n, n); }
    void check_present(std::size_t offset, std::size_t size) const
    {
        p_buffer->check_present(offset, size);
    }

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

#ifndef __EMSCRIPTEN__
template <cpp_utils::io::range_reader reader_t>
auto make_shared_remote_adapter(
    reader_t reader, std::size_t size, cpp_utils::io::remote_buffer_options options)
{
    using remote_t = cpp_utils::io::remote_buffer<reader_t>;
    return shared_buffer_t<remote_t> { std::make_shared<remote_t>(
        std::move(reader), size, std::move(options)) };
}

template <cpp_utils::io::range_reader reader_t>
auto make_shared_remote_adapter(reader_t reader, cpp_utils::io::remote_buffer_options options)
{
    const auto size = reader.size();
    return make_shared_remote_adapter(std::move(reader), size, std::move(options));
}
#endif

}
