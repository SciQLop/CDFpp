#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2019, Plasma Physics Laboratory - CNRS
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
#include "../cdf-data.hpp"
#include "../cdf-endianness.hpp"
#include "../variable.hpp"
#include "cdf-io-common.hpp"
#include "cdf-io-desc-records.hpp"
#include <cstdint>
#include <numeric>

namespace cdf::io::variable
{
namespace
{
    struct vvr_data_chunk
    {
        std::size_t offset;
        std::size_t size;
    };

    template <cdf_r_z type, typename cdf_vdr_t, typename stream_t, typename context_t>
    std::vector<uint32_t> get_variable_dimensions(
        const cdf_vdr_t& vdr, stream_t& stream, context_t& context)
    {
        if constexpr (type == cdf_r_z::z)
        {
            std::vector<uint32_t> sizes(vdr.zNumDims.value);
            if (vdr.zNumDims.value)
            {
                std::size_t offset = vdr.offset + AFTER(vdr.zNumDims);
                common::load_values<endianness::big_endian_t>(stream, offset, sizes);
            }
            return sizes;
        }
        else
        {
            return context.gdr.rDimSizes.value;
        }
    }

    template <typename cdf_version_tag_t, typename stream_t>
    std::vector<vvr_data_chunk> parse_vxr(
        const cdf_VXR_t<cdf_version_tag_t, stream_t>& vxr, uint32_t record_size)
    {
        std::vector<vvr_data_chunk> chunks(vxr.NusedEntries.value);
        for (int i = 0; i < vxr.NusedEntries.value; i++)
        {
            int record_count = vxr.Last.value[i] - vxr.First.value[i] + 1;
            cdf_VVR_t<cdf_version_tag_t, stream_t> vvr { vxr.p_buffer, vxr.Offset.value[i] };
            chunks[i]
                = vvr_data_chunk { vvr.offset + AFTER(vvr.header), record_count * record_size };
        }
        return chunks;
    }

    std::size_t var_record_size(const std::vector<uint32_t>& shape, CDF_Types type)
    {
        return cdf_type_size(type)
            * std::accumulate(std::cbegin(shape), std::cend(shape), 1, std::multiplies<uint32_t>());
    }

    template <cdf_r_z type, typename cdf_version_tag_t, typename stream_t, typename context_t>
    bool load_all_Vars(stream_t& stream, context_t& context, common::cdf_repr& cdf)
    {
        std::for_each(begin_VDR<type>(context.gdr), end_VDR<type>(context.gdr),
            [&](const cdf_VDR_t<cdf_version_tag_t, stream_t>& vdr) {
                if (vdr.is_loaded)
                {
                    auto shape = get_variable_dimensions<type>(vdr, stream, context);
                    uint32_t record_size = var_record_size(shape, vdr.DataType.value);
                    uint32_t record_count = vdr.MaxRec.value + 1;
                    shape.insert(std::cbegin(shape), record_count);
                    std::vector<vvr_data_chunk> data_chunks;
                    std::for_each(begin_VXR(vdr), end_VXR(vdr),
                        [&](const cdf_VXR_t<cdf_version_tag_t, stream_t>& vxr) {
                            auto new_chunks = parse_vxr(vxr, record_size);
                            data_chunks.insert(std::end(data_chunks), std::begin(new_chunks),
                                std::end(new_chunks));
                        });
                    std::vector<char> data(record_size * record_count);
                    std::for_each(std::cbegin(data_chunks), std::cend(data_chunks),
                        [&, pos = 0](const vvr_data_chunk& chunk) mutable {
                            stream.read(data.data() + pos,chunk.offset, std::min(chunk.size, std::size(data) - pos) );
                            pos += chunk.size;
                        });

                    common::add_variable(cdf, vdr.Name.value, vdr.Num.value,
                        load_values(
                            data.data(), std::size(data), vdr.DataType.value, cdf_encoding::IBMPC),
                        std::move(shape));
                }
            });
        return true;
    }
}

template <typename cdf_version_tag_t, typename stream_t, typename context_t>
bool load_all(stream_t& stream, context_t& context, common::cdf_repr& cdf)
{
    return load_all_Vars<cdf_r_z::r, cdf_version_tag_t>(stream, context, cdf)
        & load_all_Vars<cdf_r_z::z, cdf_version_tag_t>(stream, context, cdf);
}

}
