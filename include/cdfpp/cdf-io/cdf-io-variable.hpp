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
#include "../cdf-debug.hpp"
#include "../cdf-endianness.hpp"
#include "../variable.hpp"
#include "cdf-io-common.hpp"
#include "cdf-io-desc-records.hpp"
#include "cdf-io-rle.hpp"
#include "cdf-io-zlib.hpp"
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
            std::vector<uint32_t> all_sizes(vdr.zNumDims.value);
            std::vector<uint32_t> sizes;
            if (vdr.zNumDims.value)
            {
                std::size_t offset = vdr.offset + AFTER(vdr.zNumDims);
                common::load_values<endianness::big_endian_t>(stream, offset, all_sizes);
                std::copy_if(std::cbegin(all_sizes), std::cend(all_sizes),
                    std::back_inserter(sizes),
                    [DimVarys = vdr.DimVarys.value.begin()]([[maybe_unused]] const auto& v) mutable
                    {
                        bool vary = *DimVarys != 0;
                        DimVarys++;
                        return vary;
                    });
            }
            if (vdr.DataType.value == cdf::CDF_Types::CDF_CHAR
                or vdr.DataType.value == cdf::CDF_Types::CDF_UCHAR)
            {
                sizes.push_back(vdr.NumElems.value);
            }
            return sizes;
        }
        else
        {
            if (std::size(vdr.DimVarys.value) == 0)
                return { 1 };
            std::vector<uint32_t> shape;
            std::copy_if(std::cbegin(context.gdr.rDimSizes.value),
                std::cend(context.gdr.rDimSizes.value), std::back_inserter(shape),
                [DimVarys = vdr.DimVarys.value.begin()]([[maybe_unused]] const auto& v) mutable
                {
                    bool vary = *DimVarys != 0;
                    DimVarys++;
                    return vary;
                });
            return shape;
        }
    }


    template <typename cdf_version_tag_t, typename buffer_t>
    inline void load_vvr_data(buffer_t& stream, std::size_t size,
        const cdf_VVR_t<cdf_version_tag_t, buffer_t>& vvr, char* const data)
    {
        stream.read(data, vvr.offset + AFTER(vvr.header), size);
    }

    template <typename cdf_version_tag_t, typename buffer_t>
    inline void load_vvr_data(buffer_t& stream, const cdf_VVR_t<cdf_version_tag_t, buffer_t>& vvr,
        const std::size_t vvr_records_count, const uint32_t record_size, std::size_t& pos,
        std::vector<char>& data)
    {
        std::size_t data_size = std::min(vvr_records_count * record_size, std::size(data) - pos);
        CDFPP_ASSERT(data_size <= vvr.data_size());
        load_vvr_data<cdf_version_tag_t>(stream, data_size, vvr, data.data() + pos);
        pos += data_size;
    }


    template <typename cdf_version_tag_t, typename buffer_t>
    inline void load_cvvr_data(const cdf_CVVR_t<cdf_version_tag_t, buffer_t>& cvvr,
        const std::size_t vvr_records_count, const uint32_t record_size, std::size_t& pos,
        const cdf_compression_type compression_type, std::vector<char>& data)
    {
        std::vector<char> vvr_data;
        if (compression_type == cdf_compression_type::gzip_compression)
        {
            zlib::gzinflate(cvvr.data.value, vvr_data);
        }
        else
        {
            if (compression_type == cdf_compression_type::rle_compression)
            {
                rle::deflate(cvvr.data.value, vvr_data);
            }
            else
                throw std::runtime_error { "Unsuported variable compression algorithm" };
        }
        if (std::size(vvr_data))
        {
            std::size_t data_size
                = std::min(vvr_records_count * record_size, std::size(data) - pos);
            CDFPP_ASSERT(data_size <= std::size(vvr_data));
            std::copy(std::cbegin(vvr_data), std::cbegin(vvr_data) + data_size, data.data() + pos);
            pos += data_size;
        }
    }

    template <bool maybe_compressed, typename cdf_version_tag_t, typename stream_t>
    void load_var_data(stream_t& stream, std::vector<char>& data, std::size_t& pos,
        const cdf_VXR_t<cdf_version_tag_t, stream_t>& vxr, uint32_t record_size,
        const cdf_compression_type compression_type)
    {
        for (auto i = 0UL; i < vxr.NusedEntries.value; i++)
        {
            int record_count = vxr.Last.value[i] - vxr.First.value[i] + 1;

            if (cdf_mutable_variable_record_t<cdf_version_tag_t, stream_t> cvvr_or_vvr {};
                cvvr_or_vvr.load_from(vxr.p_buffer, vxr.Offset.value[i]))
            {
                using vvr_t = typename decltype(cvvr_or_vvr)::vvr_t;
                using vxr_t = typename decltype(cvvr_or_vvr)::vxr_t;
                using cvvr_t = typename decltype(cvvr_or_vvr)::cvvr_t;

                cvvr_or_vvr.visit(
                    [&stream, &data, &pos, record_count, record_size](const vvr_t& vvr) -> void {
                        load_vvr_data<cdf_version_tag_t, stream_t>(
                            stream, vvr, record_count, record_size, pos, data);
                    },
                    [&stream, &data, &pos, record_size, compression_type](vxr_t vxr) -> void
                    {
                        load_var_data<maybe_compressed, cdf_version_tag_t, stream_t>(
                            stream, data, pos, vxr, record_size, compression_type);
                        while (vxr.VXRnext.value)
                        {
                            vxr.load(vxr.VXRnext.value);
                            load_var_data<maybe_compressed, cdf_version_tag_t, stream_t>(
                                stream, data, pos, vxr, record_size, compression_type);
                        }
                    },
                    [&stream, &data, &pos, record_count, record_size, compression_type](
                        const cvvr_t& cvvr) -> void
                    {
                        load_cvvr_data<cdf_version_tag_t, stream_t>(
                            cvvr, record_count, record_size, pos, compression_type, data);
                    },
                    [](const std::monostate&) -> void {
                        throw std::runtime_error {
                            "Error loading variable data expecting VVR, CVVR or VXR"
                        };
                    });
            }
        }
    }

    template <bool maybe_compressed, cdf_r_z rz_, typename cdf_version_tag_t, typename stream_t>
    std::vector<char> load_var_data(stream_t& stream,
        const cdf_VDR_t<rz_, cdf_version_tag_t, stream_t>& vdr, uint32_t record_size,
        uint32_t record_count)
    {
        std::vector<char> data(record_count * record_size);
        std::size_t pos { 0UL };
        cdf_VXR_t<cdf_version_tag_t, stream_t> vxr { stream };
        const auto compression_type = [&]()
        {
            if constexpr (maybe_compressed)
            {
                if (cdf_CPR_t<cdf_version_tag_t, stream_t> CPR { stream };
                    vdr.CPRorSPRoffset.value != static_cast<decltype(vdr.CPRorSPRoffset.value)>(-1)
                    && CPR.load(vdr.CPRorSPRoffset.value))
                    return CPR.cType.value;
            }
            return cdf_compression_type::no_compression;
        }();
        if (vdr.VXRhead.value != 0 && vxr.load(vdr.VXRhead.value))
        {
            load_var_data<maybe_compressed>(stream, data, pos, vxr, record_size, compression_type);
            if (vxr.VXRnext.value)
            {
                do
                {
                    vxr.load(vxr.VXRnext.value);
                    if (vxr.is_loaded)
                    {
                        load_var_data<maybe_compressed>(
                            stream, data, pos, vxr, record_size, compression_type);
                    }
                    else
                    {
                        throw std::runtime_error { "Failed to read vxr" };
                    }
                } while (vxr.VXRnext.value != 0);
            }
        }
        return data;
    }


    std::size_t var_record_size(const std::vector<uint32_t>& shape, CDF_Types type)
    {
        return cdf_type_size(type)
            * std::accumulate(std::cbegin(shape), std::cend(shape), 1, std::multiplies<uint32_t>());
    }

    template <cdf_r_z type, typename cdf_version_tag_t, typename stream_t,
        typename context_t>
    bool load_all_Vars(stream_t& stream, context_t& context, common::cdf_repr& cdf)
    {
        std::for_each(begin_VDR<type>(context.gdr), end_VDR<type>(context.gdr),
            [&](const cdf_VDR_t<type, cdf_version_tag_t, stream_t>& vdr)
            {
                if (vdr.is_loaded)
                {
                    auto shape = get_variable_dimensions<type>(vdr, stream, context);
                    uint32_t record_size = var_record_size(shape, vdr.DataType.value);
                    uint32_t record_count = common::is_nrv(vdr) ? 1 : (vdr.MaxRec.value + 1);
                    if ((vdr.DataType.value != CDF_Types::CDF_CHAR
                            and vdr.DataType.value != CDF_Types::CDF_UCHAR)
                        or !common::is_nrv(vdr))
                    {
                        shape.insert(std::cbegin(shape), record_count);
                    }
                    auto data = [&]()
                    {
                        if (common::is_compressed(vdr))
                        {
                            return load_var_data<true>(stream, vdr, record_size, record_count);
                        }
                        else
                        {
                            return load_var_data<false>(stream, vdr, record_size, record_count);
                        }
                        return std::vector<char> {};
                    }();

                    common::add_variable(cdf, vdr.Name.value, vdr.Num.value,
                        load_values<false>(
                            data.data(), std::size(data), vdr.DataType.value, context.encoding()),
                        std::move(shape));
                }
            });
        return true;
    }
}

template <typename cdf_version_tag_t, typename context_t>
bool load_all(context_t& context, common::cdf_repr& cdf)
{
    return load_all_Vars<cdf_r_z::r, cdf_version_tag_t>(
               context.buffer, context, cdf)
        & load_all_Vars<cdf_r_z::z, cdf_version_tag_t>(
            context.buffer, context, cdf);
}

}
