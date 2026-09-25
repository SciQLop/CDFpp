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
#include "../cdf-debug.hpp"
#include "../cdf-enums.hpp"
#include <cpp_utils/endianness/endianness.hpp>

namespace cdf::endianness
{
using cpp_utils::endianness::big_endian_t;
using cpp_utils::endianness::host_endianness_t;
using cpp_utils::endianness::is_big_endian_v;
using cpp_utils::endianness::is_little_endian_v;
using cpp_utils::endianness::little_endian_t;

using cpp_utils::endianness::decode;
using cpp_utils::endianness::decode_v;

[[nodiscard]] constexpr bool is_big_endian_encoding(cdf_encoding encoding)
{
    return encoding == cdf_encoding::network || encoding == cdf_encoding::SUN
        || encoding == cdf_encoding::NeXT || encoding == cdf_encoding::PPC
        || encoding == cdf_encoding::SGi || encoding == cdf_encoding::IBMRS
        || encoding == cdf_encoding::ARM_BIG;
}

[[nodiscard]] inline bool is_little_endian_encoding(cdf_encoding encoding)
{
    return !is_big_endian_encoding(encoding);
}

// epoch16 packs two doubles (seconds, picoseconds fraction) into one record; there's no generic
// cpp_utils overload for it, so it's decoded here as a run of 2*size uint64_t words.
template <typename src_endianess_t>
CDFPP_NON_NULL(1)
inline void decode_v(epoch16* data, std::size_t size)
{
    decode_v<src_endianess_t>(reinterpret_cast<uint64_t*>(data), size * 2);
}
}
