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

static_assert(cpp_utils::io::sequential_writer<file_writer>);

}
