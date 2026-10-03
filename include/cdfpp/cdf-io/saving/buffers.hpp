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

#include <cpp_utils/io/sequential_writer.hpp>
#include <filesystem>
#include <fstream>
#if defined(__APPLE__)
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <string>
#include <system_error>
#include <vector>

namespace cdf::io::buffers
{

using cpp_utils::io::vector_writer;

// Room for `count` more bytes at the end of a buffer, to fill in place.
template <typename Container>
[[nodiscard]] char* append(vector_writer<Container>& writer, std::size_t count)
{
    writer.data.resize(writer.global_offset + count);
    auto* room = reinterpret_cast<char*>(writer.data.data()) + writer.global_offset;
    writer.global_offset += count;
    return room;
}

// Writes over an existing file, then cuts what is left past the new end, rather than
// truncating it first: btrfs and ext4 flush a file truncated to zero when it is closed, which
// made saving over a file 2.5x slower (https://lkml.iu.edu/hypermail/linux/kernel/1409.0/02294.html).
#if !defined(__APPLE__)
struct file_writer
{
    std::filesystem::path path;
    std::fstream os;
    std::size_t global_offset = 0;

    explicit file_writer(const std::string& fname) : path { fname }
    {
        os.open(fname, std::ios::in | std::ios::out | std::ios::binary);
        if (!os.is_open())
            os.open(fname, std::ios::out | std::ios::binary);
    }

    [[nodiscard]] bool is_open() const noexcept { return os.is_open(); }

    std::size_t write(const char* const data_ptr, std::size_t count)
    {
        os.write(data_ptr, static_cast<std::streamsize>(count));
        global_offset += count;
        return global_offset;
    }

    std::size_t fill(const char v, std::size_t count)
    {
        std::vector<char> values(count, v);
        return write(values.data(), count);
    }

    [[nodiscard]] std::size_t offset() const noexcept { return global_offset; }

    // Closes the file and cuts the bytes an older, bigger file left; false if anything failed.
    [[nodiscard]] bool finish()
    {
        os.close();
        const bool written = !os.fail();
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec)
            && std::filesystem::file_size(path, ec) > global_offset && !ec)
            std::filesystem::resize_file(path, global_offset, ec);
        return written && !ec;
    }
};
#else
// macOS: straight write() calls, not std::fstream. Apple's libc++ passes big writes through its
// small stdio buffer: 27 MB took 28 ms instead of 8 on an M2. libstdc++ sends them to the kernel
// at once, so Linux keeps the std::fstream version above.
struct file_writer
{
    int fd = -1;
    bool failed = false;
    std::size_t global_offset = 0;

    explicit file_writer(const std::string& fname)
            : fd { ::open(fname.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0666) }
    {
    }

    file_writer(const file_writer&) = delete;
    file_writer& operator=(const file_writer&) = delete;

    ~file_writer()
    {
        if (fd != -1)
            ::close(fd);
    }

    [[nodiscard]] bool is_open() const noexcept { return fd != -1; }

    std::size_t write(const char* data_ptr, std::size_t count)
    {
        global_offset += count;
        while (count != 0 && !failed)
        {
            const auto written = ::write(fd, data_ptr, count);
            if (written < 0)
            {
                failed = errno != EINTR;
                continue;
            }
            data_ptr += written;
            count -= static_cast<std::size_t>(written);
        }
        return global_offset;
    }

    std::size_t fill(const char v, std::size_t count)
    {
        std::vector<char> values(count, v);
        return write(values.data(), count);
    }

    [[nodiscard]] std::size_t offset() const noexcept { return global_offset; }

    // Cuts the bytes an older, bigger file left and closes the file; false if anything failed.
    [[nodiscard]] bool finish()
    {
        if (!failed && ::lseek(fd, 0, SEEK_END) > static_cast<off_t>(global_offset))
            failed = ::ftruncate(fd, static_cast<off_t>(global_offset)) != 0;
        failed |= ::close(fd) != 0;
        fd = -1;
        return !failed;
    }
};
#endif

static_assert(cpp_utils::io::sequential_writer<file_writer>);

}
