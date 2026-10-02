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
#include "../cdf-data.hpp"
#include "../cdf-debug.hpp"
#include <cpp_utils/containers/no_init_vector.hpp>
using cpp_utils::containers::no_init_vector;
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <variant>
#include <vector>

namespace cdf::majority
{

namespace _private
{

    template <bool backward, std::size_t N, std::size_t sz, typename T, typename U>
    [[nodiscard]] std::size_t _flat_index(const T& index, const U& shape)
    {
        constexpr auto n = backward ? sz - N - 1 : N;
        if constexpr (N != sz - 1)
        {
            return static_cast<std::size_t>(index[n])
                + static_cast<std::size_t>(shape[n])
                * _flat_index<backward, N + 1, sz>(index, shape);
        }
        else
            return static_cast<std::size_t>(index[n]);
    }

    template <bool backward, typename T, typename U>
    [[nodiscard]] std::size_t flat_index(const T& index, const U& shape)
    {
        switch (std::size(index))
        {
            case 2:
                return _private::_flat_index<backward, 0, 2>(index, shape);
            case 3:
                return _private::_flat_index<backward, 0, 3>(index, shape);
            case 4:
                return _private::_flat_index<backward, 0, 4>(index, shape);
            case 5:
                return _private::_flat_index<backward, 0, 5>(index, shape);
            case 6:
                return _private::_flat_index<backward, 0, 6>(index, shape);
            case 7:
                return _private::_flat_index<backward, 0, 7>(index, shape);
            case 8:
                return _private::_flat_index<backward, 0, 8>(index, shape);
            case 9:
                return _private::_flat_index<backward, 0, 9>(index, shape);
            case 10:
                return _private::_flat_index<backward, 0, 10>(index, shape);
            default:
                break;
        }
        return 0UL;
    }
}

template <typename T, typename U>
[[nodiscard]] auto flat_index(const T& index, const U& shape)
{
    return _private::flat_index<false, T, U>(index, shape);
}

template <typename T, typename U>
[[nodiscard]] auto inverted_flat_index(const T& index, const U& shape)
{
    return _private::flat_index<true, T, U>(index, shape);
}

namespace _private
{
    // How to walk a record so its values come out in the other majority: `extents` from the
    // fastest varying dimension of the output, `strides` how far each one steps in the input.
    struct gather_pattern
    {
        std::vector<std::size_t> extents;
        std::vector<std::size_t> strides;
    };

    // A string element, shape.back() characters, stays together as the fastest dimension.
    template <bool is_string, bool to_column, typename shape_t>
    [[nodiscard]] gather_pattern make_gather_pattern(const shape_t& shape)
    {
        const std::size_t element = is_string ? shape.back() : 1;
        const std::vector<std::size_t> dims(
            std::cbegin(shape) + 1, std::cend(shape) - (is_string ? 1 : 0));
        std::vector<std::size_t> row_strides(std::size(dims)), column_strides(std::size(dims));
        for (std::size_t d = std::size(dims), stride = element; d-- > 0; stride *= dims[d])
            row_strides[d] = stride;
        for (std::size_t d = 0, stride = element; d < std::size(dims); stride *= dims[d++])
            column_strides[d] = stride;
        gather_pattern pattern;
        if (is_string)
            pattern = { { element }, { 1 } };
        for (std::size_t i = 0; i < std::size(dims); i++)
        {
            const auto d = to_column ? i : std::size(dims) - 1 - i;
            pattern.extents.push_back(dims[d]);
            pattern.strides.push_back(to_column ? row_strides[d] : column_strides[d]);
        }
        return pattern;
    }

    // Walks the levels of `pattern` other than `skipped_a` and `skipped_b`, calling
    // `f(from_offset, to_offset)` for each position; `to` is written in order.
    template <typename F>
    void for_each_outer(const gather_pattern& pattern, const std::vector<std::size_t>& to_strides,
        std::size_t skipped_a, std::size_t skipped_b, F&& f)
    {
        struct level
        {
            std::size_t extent, from_stride, to_stride;
        };
        std::vector<level> levels;
        for (std::size_t l = 0; l < std::size(pattern.extents); l++)
            if (l != skipped_a && l != skipped_b)
                levels.push_back({ pattern.extents[l], pattern.strides[l], to_strides[l] });
        std::vector<std::size_t> index(std::size(levels));
        std::size_t from_offset = 0, to_offset = 0;
        while (true)
        {
            f(from_offset, to_offset);
            std::size_t l = 0;
            for (; l < std::size(levels); l++)
            {
                from_offset += levels[l].from_stride;
                to_offset += levels[l].to_stride;
                if (++index[l] < levels[l].extent)
                    break;
                from_offset -= levels[l].extent * levels[l].from_stride;
                to_offset -= levels[l].extent * levels[l].to_stride;
                index[l] = 0;
            }
            if (l == std::size(levels))
                return;
        }
    }

    // A fixed size lets the compiler transpose in registers: 4 loads, 8 shuffles and 4 stores
    // for 4x4 floats, even with SSE2 only.
    template <std::size_t N, typename T>
    void transpose_block(const T* from, std::size_t from_stride, T* to, std::size_t to_stride)
    {
        T block[N][N];
        for (std::size_t r = 0; r < N; r++)
            for (std::size_t c = 0; c < N; c++)
                block[c][r] = from[r * from_stride + c];
        for (std::size_t c = 0; c < N; c++)
            for (std::size_t r = 0; r < N; r++)
                to[c * to_stride + r] = block[c][r];
    }

