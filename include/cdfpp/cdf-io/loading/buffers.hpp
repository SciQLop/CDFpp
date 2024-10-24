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
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

#if __has_include(<sys/mman.h>)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define USE_MMAP
#endif
#if __has_include(<windows.h>)
#define NOMINMAX
#include <windows.h>
#define USE_MapViewOfFile
#endif

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

template <typename buffer_t>
inline constexpr auto get_data_ptr(buffer_t& buffer) -> decltype(get_data_ptr(buffer.buffer))
{
    return get_data_ptr(buffer.buffer);
}

template <typename buffer_t>
inline constexpr auto get_data_ptr(buffer_t& buffer) -> decltype(buffer.view(0UL))
{
    return buffer.view(0UL);
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
    [[nodiscard]] char& operator[](std::size_t index)
    {
        return *(p_buffer.get() + p_offset + index);
    }
    [[nodiscard]] char& operator[](std::size_t index) const
    {
        return *(p_buffer.get() + p_offset + index);
    }

    [[nodiscard]] char* data() { return p_buffer.get() + p_offset; }
    [[nodiscard]] char* begin() { return p_buffer.get() + p_offset; }
    [[nodiscard]] char* end() { return p_buffer.get() + p_offset + p_size; }
    [[nodiscard]] const char* cbegin() const { return p_buffer.get() + p_offset; }
    [[nodiscard]] const char* cend() const { return p_buffer.get() + p_offset + p_size; }
    [[nodiscard]] std::size_t size() const { return p_size; }
    [[nodiscard]] std::size_t offset() const { return p_offset; }
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

    auto view(const std::size_t offset) const { return this->data() + offset; }

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


struct mmap_adapter
{
#ifdef USE_MMAP
    int fd = -1;
#endif
    char* mapped_file = nullptr;
    std::size_t f_size = 0UL;
    using implements_view = std::true_type;
#ifdef USE_MapViewOfFile
    HANDLE hMapFile = NULL;
    HANDLE hFile = NULL;
#endif

    mmap_adapter(const std::string& path)
    {

        if (std::filesystem::exists(path))
        {
            this->f_size = std::filesystem::file_size(path);
            if (this->f_size)
            {
#ifdef USE_MMAP
                if (fd = open(path.c_str(), O_RDONLY, static_cast<mode_t>(0600)); fd != -1)
                {

                    {
                        mapped_file = static_cast<char*>(mmap(
                            nullptr, this->f_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0UL));
                    }
                }
#endif
#ifdef USE_MapViewOfFile
                hFile = ::CreateFile(path.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
                if (hFile != INVALID_HANDLE_VALUE)
                {
                    hMapFile = ::CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
                    if (hMapFile != NULL)
                    {
                        mapped_file = static_cast<char*>(
                            ::MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, this->f_size));
                    }
                }
#endif
            }
        }
    }
    ~mmap_adapter()
    {
        if (mapped_file)
        {
#ifdef USE_MMAP
            munmap(mapped_file, f_size);
            close(fd);
#endif
#ifdef USE_MapViewOfFile
            ::UnmapViewOfFile(mapped_file);
            ::CloseHandle(hFile);
            ::CloseHandle(hMapFile);
#endif
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

    auto view(const std::size_t offset) const { return mapped_file + offset; }

    bool is_valid() const
    {
#ifdef USE_MMAP
        return fd != -1 && mapped_file != nullptr;
#endif
#ifdef USE_MapViewOfFile
        return hFile != NULL && hMapFile != NULL && mapped_file != nullptr;
#endif
    }
};


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
    inline auto read(Types&&... args) -> decltype(std::declval<std::shared_ptr<buffer_t>>()->read(
                                          std::forward<Types>(args)...))
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

    auto view(const std::size_t offset) const -> decltype(std::declval<buffer_t>().view(offset))
    {
        return p_buffer->view(offset);
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
    return shared_buffer_t(std::make_shared<mmap_adapter>(path));
}

}
