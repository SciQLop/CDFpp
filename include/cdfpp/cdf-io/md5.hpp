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
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

// The checksum CDF files can end with. RFC 1321: https://www.rfc-editor.org/rfc/rfc1321
namespace cdf::io
{

class md5
{
public:
    static constexpr std::size_t digest_size = 16;
    using digest_t = std::array<char, digest_size>;

    void update(const char* data, std::size_t size)
    {
        p_length += size;
        if (p_buffered != 0)
        {
            const auto count = std::min(size, block_size - p_buffered);
            std::memcpy(p_buffer.data() + p_buffered, data, count);
            p_buffered += count;
            data += count;
            size -= count;
            if (p_buffered < block_size)
                return;
            transform(p_buffer.data());
            p_buffered = 0;
        }
        for (; size >= block_size; data += block_size, size -= block_size)
            transform(reinterpret_cast<const unsigned char*>(data));
        std::memcpy(p_buffer.data(), data, size);
        p_buffered = size;
    }

    [[nodiscard]] digest_t digest() const
    {
        md5 last = *this;
        last.pad();
        digest_t digest;
        for (std::size_t word = 0; word < 4; ++word)
            for (std::size_t byte = 0; byte < 4; ++byte)
                digest[word * 4 + byte] = static_cast<char>(last.p_state[word] >> (8 * byte));
        return digest;
    }

private:
    static constexpr std::size_t block_size = 64;

    static constexpr std::array<uint32_t, 64> sines = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
    };

    static constexpr std::array<int, 16> shifts
        = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };

    std::array<uint32_t, 4> p_state = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
    std::array<unsigned char, block_size> p_buffer {};
    std::size_t p_buffered = 0;
    uint64_t p_length = 0;

    // The four round functions, in forms equivalent to the RFC's that keep `b`, the value just
    // computed, as late as possible: it is the critical path. G is a bit select, so its two
    // halves have no bit in common and add up; the half without `b` is added early. See
    // https://github.com/animetosho/md5-optimisation
    template <std::size_t step>
    [[nodiscard]] static constexpr uint32_t mix_early(uint32_t c, uint32_t d) noexcept
    {
        if constexpr (step >= 16 && step < 32)
            return c & ~d;
        else
            return 0;
    }
    template <std::size_t step>
    [[nodiscard]] static constexpr uint32_t mix(uint32_t b, uint32_t c, uint32_t d) noexcept
    {
        if constexpr (step < 16)
            return d ^ (b & (c ^ d));
        else if constexpr (step < 32)
            return b & d;
        else if constexpr (step < 48)
            return b ^ (c ^ d);
        else
            return c ^ (b | ~d);
    }
    template <std::size_t step>
    static constexpr std::size_t word_of = step < 16 ? step
        : step < 32                                  ? (5 * step + 1) % 16
        : step < 48                                  ? (3 * step + 5) % 16
                                                     : (7 * step) % 16;

    template <std::size_t step>
    static void do_step(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d,
        const std::array<uint32_t, 16>& words) noexcept
    {
        const auto early = a + sines[step] + words[word_of<step>] + mix_early<step>(c, d);
        const auto rotated = std::rotl(early + mix<step>(b, c, d), shifts[(step / 16) * 4 + step % 4]);
        a = d;
        d = c;
        c = b;
        b += rotated;
    }

    // The 64 steps unrolled at compile time, so their constants are immediates.
    template <std::size_t... steps>
    void do_steps(const std::array<uint32_t, 16>& words, std::index_sequence<steps...>) noexcept
    {
        auto [a, b, c, d] = p_state;
        (do_step<steps>(a, b, c, d, words), ...);
        p_state[0] += a;
        p_state[1] += b;
        p_state[2] += c;
        p_state[3] += d;
    }

    void transform(const unsigned char* block)
    {
        std::array<uint32_t, 16> words;
        if constexpr (std::endian::native == std::endian::little)
            std::memcpy(words.data(), block, sizeof(words));
        else
            for (std::size_t i = 0; i < 16; ++i)
                words[i] = uint32_t { block[4 * i] } | uint32_t { block[4 * i + 1] } << 8
                    | uint32_t { block[4 * i + 2] } << 16 | uint32_t { block[4 * i + 3] } << 24;
        do_steps(words, std::make_index_sequence<64> {});
    }

    // A 1 bit, zeros up to 8 bytes before the end of a block, then the length in bits.
    void pad()
    {
        const uint64_t bits = p_length * 8;
        const auto one = static_cast<char>(0x80);
        update(&one, 1);
        const std::array<char, block_size> zeros {};
        update(zeros.data(), (block_size + block_size - 8 - p_buffered) % block_size);
        std::array<char, 8> length;
        for (std::size_t byte = 0; byte < 8; ++byte)
            length[byte] = static_cast<char>(bits >> (8 * byte));
        update(length.data(), std::size(length));
    }
};

}
