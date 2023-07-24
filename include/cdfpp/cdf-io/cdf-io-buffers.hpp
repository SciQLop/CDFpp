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
#include "cdfpp/attribute.hpp"
#include "cdfpp/cdf-data.hpp"
#include "cdfpp/cdf-enums.hpp"
#include <cassert>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

namespace cdf::io::buffers
{

template <typename buffer_t>
inline constexpr auto get_data_ptr(buffer_t& buffer) -> decltype(buffer.data())
{
    return buffer.data();
}

template <typename buffer_t>
inline constexpr auto get_data_ptr(
    buffer_t& buffer, typename std::enable_if<std::is_pointer_v<buffer_t>>::type* = 0)
{
    return buffer;
}

template <typename stream_t>
std::size_t filesize(stream_t& file)
{
    if (file.is_open())
    {
        file.seekg(0, file.beg);
        auto pos = file.tellg();
        file.seekg(0, file.end);
        pos = file.tellg() - pos;
        file.seekg(0, file.beg);
        return static_cast<std::size_t>(pos);
    }
    return 0;
}

struct array_view
{
    using value_type = char;
    std::shared_ptr<char> p_buffer;
    std::size_t p_size;
    std::size_t p_offset;
    array_view(const std::shared_ptr<char>& buffer, std::size_t size, std::size_t offset = 0UL)
            : p_buffer { buffer }, p_size { size }, p_offset { offset }
    {
    }
    char& operator[](std::size_t index) { return *(p_buffer.get() + p_offset + index); }
    char& operator[](std::size_t index) const { return *(p_buffer.get() + p_offset + index); }

    char* data() { return p_buffer.get() + p_offset; }
    char* begin() { return p_buffer.get() + p_offset; }
    char* end() { return p_buffer.get() + p_offset + p_size; }
    const char* cbegin() const { return p_buffer.get() + p_offset; }
    const char* cend() const { return p_buffer.get() + p_offset + p_size; }
    std::size_t size() const { return p_size; }
    std::size_t offset() const { return p_offset; }
};

struct buffer
{
    std::shared_ptr<char> p_data;
    std::size_t p_size;
    std::size_t p_offset;
    buffer(char* data, std::size_t size, std::size_t offset = 0UL)
            : p_data { data, std::default_delete<char[]>() }, p_size { size }, p_offset { offset }
    {
    }
    array_view view(std::size_t offset, std::size_t size)
    {
        return array_view { p_data, size, offset - p_offset };
    }
    std::size_t size() const { return p_size; }
    std::size_t offset() const { return p_offset; }
};

bool contains(const buffer& b, std::size_t offset, std::size_t size)
{
    auto starts_inside = (offset >= b.offset()) && (offset < (b.offset() + b.size()));
    auto finishes_inside = ((offset + size) <= (b.offset() + b.size()));
    return starts_inside && finishes_inside;
}

template <typename stream_t>
struct stream_adapter
{
    std::optional<buffer> p_buffer;
    stream_t stream;
    std::size_t p_fsize;
    using implements_view = std::false_type;

    stream_adapter(stream_t&& stream) : stream { std::move(stream) }
    {
        p_fsize = filesize(this->stream);
    }

    void _fill_buffer(const std::size_t offset, const std::size_t size)
    {
        assert((offset + size) <= p_fsize);
        stream.seekg(offset);
        auto read_size = std::min(size, p_fsize - offset);
        char* data = new char[read_size];
        stream.read(data, read_size);
        p_buffer = buffer { data, read_size, offset };
    }

    template <typename T>
    auto impl_read(T& array, const std::size_t offset, const std::size_t size)
        -> decltype(array.data(), void())
    {
        stream.seekg(offset);
        stream.read(array.data(), size);
    }

    template <typename T>
    auto impl_read(T& array, const std::size_t offset, const std::size_t size)
        -> decltype(*array, void())
    {
        stream.seekg(offset);
        stream.read(array, size);
    }

