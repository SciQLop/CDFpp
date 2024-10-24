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
#include "../common.hpp"
#include "../decompression.hpp"
#include "../desc-records.hpp"
#include "./records-loading.hpp"
#include "cdfpp/cdf-data.hpp"
#include "cdfpp/no_init_vector.hpp"
#include "cdfpp/variable.hpp"
#include <algorithm>
#include <cstdint>
#include <numeric>

namespace cdf::io::variable
{
namespace
{
    [[nodiscard]] std::size_t var_record_size(
        const no_init_vector<uint32_t>& shape, CDF_Types type) noexcept
    {
        if (std::size(shape))
            return cdf_type_size(type) * flat_size(shape);
        return cdf_type_size(type);
    }

    template <cdf_r_z type, typename cdf_vdr_t, typename context_t>
    no_init_vector<uint32_t> get_variable_dimensions(const cdf_vdr_t& vdr, context_t& context)
    {
        if constexpr (type == cdf_r_z::z)
        {

            no_init_vector<uint32_t> shape;
            if (vdr.zNumDims)
            {
                std::copy_if(std::cbegin(vdr.zDimSizes.values), std::cend(vdr.zDimSizes.values),
                    std::back_inserter(shape),
                    [DimVarys = vdr.DimVarys.values.begin()]([[maybe_unused]] const auto& v) mutable
                    {
                        bool vary = *DimVarys != 0;
                        DimVarys++;
                        return vary;
                    });
            }
            if (vdr.DataType == cdf::CDF_Types::CDF_CHAR
                or vdr.DataType == cdf::CDF_Types::CDF_UCHAR)
            {
                shape.push_back(vdr.NumElems);
            }
            return shape;
        }
        else
        {
            no_init_vector<uint32_t> shape;
            if (std::size(vdr.DimVarys.values) != 0)
            {
                std::copy_if(std::cbegin(context.gdr.rDimSizes.values),
                    std::cend(context.gdr.rDimSizes.values), std::back_inserter(shape),
                    [DimVarys = vdr.DimVarys.values.begin()]([[maybe_unused]] const auto& v) mutable
                    {
                        bool vary = *DimVarys != 0;
                        DimVarys++;
                        return vary;
                    });
            }
            if (vdr.DataType == cdf::CDF_Types::CDF_CHAR
                or vdr.DataType == cdf::CDF_Types::CDF_UCHAR)
            {
                shape.push_back(vdr.NumElems);
            }
            if (std::size(shape) == 0)
            {
                return { 1 };
            }
            return shape;
        }
    }


    template <typename cdf_version_tag_t, typename buffer_t>
    inline void load_vvr_data(buffer_t& stream, std::size_t offset, std::size_t size,
        const cdf_VVR_t<cdf_version_tag_t>& vvr, char* const data)
    {
        stream.read(
            data, offset + sizeof(vvr.header.record_size) + sizeof(vvr.header.record_type), size);
    }

    template <typename cdf_version_tag_t, typename buffer_t>
    inline void load_vvr_data(buffer_t& stream, std::size_t offset,
        const cdf_VVR_t<cdf_version_tag_t>& vvr, const std::size_t vvr_records_count,
        const uint32_t record_size, std::size_t& pos, char* data, std::size_t data_len)
    {
        std::size_t vvr_data_size
            = std::min(static_cast<std::size_t>(vvr_records_count * record_size),
                static_cast<std::size_t>(data_len - pos));
        load_vvr_data<cdf_version_tag_t>(stream, offset, vvr_data_size, vvr, data + pos);
        pos += vvr_data_size;
    }


    template <typename cdf_version_tag_t, typename buffer_t>
    inline void load_cvvr_data(const cdf_CVVR_t<cdf_version_tag_t>& cvvr, std::size_t& pos,
        const cdf_compression_type compression_type, char* data, std::size_t data_len)
    {
        pos += decompression::inflate(
            compression_type, cvvr.data.values, data + pos, data_len - pos);
    }

    template <typename cdf_version_tag_t, typename stream_t>
    void load_var_data(stream_t& stream, char* data, std::size_t data_len, std::size_t& pos,
        const cdf_VXR_t<cdf_version_tag_t>& vxr, uint32_t record_size,
        const cdf_compression_type compression_type)
    {
        for (auto i = 0UL; i < vxr.NusedEntries; i++)
        {
            int record_count = vxr.Last.values[i] - vxr.First.values[i] + 1;

            if (cdf_mutable_variable_record_t<cdf_version_tag_t> cvvr_or_vvr {};
                load_mut_record(cvvr_or_vvr, stream, vxr.Offset.values[i]))
            {
                using vvr_t = typename decltype(cvvr_or_vvr)::vvr_t;
                using vxr_t = typename decltype(cvvr_or_vvr)::vxr_t;
                using cvvr_t = typename decltype(cvvr_or_vvr)::cvvr_t;

                cvvr_or_vvr.visit(
                    [&stream, &data, data_len, &pos, record_count, record_size,
                        offset = vxr.Offset.values[i]](const vvr_t& vvr) -> void
                    {
                        load_vvr_data<cdf_version_tag_t, stream_t>(
                            stream, offset, vvr, record_count, record_size, pos, data, data_len);
                    },
                    [&stream, &data, data_len, &pos, record_size, compression_type](
                        vxr_t vxr) -> void
                    {
                        load_var_data<cdf_version_tag_t, stream_t>(
                            stream, data, data_len, pos, vxr, record_size, compression_type);
                        while (vxr.VXRnext)
                        {
                            load_record(vxr, stream, vxr.VXRnext);
                            load_var_data<cdf_version_tag_t, stream_t>(
                                stream, data, data_len, pos, vxr, record_size, compression_type);
                        }
                    },
                    [&stream, &data, data_len, &pos, record_count, record_size, compression_type](
                        const cvvr_t& cvvr) -> void {
                        load_cvvr_data<cdf_version_tag_t, stream_t>(
                            cvvr, pos, compression_type, data, data_len);
                    },
                    [](const std::monostate&) -> void {
                        throw std::runtime_error {
                            "Error loading variable data expecting VVR, CVVR or VXR"
                        };
                    });
            }
        }
    }

