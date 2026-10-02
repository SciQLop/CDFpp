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

    [[nodiscard]] bool matches_memory(const Variable& variable) const
    {
        return has_host_byte_order(encoding) && !transposes(variable);
    }

    // `count` records of `variable` from `first`, as the file stores them.
    [[nodiscard]] data_t records(const Variable& variable, std::size_t first, std::size_t count,
        std::size_t record_size) const
    {
        data_t records = new_data_container(count * record_size, variable.type());
        records_into(records, variable, first, count, record_size);
        return records;
    }

    // Same, into `out`, which holds at least `count` records: saving reuses one buffer.
    // Swapped or transposed while copied, so the values are read once.
    void records_into(data_t& out, const Variable& variable, std::size_t first, std::size_t count,
        std::size_t record_size) const
    {
        const auto* from = variable.bytes_ptr() + first * record_size;
        const auto bytes = count * record_size;
        const auto value_size = cdf_type_size(variable.type());
        if (!transposes(variable))
            return copy_in_byte_order(from, out.bytes_ptr(), bytes, value_size);
        auto shape = variable.shape();
        shape[0] = static_cast<uint32_t>(count);
        majority::copy_to_column_major(from, out, shape);
        if (!has_host_byte_order(encoding))
            copy_in_byte_order(out.bytes_ptr(), out.bytes_ptr(), bytes, value_size);
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

private:
    // Records with less than 2 dimensions read the same in both majorities (see majority::swap).
    [[nodiscard]] bool transposes(const Variable& variable) const
    {
        const bool is_string = variable.type() == CDF_Types::CDF_CHAR
            || variable.type() == CDF_Types::CDF_UCHAR;
        return majority == cdf_majority::column
            && std::size(variable.shape()) > (is_string ? 3UL : 2UL);
    }

    void copy_in_byte_order(
        const char* from, char* to, std::size_t bytes, std::size_t value_size) const
    {
        if (has_host_byte_order(encoding) || value_size == 1)
        {
            if (from != to)
                std::memcpy(to, from, bytes);
        }
        else if (value_size == 2)
            copy_swapped<uint16_t>(from, to, bytes);
        else if (value_size == 4)
            copy_swapped<uint32_t>(from, to, bytes);
        else // 8 bytes, or an epoch16: two doubles
            copy_swapped<uint64_t>(from, to, bytes);
    }

    template <typename word_t>
    static void copy_swapped(const char* from, char* to, std::size_t bytes)
    {
        using other_endianness_t = std::conditional_t<
            endianness::is_little_endian_v<endianness::host_endianness_t>,
            endianness::big_endian_t, endianness::little_endian_t>;
        endianness::decode_v<other_endianness_t>(reinterpret_cast<const word_t*>(from),
            bytes / sizeof(word_t), reinterpret_cast<word_t*>(to));
    }
};

} // namespace cdf::io::saving
