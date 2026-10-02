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
#include <cpp_utils/containers/no_init_vector.hpp>
#include <cstring>
#include <span>

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

template <CDF_Types type>
void to_byte_order(std::span<from_cdf_type_t<type>> values, cdf_encoding encoding)
{
    if constexpr (!is_cdf_string_type(type))
    {
        if (endianness::is_big_endian_encoding(encoding))
            endianness::decode_v<endianness::big_endian_t>(values.data(), std::size(values));
        else
            endianness::decode_v<endianness::little_endian_t>(values.data(), std::size(values));
    }
}

struct file_layout
{
    cdf_majority majority = cdf_majority::row;
    cdf_encoding encoding = CDFpp_ENCODING;

    [[nodiscard]] bool matches_memory() const
    {
        return majority == cdf_majority::row and has_host_byte_order(encoding);
    }

    // `count` records of `variable` from `first`, as the file stores them.
    [[nodiscard]] no_init_vector<char> records(const Variable& variable, std::size_t first,
        std::size_t count, std::size_t record_size) const
    {
        no_init_vector<char> bytes(count * record_size);
        std::memcpy(bytes.data(), variable.bytes_ptr() + first * record_size, std::size(bytes));
        cdf_type_dispatch(variable.type(),
            [&]<CDF_Types type>()
            {
                using value_t = from_cdf_type_t<type>;
                std::span<value_t> values { reinterpret_cast<value_t*>(bytes.data()),
                    std::size(bytes) / sizeof(value_t) };
                if (majority == cdf_majority::column)
                {
                    auto shape = variable.shape();
                    shape[0] = static_cast<uint32_t>(count);
                    majority::swap<is_cdf_string_type(type), Variable::shape_t, decltype(values),
                        true>(values, shape);
                }
                to_byte_order<type>(values, encoding);
            });
        return bytes;
    }

    // Attribute entries and pad values: majority doesn't apply to them.
    // A byte swap undoes itself, so decoding from the file's byte order also encodes to it.
    [[nodiscard]] data_t value(const data_t& value) const
    {
        return load_values<false>(data_t { value }, encoding);
    }
};

} // namespace cdf::io::saving
