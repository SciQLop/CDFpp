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

    struct index_swap_pair
    {
        std::size_t src;
        std::size_t dest;
    };
    inline void next_index(std::vector<std::size_t>& nd_index, const std::vector<std::size_t>& shape)
    {
        for (auto dim = 0UL; dim < std::size(shape); dim++)
        {
            nd_index[dim]++;
            if (nd_index[dim] < shape[dim])
                return;
            nd_index[dim] = 0;
        }
    }

    inline auto generate_access_pattern(const std::vector<std::size_t>& record_shape)
    {
        const auto record_size = std::accumulate(std::cbegin(record_shape), std::cend(record_shape),
            1UL, std::multiplies<std::size_t>());
        std::vector<index_swap_pair> access_patern(record_size);
        std::vector<std::size_t> nd_index(std::size(record_shape));
        for (auto index = 0UL; index < record_size; index++)
        {
            auto reversed_flat_index = inverted_flat_index(nd_index, record_shape);
            access_patern[index] = { index, reversed_flat_index };
            next_index(nd_index, record_shape);
        }
        return access_patern;
    }

}

// From column to row major by default, from row to column major with to_column.
template <bool is_string, typename shape_t, typename data_t, bool to_column = false>
void swap(data_t& data, const shape_t& shape)
{
    const auto dimensions = std::size(shape);
    // Records with less than 2 dimensions read the same in both majorities. A string is an
    // element of shape.back() characters.
    if ((dimensions > 2 && !is_string) or (is_string and dimensions > 3))
    {
        const std::size_t records_count = shape[0];
        const std::size_t element_size = is_string ? shape.back() : 1;
        const std::vector<std::size_t> record_shape(
            std::rbegin(shape) + (is_string ? 1 : 0), std::crend(shape) - 1);
        const auto access_patern = _private::generate_access_pattern(record_shape);
        const auto values_per_record = std::size(access_patern) * element_size;
        std::vector<typename data_t::value_type> temporary_record(values_per_record);
        for (std::size_t offset = 0; offset < records_count * values_per_record;
             offset += values_per_record)
        {
            for (const auto& [row, column] : access_patern)
            {
                const auto [to, from]
                    = to_column ? std::pair { column, row } : std::pair { row, column };
                std::copy_n(data.data() + offset + from * element_size, element_size,
                    temporary_record.data() + to * element_size);
            }
            std::copy(std::cbegin(temporary_record), std::cend(temporary_record),
                data.data() + offset);
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
}
