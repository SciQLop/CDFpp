/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2026, Laboratory of Plasma Physics- CNRS
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
#include "../endianness.hpp"
#include "../majority-swap.hpp"
#include "cdfpp/cdf-data.hpp"
#include "cdfpp/variable.hpp"
#include <cstring>

// Values live in memory in row major order and in the host byte order; a file may store them
// otherwise (CDF Internal Format Description, "Majority" and "Data Encoding"). Saving passes the
// values through file_layout on their way to the file, a block at a time.
namespace cdf::io::saving
{

[[nodiscard]] inline bool has_host_byte_order(cdf_encoding encoding)
{
    return endianness::is_big_endian_encoding(encoding)
        == endianness::is_big_endian_v<endianness::host_endianness_t>;
}

struct file_layout
{
    cdf_majority majority = cdf_majority::row;
    cdf_encoding encoding = CDFpp_ENCODING;

    [[nodiscard]] bool matches_memory() const
    {
        return majority == cdf_majority::row && has_host_byte_order(encoding);
    }

    // `count` records of `variable` from `first`, as the file stores them.
    [[nodiscard]] data_t records(const Variable& variable, std::size_t first, std::size_t count,
        std::size_t record_size) const
    {
        data_t records = new_data_container(count * record_size, variable.type());
        std::memcpy(
            records.bytes_ptr(), variable.bytes_ptr() + first * record_size, records.bytes());
        if (majority == cdf_majority::column)
        {
            auto shape = variable.shape();
            shape[0] = static_cast<uint32_t>(count);
            majority::to_column_major(records, shape);
        }
        return in_byte_order(std::move(records));
    }

    // Attribute entries and pad values: majority doesn't apply to them.
    [[nodiscard]] data_t value(const data_t& value) const
    {
        return in_byte_order(data_t { value });
    }

    // A byte swap undoes itself, so decoding from the file's byte order also encodes to it.
    [[nodiscard]] data_t in_byte_order(data_t values) const
    {
        return load_values<false>(std::move(values), encoding);
    }
};

} // namespace cdf::io::saving
