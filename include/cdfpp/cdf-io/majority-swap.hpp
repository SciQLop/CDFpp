#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2022, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#include "../cdf-debug.hpp"
#include "../cdf-data.hpp"
#include "../no_init_vector.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <variant>
#include <vector>

namespace cdf
{
namespace _private
{

    struct index_swap_pair
    {
        std::size_t src;
        std::size_t dest;
    };
    void next_index(std::vector<std::size_t>& nd_index, const std::vector<std::size_t>& shape)
    {
        for (auto dim = 0UL; dim < std::size(shape); dim++)
        {
            nd_index[dim]++;
            if (nd_index[dim] < shape[dim])
                return;
            nd_index[dim] = 0;
        }
    }

    template <typename index_iter_t, typename shape_iter_t>
    [[nodiscard]] auto flat_index(index_iter_t&& nd_index_b, index_iter_t&& nd_index_e, shape_iter_t&& shape_b)
    {
        auto index = 0UL;
        auto shape_product = 1UL;
        do
        {
            index += (*nd_index_b) * shape_product;
            shape_product *= *shape_b;
            nd_index_b++;
            shape_b++;
        } while (nd_index_b != nd_index_e);
        return index;
    }

    auto generate_access_pattern(const std::vector<std::size_t>& record_shape)
    {
        const auto record_size = std::accumulate(std::cbegin(record_shape), std::cend(record_shape),
            1UL, std::multiplies<std::size_t>());
        std::vector<index_swap_pair> access_patern(record_size);
        std::vector<std::size_t> nd_index(std::size(record_shape));
        for (auto index = 0UL; index < record_size; index++)
        {
            auto reversed_flat_index = flat_index(
                std::crbegin(nd_index), std::crend(nd_index), std::crbegin(record_shape));
            access_patern[index] = { index, reversed_flat_index };
            next_index(nd_index, record_shape);
        }
        return access_patern;
    }

}

namespace majority
{
    template <typename shape_t, typename data_t,
        bool is_string = std::is_same_v<typename data_t::value_type, char>>
    void swap(data_t& data, const shape_t& shape)
    {
        const auto dimensions = std::size(shape);
        // Basically a variable with shape=2 is a variable with 1D records
        if (dimensions > 2 or (is_string and dimensions > 2))
        {
            const std::size_t records_count = is_string ? 1 : shape[0];
            const std::vector<std::size_t> record_shape(
                std::cbegin(shape) + (is_string ? 0 : 1), std::cend(shape) - (is_string ? 1 : 0));
            const auto access_patern = _private::generate_access_pattern(record_shape);

            std::vector<typename data_t::value_type> temporary_record(
                std::size(access_patern) * (is_string ? shape.back() : 1));

            for (auto record = 0UL; record < records_count; record++)
            {
                auto offset = record * std::size(access_patern);
                for (const auto& swap_pair : access_patern)
                {
                    if constexpr (is_string)
                    {
                        std::memcpy(temporary_record.data() + (swap_pair.src * shape.back()),
                            data.data() + offset + (swap_pair.dest * shape.back()), shape.back());
                    }
                    else
                    {
                        temporary_record[swap_pair.src] = data[offset + swap_pair.dest];
                    }
                }
                std::memcpy(data.data() + offset, temporary_record.data(),
                    (is_string ? shape.back() : sizeof(typename data_t::value_type))
                        * std::size(access_patern));
            }
        }
    }

    void swap(data_t& data, const no_init_vector<uint32_t>& shape)
    {
        switch (data.type())
        {
            case CDF_Types::CDF_BYTE:
            case CDF_Types::CDF_INT1:
                swap(data.get<int8_t>(), shape);
                break;
            case CDF_Types::CDF_CHAR:
                swap(data.get<CDF_Types::CDF_CHAR>(), shape);
                break;
            case CDF_Types::CDF_UCHAR:
                swap(data.get<CDF_Types::CDF_UCHAR>(), shape);
                break;
            case CDF_Types::CDF_UINT1:
                swap(data.get<unsigned char>(), shape);
                break;
            case CDF_Types::CDF_UINT2:
                swap(data.get<uint16_t>(), shape);
                break;
            case CDF_Types::CDF_UINT4:
                swap(data.get<uint32_t>(), shape);
                break;
            case CDF_Types::CDF_INT2:
                swap(data.get<int16_t>(), shape);
                break;
            case CDF_Types::CDF_INT4:
                swap(data.get<int32_t>(), shape);
                break;
            case CDF_Types::CDF_INT8:
                swap(data.get<int64_t>(), shape);
                break;
            case CDF_Types::CDF_FLOAT:
            case CDF_Types::CDF_REAL4:
                swap(data.get<float>(), shape);
                break;
            case CDF_Types::CDF_DOUBLE:
            case CDF_Types::CDF_REAL8:
                swap(data.get<double>(), shape);
                break;
            case CDF_Types::CDF_EPOCH:
                swap(data.get<epoch>(), shape);
                break;
            case CDF_Types::CDF_EPOCH16:
                swap(data.get<epoch16>(), shape);
                break;
            case CDF_Types::CDF_TIME_TT2000:
                swap(data.get<tt2000_t>(), shape);
                break;
            default:
                break;
        }
    }
}
}