    auto read(const std::size_t offset, const std::size_t size)
    {
        if (!p_buffer || !contains(*p_buffer, offset, size))
            _fill_buffer(offset, size);
        return p_buffer->view(offset, size);
    }

    template <std::size_t size>
    auto read(const std::size_t offset)
    {
        if (!p_buffer || !contains(*p_buffer, offset, size))
            _fill_buffer(offset, size);
        return p_buffer->view(offset, size);
    }

    void read(char* dest, const std::size_t offset, const std::size_t size)
    {
        if (!p_buffer || !contains(*p_buffer, offset, size))
            _fill_buffer(offset, size);
        auto view = p_buffer->view(offset, size);
        std::memcpy(dest, view.data(), size);
    }
    bool is_valid() { return stream.is_open(); }
};

namespace
{
    template <typename T>
    auto begin(T&& collection)
    {
        if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
            return std::forward<T>(collection);
        else
            return std::begin(std::forward<T>(collection));
    }

    template <typename T>
    auto end(T&& collection)
    {
        if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
            return std::forward<T>(collection);
        else
            return std::end(std::forward<T>(collection));
    }
}

template <typename array_t>
struct with_ownership
{
    array_t array;
    with_ownership(array_t&& array) : array { std::move(array) } { }
};
template <typename array_t>
struct without_ownership
{
    std::conditional_t<std::is_pointer_v<array_t>, const array_t, const array_t&> array;
    without_ownership(const array_t& array) : array { array } { }
};

template <typename array_t, bool takes_ownership = false>
struct array_adapter
        : std::conditional_t<takes_ownership, with_ownership<array_t>, without_ownership<array_t>>
{
    const std::size_t size;
    using implements_view = std::true_type;

    template <bool owns = takes_ownership>
    array_adapter(const array_t& array, typename std::enable_if<!owns>::type* = 0)
            : without_ownership<array_t> { array }, size { std::size(array) }
    {
    }
    template <bool owns = takes_ownership>
    array_adapter(const array_t& array, std::size_t size, typename std::enable_if<!owns>::type* = 0)
            : without_ownership<array_t> { array }, size { size }
    {
    }

    template <bool owns = takes_ownership>
    array_adapter(array_t&& array,
        typename std::enable_if<owns or std::is_rvalue_reference_v<array_t>>::type* = 0)
            : with_ownership<array_t> { std::move(array) }, size { std::size(this->array) }
    {
    }

    template <typename T>
    void impl_read(T& output_array, const std::size_t offset, const std::size_t size) const
    {
        std::memcpy(get_data_ptr(output_array), get_data_ptr(this->array) + offset, size);
    }

    auto read(const std::size_t offset, const std::size_t) const { return this->data() + offset; }

    template <std::size_t size>
    std::array<char, size> read(const std::size_t offset) const
    {
        std::array<char, size> buffer;
        impl_read(buffer, offset, size);
        return buffer;
    }

    template <std::size_t size>
    auto view(const std::size_t offset) const
    {
        return this->data() + offset;
    }

    void read(char* dest, std::size_t offset, std::size_t size) const
    {
        impl_read(dest, offset, size);
    }

    template <bool is_ptr = std::is_pointer_v<array_t>>
    const char* data(typename std::enable_if<is_ptr>::type* = 0) const
    {
        return this->array;
    }

    template <bool is_ptr = std::is_pointer_v<array_t>>
    const char* data(typename std::enable_if<!is_ptr>::type* = 0) const
    {
        return this->array.data();
    }

    bool is_valid() const { return size != 0; }
};

template <typename array_t>
using owning_array_adapter = array_adapter<array_t, true>;

#if __has_include(<sys/mman.h>)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
struct mmap_adapter
{
    int fd = -1;
    char* mapped_file = nullptr;
    std::size_t f_size = 0UL;
    using implements_view = std::true_type;