    // Moves tiles between the fastest output level and the level contiguous in the input, so
    // both sides stay within a few cache lines: a strided store per value filled the store
    // queue. Narrow tiles (32 bytes) keep the fewest lines waiting for stores.
    template <typename T>
    void gather(const T* from, T* to, const gather_pattern& pattern)
    {
        const auto& [extents, strides] = pattern;
        std::vector<std::size_t> to_strides(std::size(extents));
        for (std::size_t l = 0, stride = 1; l < std::size(extents); stride *= extents[l++])
            to_strides[l] = stride;
        const auto contiguous
            = static_cast<std::size_t>(std::find(std::cbegin(strides) + 1, std::cend(strides), 1)
                - std::cbegin(strides));
        if (contiguous == std::size(strides)) // a string: its characters are already together
        {
            for_each_outer(pattern, to_strides, 0, 0,
                [&](std::size_t from_offset, std::size_t to_offset)
                { std::copy_n(from + from_offset, extents[0], to + to_offset); });
            return;
        }
        constexpr std::size_t block = std::max(std::size_t { 1 }, 16 / sizeof(T));
        constexpr std::size_t tile = std::max(block, 32 / sizeof(T));
        const std::size_t fast_extent = extents[0], fast_stride = strides[0];
        const std::size_t slow_extent = extents[contiguous], slow_stride = to_strides[contiguous];
        const bool in_blocks = fast_extent % block == 0 && slow_extent % block == 0;
        for_each_outer(pattern, to_strides, 0, contiguous,
            [&](std::size_t from_offset, std::size_t to_offset)
            {
                for (std::size_t first = 0; first < slow_extent; first += tile)
                {
                    const auto last = std::min(first + tile, slow_extent);
                    if (in_blocks)
                        for (std::size_t i = 0; i < fast_extent; i += block)
                            for (std::size_t k = first; k < last; k += block)
                                transpose_block<block>(from + from_offset + i * fast_stride + k,
                                    fast_stride, to + to_offset + i + k * slow_stride,
                                    slow_stride);
                    else
                        for (std::size_t i = 0; i < fast_extent; i++)
                            for (std::size_t k = first; k < last; k++)
                                to[to_offset + i + k * slow_stride]
                                    = from[from_offset + i * fast_stride + k];
                }
            });
    }
}

// Records with less than 2 dimensions read the same in both majorities. A string is an
// element of shape.back() characters.
template <bool is_string, typename shape_t>
[[nodiscard]] bool majorities_differ(const shape_t& shape)
{
    return std::size(shape) > (is_string ? 3UL : 2UL);
}

// From column to row major by default, from row to column major with to_column.
template <bool is_string, typename shape_t, typename data_t, bool to_column = false>
void swap(data_t& data, const shape_t& shape)
{
    if (majorities_differ<is_string>(shape))
    {
        const std::size_t records_count = shape[0];
        const auto pattern = _private::make_gather_pattern<is_string, to_column>(shape);
        const auto values_per_record = std::accumulate(std::cbegin(pattern.extents),
            std::cend(pattern.extents), std::size_t { 1 }, std::multiplies<std::size_t>());
        no_init_vector<typename data_t::value_type> record(values_per_record);
        for (std::size_t offset = 0; offset < records_count * values_per_record;
             offset += values_per_record)
        {
            std::copy_n(data.data() + offset, values_per_record, record.data());
            _private::gather(record.data(), data.data() + offset, pattern);
        }
    }
}

template <bool to_column = false>
inline void swap(data_t& data, const no_init_vector<uint32_t>& shape)
{
    if (data.type() == CDF_Types::CDF_NONE)
        return;
    cdf_type_dispatch(data.type(),
        [&]<CDF_Types t>()
        {
            constexpr bool is_str = is_cdf_string_type(t);
            auto& values = data.get<t>();
            swap<is_str, no_init_vector<uint32_t>, std::decay_t<decltype(values)>, to_column>(
                values, shape);
        });
}

inline void to_column_major(data_t& data, const no_init_vector<uint32_t>& shape)
{
    swap<true>(data, shape);
}

// The records of `shape`, row major at `from`, copied to `to` in column major order: no
// temporary record, unlike swapping in place.
inline void copy_to_column_major(
    const char* from, data_t& to, const no_init_vector<uint32_t>& shape)
{
    if (to.type() == CDF_Types::CDF_NONE)
        return;
    cdf_type_dispatch(to.type(),
        [&]<CDF_Types t>()
        {
            constexpr bool is_str = is_cdf_string_type(t);
            using value_t = from_cdf_type_t<t>;
            const auto* values = reinterpret_cast<const value_t*>(from);
            auto* out = reinterpret_cast<value_t*>(to.bytes_ptr());
            const std::size_t count = std::accumulate(std::cbegin(shape), std::cend(shape),
                std::size_t { 1 }, std::multiplies<std::size_t>());
            if (!majorities_differ<is_str>(shape))
                return void(std::copy_n(values, count, out));
            const auto pattern = _private::make_gather_pattern<is_str, true>(shape);
            const auto per_record = count / shape[0];
            for (std::size_t offset = 0; offset < count; offset += per_record)
                _private::gather(values + offset, out + offset, pattern);
        });
}
}
