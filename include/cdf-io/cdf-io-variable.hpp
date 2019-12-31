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
#include "../cdf-file.hpp"
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

    template <typename cdf_version_tag_t, typename stream_t>
    std::size_t load(std::size_t offset, stream_t& stream, CDF& cdf)
    {
        cdf_VDR_t<cdf_version_tag_t, stream_t> VDR { stream, offset };
        if (VDR.is_loaded)
        {
            return VDR.VDRnext.value;
        }
        return 0;
    }

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

    template <cdf_r_z type, typename cdf_vxr_t, typename stream_t, typename context_t>
    Variable::var_data_t get_variable_data(
        const cdf_vxr_t& vxr, CDF_Types value_type, stream_t& stream, context_t& context)
    {
        if constexpr (type == cdf_r_z::z)
        {
            return {};
        }
        else
        {
            return {};
        }
    }

    template <typename stream_t>
    void load_data(stream_t& stream, const vvr_data_chunk& chunk, char* data)
    {
        return common::read_buffer(stream, data, chunk.offset, chunk.size);
    }

    template <typename cdf_version_tag_t, typename stream_t>
    vvr_data_chunk get_vvr_desc(const cdf_VVR_t<cdf_version_tag_t, stream_t>& vvr, std::size_t size)
    {
        return vvr_data_chunk { vvr.offset + AFTER(vvr.header), size };
    }

    template <typename cdf_version_tag_t, typename stream_t>
    std::vector<vvr_data_chunk> parse_vxr(
        const cdf_VXR_t<cdf_version_tag_t, stream_t>& vxr, uint32_t record_size)
    {
        std::vector<vvr_data_chunk> chunks(vxr.NusedEntries.value);
        for (int i = 0; i < vxr.NusedEntries.value; i++)
        {
            int record_count = vxr.Last.value[i] - vxr.First.value[i];
            cdf_VVR_t<cdf_version_tag_t, stream_t> vvr { vxr.p_stream, vxr.Offset.value[i] };
            chunks[i] = get_vvr_desc(vvr, record_size * record_count);
        }
        return chunks;
    }

    std::size_t var_record_size(const std::vector<uint32_t>& shape, CDF_Types type)
    {
        return cdf_type_size(type)
            * std::accumulate(std::cbegin(shape), std::cend(shape), 1, std::multiplies<uint32_t>());
    }

    template <cdf_r_z type, typename cdf_version_tag_t, typename stream_t, typename context_t>
    bool load_all_Vars(stream_t& stream, context_t& context, CDF& cdf)
    {
        std::for_each(begin_VDR<type>(context.gdr), end_VDR<type>(context.gdr),
            [&](const cdf_VDR_t<cdf_version_tag_t, stream_t>& vdr) {
                if (vdr.is_loaded)
                {
                    auto shape = get_variable_dimensions<type>(vdr, stream, context);
                    uint32_t record_size = var_record_size(shape, vdr.DataType.value);
                    std::vector<vvr_data_chunk> data_chunks;
                    std::for_each(begin_VXR(vdr), end_VXR(vdr),
                        [&](const cdf_VXR_t<cdf_version_tag_t, stream_t>& vxr) {
                            auto new_chunks = parse_vxr(vxr, record_size);
                            data_chunks.insert(std::end(data_chunks), std::begin(new_chunks),
                                std::end(new_chunks));
                        });
                    std::size_t total_size
                        = std::accumulate(std::cbegin(data_chunks), std::cend(data_chunks), 0,
                            [](std::size_t acc, const auto& chunk) { return acc + chunk.size; });
                    std::vector<char> data(total_size);
                    std::for_each(std::cbegin(data_chunks), std::cend(data_chunks),
                        [&, pos = 0](const vvr_data_chunk& chunk) mutable {
                            load_data(stream, chunk, data.data() + pos);
                            pos += chunk.size;
                        });
                    Variable v{vdr.Name.value, std::move(shape), load_values(data.data(), std::size(data), vdr.DataType.value, cdf_encoding::IBMPC)};
                    add_variable(cdf, vdr.Name.value, std::move(v));
                }
            });
        return true;
    }
}

template <typename cdf_version_tag_t, typename stream_t, typename context_t>
bool load_all(stream_t& stream, context_t& context, CDF& cdf)
{
    return load_all_Vars<cdf_r_z::r, cdf_version_tag_t>(stream, context, cdf)
        & load_all_Vars<cdf_r_z::z, cdf_version_tag_t>(stream, context, cdf);
}

}