    mmap_adapter(const std::string& path)
    {
        if (fd = open(path.c_str(), O_RDONLY, static_cast<mode_t>(0600)); fd != -1)
        {
            struct stat fileInfo;
            if (fstat(fd, &fileInfo) != -1 && fileInfo.st_size != 0)
            {
                mapped_file = static_cast<char*>(
                    mmap(nullptr, fileInfo.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0UL));
                this->f_size = fileInfo.st_size;
            }
        }
    }
    ~mmap_adapter()
    {
        if (mapped_file)
        {
            munmap(mapped_file, f_size);
            close(fd);
        }
    }

    auto read(const std::size_t offset, const std::size_t) { return mapped_file + offset; }

    template <std::size_t size>
    auto read(const std::size_t offset)
    {
        return mapped_file + offset;
    }

    void read(char* dest, const std::size_t offset, const std::size_t size)
    {
        std::memcpy(dest, mapped_file + offset, size);
    }

    template <std::size_t size>
    auto view(const std::size_t offset) const
    {
        return mapped_file + offset;
    }

    bool is_valid() const { return fd != -1 && mapped_file != nullptr; }
};
#endif

template <class buffer_t>
struct shared_buffer_t
{
    shared_buffer_t() = delete;
    using implements_view = typename buffer_t::implements_view;

    shared_buffer_t(std::shared_ptr<buffer_t>&& buffer) : p_buffer { std::move(buffer) } { }

    shared_buffer_t(const shared_buffer_t& other) { p_buffer = other.p_buffer; }
    shared_buffer_t(shared_buffer_t&& other) { p_buffer = std::move(other.p_buffer); }
    shared_buffer_t& operator=(const shared_buffer_t&) = default;
    shared_buffer_t& operator=(shared_buffer_t&&) = default;

    template <class... Types>
    inline auto read(Types&&... args)
        -> decltype(std::declval<std::shared_ptr<buffer_t>>()->read(std::forward<Types>(args)...))
    {
        return p_buffer->read(std::forward<Types>(args)...);
    }

    template <class... Types>
    inline auto read(Types&&... args) const
        -> decltype(std::declval<std::shared_ptr<buffer_t>>()->read(std::forward<Types>(args)...))
    {
        return p_buffer->read(std::forward<Types>(args)...);
    }

    template <std::size_t size>
    inline auto read(const std::size_t offset)
    {
        return p_buffer->template read<size>(offset);
    }

    template <std::size_t size>
    inline auto read(const std::size_t offset) const
    {
        return p_buffer->template read<size>(offset);
    }

    template <std::size_t size>
    auto view(const std::size_t offset) const
        -> decltype(std::declval<buffer_t>().template view<size>(offset))
    {
        return p_buffer->template view<size>(offset);
    }

    inline bool is_valid() const { return p_buffer->is_valid(); }

private:
    std::shared_ptr<buffer_t> p_buffer;
};

template <typename array_t>
inline auto make_shared_array_adapter(array_t&& array)
{
    if constexpr (std::is_rvalue_reference_v<decltype(std::forward<array_t>(array))>)
        return shared_buffer_t(
            std::make_shared<array_adapter<array_t, true>>(std::forward<array_t>(array)));
    else
        return shared_buffer_t(
            std::make_shared<array_adapter<array_t>>(std::forward<array_t>(array)));
}


inline auto make_shared_array_adapter(const char* const data, std::size_t size)
{
    return shared_buffer_t(std::make_shared<array_adapter<const char* const>>(data, size));
}

inline auto make_shared_file_adapter(const std::string& path)
{
#if __has_include(<sys/mman.h>)
    return shared_buffer_t(std::make_shared<mmap_adapter>(path));
#else
    return shared_buffer_t(std::make_shared<stream_adapter<std::fstream>>(
        std::fstream { path, std::ios::in | std::ios::binary }));
#endif
}

}