    template <typename VDR_t, typename stream_t>
    data_t load_var_data(stream_t& stream, const VDR_t& vdr, const uint32_t record_size,
        const uint32_t record_count, const cdf_compression_type compression_type)
    {
        data_t data = new_data_container(
            static_cast<std::size_t>(record_count) * static_cast<std::size_t>(record_size),
            vdr.DataType);
        std::size_t pos { 0UL };
        cdf_VXR_t<typename VDR_t::cdf_version_t> vxr;

        if (vdr.VXRhead != 0 && load_record(vxr, stream, vdr.VXRhead))
        {
            load_var_data(stream, data.bytes_ptr(), record_count * record_size, pos, vxr,
                record_size, compression_type);
            if (vxr.VXRnext)
            {
                do
                {
                    if (load_record(vxr, stream, vxr.VXRnext))
                    {
                        load_var_data(stream, data.bytes_ptr(), record_count * record_size, pos,
                            vxr, record_size, compression_type);
                    }
                    else
                    {
                        throw std::runtime_error { "Failed to read vxr" };
                    }
                } while (vxr.VXRnext != 0);
            }
        }
        return data;
    }


    template <bool iso_8859_1_to_utf8, typename stream_t, typename VDR_t>
    struct defered_variable_loader
    {
        defered_variable_loader(stream_t stream, cdf_encoding encoding, VDR_t vdr,
            uint32_t record_count, uint32_t record_size, cdf_compression_type compression)
                : p_stream { stream }
                , p_encoding { encoding }
                , p_vdr { vdr }
                , p_record_count { record_count }
                , p_record_size { record_size }
                , p_compression { compression }
        {
        }

        inline data_t operator()()
        {
            return load_values<iso_8859_1_to_utf8>(
                load_var_data(this->p_stream, this->p_vdr, this->p_record_size,
                    this->p_record_count, p_compression),
                this->p_encoding);
        }

    private:
        stream_t p_stream;
        cdf_encoding p_encoding;
        VDR_t p_vdr;
        uint32_t p_record_count;
        uint32_t p_record_size;
        cdf_compression_type p_compression;
    };

    template <cdf_r_z type, typename cdf_version_tag_t, bool iso_8859_1_to_utf8, typename context_t>
    bool load_all_Vars(context_t& context, common::cdf_repr& cdf, bool lazy_load = false)
    {
        std::for_each(begin_VDR<type>(context), end_VDR<type>(context),
            [&](const auto& blk)
            {
                const auto& [offset, vdr] = blk;
                {
                    auto shape = get_variable_dimensions<type>(vdr, context);
                    const uint32_t record_size = var_record_size(shape, vdr.DataType);
                    const auto is_nrv = common::is_nrv(vdr);
                    const auto compression_type = [&, &stream = context, &vdr = vdr]()
                    {
                        if (common::is_compressed(vdr))
                        {
                            if (cdf_CPR_t<cdf_version_tag_t> CPR;
                                vdr.CPRorSPRoffset != static_cast<decltype(vdr.CPRorSPRoffset)>(-1)
                                && load_record(CPR, stream, vdr.CPRorSPRoffset))
                                return CPR.cType;
                        }
                        return cdf_compression_type::no_compression;
                    }();
                    const uint32_t record_count = [is_nrv, MaxRec = vdr.MaxRec]() -> uint32_t
                    {
                        if (is_nrv and MaxRec != -1)
                            return 1;
                        else
                        {
                            return MaxRec + 1;
                        }
                    }();
                    /*if ((vdr.DataType != CDF_Types::CDF_CHAR
                            and vdr.DataType != CDF_Types::CDF_UCHAR)
                        or !common::is_nrv(vdr))
                    {*/
                    shape.insert(std::cbegin(shape), record_count);
                    /*}*/
                    if (lazy_load)
                    {
                        common::add_lazy_variable(cdf, vdr.Name.value, vdr.Num,
                            lazy_data { defered_variable_loader<iso_8859_1_to_utf8,
                                            decltype(context.buffer), decltype(vdr)> {
                                            context.buffer, context.encoding(), vdr, record_count,
                                            record_size, compression_type },
                                vdr.DataType },
                            std::move(shape), is_nrv, compression_type);
                    }
                    else
                    {
                        common::add_variable(cdf, vdr.Name.value, vdr.Num,
                            load_values<iso_8859_1_to_utf8>(
                                load_var_data(context.buffer, vdr, record_size, record_count,
                                    compression_type),
                                context.encoding()),
                            std::move(shape), is_nrv, compression_type);
                    }
                }
            });
        return true;
    }
}

template <typename cdf_version_tag_t, bool iso_8859_1_to_utf8, typename context_t>
bool load_all(context_t& context, cdf::io::common::cdf_repr& cdf, bool lazy_load = false)
{
    return load_all_Vars<cdf_r_z::r, cdf_version_tag_t, iso_8859_1_to_utf8>(context, cdf, lazy_load)
        & load_all_Vars<cdf_r_z::z, cdf_version_tag_t, iso_8859_1_to_utf8>(context, cdf, lazy_load);
}

}
