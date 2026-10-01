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
#include "cdfpp/cdf-file.hpp"
#include <span>

// Values live in memory in row major order and in the host byte order; a file may store them
// otherwise (CDF Internal Format Description, "Majority" and "Data Encoding").
namespace cdf::io::saving
{

[[nodiscard]] inline bool has_host_byte_order(cdf_encoding encoding)
{
    return endianness::is_big_endian_encoding(encoding)
        == endianness::is_big_endian_v<endianness::host_endianness_t>;
}

[[nodiscard]] inline bool matches_memory_layout(const CDF& cdf)
{
    return cdf.majority == cdf_majority::row and has_host_byte_order(cdf.encoding);
}

// A byte swap undoes itself, so decoding from the file's byte order also encodes to it.
[[nodiscard]] inline data_t in_byte_order(data_t data, cdf_encoding encoding)
{
    return load_values<false>(std::move(data), encoding);
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

inline void values_to_file_layout(Variable& variable, const CDF& cdf)
{
    if (variable.type() == CDF_Types::CDF_NONE or variable.bytes() == 0)
        return;
    cdf_type_dispatch(variable.type(),
        [&]<CDF_Types type>()
        {
            using value_t = from_cdf_type_t<type>;
            std::span<value_t> values { reinterpret_cast<value_t*>(variable.bytes_ptr()),
                variable.bytes() / sizeof(value_t) };
            if (cdf.majority == cdf_majority::column)
                majority::swap<is_cdf_string_type(type), Variable::shape_t, decltype(values), true>(
                    values, variable.shape());
            to_byte_order<type>(values, cdf.encoding);
        });
}

inline void to_file_layout(Variable& variable, const CDF& cdf)
{
    values_to_file_layout(variable, cdf);
    if (const auto& pad = variable.pad_value())
        variable.set_pad_value(in_byte_order(*pad, cdf.encoding));
    for (auto& [_, attribute] : variable.attributes)
        *attribute = in_byte_order(*attribute, cdf.encoding);
}

// simplify: converts a copy of the whole CDF, so saving it takes twice its memory; converting
// each block as it is written would avoid that, for files that don't match the memory layout.
[[nodiscard]] inline CDF in_file_layout(const CDF& cdf)
{
    CDF converted = cdf;
    for (auto& [_, attribute] : converted.attributes)
        for (auto& entry : attribute)
            entry = in_byte_order(std::move(entry), cdf.encoding);
    for (auto& [_, variable] : converted.variables)
        to_file_layout(variable, cdf);
    return converted;
}

} // namespace cdf::io::saving
