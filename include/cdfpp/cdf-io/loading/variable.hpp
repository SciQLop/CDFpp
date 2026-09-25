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
#include <cpp_utils/containers/no_init_vector.hpp>
using cpp_utils::containers::no_init_vector;
#include "cdfpp/variable.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <vector>

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
                std::copy_if(std::cbegin(vdr.zDimSizes), std::cend(vdr.zDimSizes),
                    std::back_inserter(shape),
                    [DimVarys = vdr.DimVarys.begin()]([[maybe_unused]] const auto& v) mutable
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
            if (std::size(vdr.DimVarys) != 0)
            {
                std::copy_if(std::cbegin(context.gdr.rDimSizes), std::cend(context.gdr.rDimSizes),
                    std::back_inserter(shape),
                    [DimVarys = vdr.DimVarys.begin()]([[maybe_unused]] const auto& v) mutable
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

    // Records [first, last], as stored in one VVR/CVVR block.
    struct stored_records
    {
        std::size_t first;
        std::size_t last;
    };

    // Each block goes at the place of its first record: a file may leave records out (sparse
    // records), so blocks are not necessarily back to back.
    template <typename cdf_version_tag_t, typename stream_t>
    void load_var_data(stream_t& stream, char* data, std::size_t data_len,
        const cdf_VXR_t<cdf_version_tag_t>& vxr, std::size_t record_size,
        const cdf_compression_type compression_type, std::vector<stored_records>& stored)
    {
        for (int32_t i = 0; i < vxr.NusedEntries; i++)
        {
            const auto first = static_cast<std::size_t>(vxr.First[i]);
            const auto last = static_cast<std::size_t>(vxr.Last[i]);
            const std::size_t offset = first * record_size;
            if (offset >= data_len)
                continue;

            if (cdf_mutable_variable_record_t<cdf_version_tag_t> cvvr_or_vvr {};
                load_mut_record(cvvr_or_vvr, stream, vxr.Offset[i]))
            {
                using vvr_t = typename decltype(cvvr_or_vvr)::vvr_t;
                using vxr_t = typename decltype(cvvr_or_vvr)::vxr_t;
                using cvvr_t = typename decltype(cvvr_or_vvr)::cvvr_t;

                cvvr_or_vvr.visit(
                    [&](const vvr_t& vvr) -> void
                    {
                        const auto size = std::min((last - first + 1) * record_size, data_len - offset);
                        load_vvr_data<cdf_version_tag_t>(stream, vxr.Offset[i], size, vvr, data + offset);
                        stored.push_back({ first, last });
                    },
                    [&](vxr_t sub) -> void
                    {
                        load_var_data<cdf_version_tag_t, stream_t>(
                            stream, data, data_len, sub, record_size, compression_type, stored);
                        while (sub.VXRnext)
                        {
                            load_record(sub, stream, sub.VXRnext);
                            load_var_data<cdf_version_tag_t, stream_t>(
                                stream, data, data_len, sub, record_size, compression_type, stored);
                        }
                    },
                    [&](const cvvr_t& cvvr) -> void
                    {
                        decompression::inflate(
                            compression_type, cvvr.data, data + offset, data_len - offset);
                        stored.push_back({ first, last });
                    },
                    [](const std::monostate&) -> void
                    {
                        throw std::runtime_error {
                            "Error loading variable data expecting VVR, CVVR or VXR"
                        };
                    });
            }
        }
    }

    // How to fill the records a file doesn't store, as NASA's library does (CDF User's Guide,
    // "Sparse Records" and "Pad Values"): the previous stored record for previous-missing sparse
    // records, when there is one; otherwise `value`, one element in the file's byte order.
    struct missing_records_t
    {
        bool repeat_previous = false;
        std::vector<char> value;
    };

    inline void fill_missing_records(char* data, std::size_t record_count, std::size_t record_size,
        std::vector<stored_records>& stored, const missing_records_t& missing)
    {
        const auto fill = [&](std::size_t from, std::size_t to)
        {
            if (from >= to)
                return;
            if (missing.repeat_previous && from > 0)
            {
                for (auto record = from; record < to; ++record)
                    std::memcpy(data + record * record_size, data + (from - 1) * record_size,
                        record_size);
            }
            else if (!missing.value.empty())
            {
                const auto end = to * record_size;
                for (auto byte = from * record_size; byte < end; byte += std::size(missing.value))
                    std::memcpy(data + byte, missing.value.data(),
                        std::min(std::size(missing.value), end - byte));
            }
        };
        std::ranges::sort(stored, {}, &stored_records::first);
        std::size_t next = 0;
        for (const auto& block : stored)
        {
            fill(next, std::min(block.first, record_count));
            next = std::max(next, block.last + 1);
        }
        fill(next, record_count);
    }

    template <typename VDR_t, typename stream_t>
    data_t load_var_data(stream_t& stream, const VDR_t& vdr, const std::size_t record_size,
        const uint32_t record_count, const cdf_compression_type compression_type,
        const missing_records_t& missing)
    {
        const auto data_len
            = static_cast<std::size_t>(record_count) * static_cast<std::size_t>(record_size);
        data_t data = new_data_container(data_len, vdr.DataType);
        std::vector<stored_records> stored;
        cdf_VXR_t<typename VDR_t::cdf_version_t> vxr;

        if (vdr.VXRhead != 0 && load_record(vxr, stream, vdr.VXRhead))
        {
            load_var_data(stream, data.bytes_ptr(), data_len, vxr, record_size, compression_type,
                stored);
            while (vxr.VXRnext != 0)
            {
                if (!load_record(vxr, stream, vxr.VXRnext))
                    throw std::runtime_error { "Failed to read vxr" };
                load_var_data(stream, data.bytes_ptr(), data_len, vxr, record_size,
                    compression_type, stored);
            }
        }
        fill_missing_records(data.bytes_ptr(), record_count, record_size, stored, missing);
        return data;
    }

    // CDF User's Guide, table 2.8 "Default Pad Values", in native byte order.
    inline std::vector<char> default_pad_value(CDF_Types type, std::size_t num_elements)
    {
        using enum CDF_Types;
        const auto bytes_of = [](auto value)
        {
            std::vector<char> bytes(sizeof(value));
            std::memcpy(bytes.data(), &value, sizeof(value));
            return bytes;
        };
        switch (type)
        {
            case CDF_BYTE:
            case CDF_INT1:
                return bytes_of(int8_t { -127 });
            case CDF_UINT1:
                return bytes_of(uint8_t { 254 });
            case CDF_INT2:
                return bytes_of(int16_t { -32767 });
            case CDF_UINT2:
                return bytes_of(uint16_t { 65534 });
            case CDF_INT4:
                return bytes_of(int32_t { -2147483647 });
            case CDF_UINT4:
                return bytes_of(uint32_t { 4294967294U });
            case CDF_INT8:
            case CDF_TIME_TT2000:
                return bytes_of(int64_t { -9223372036854775807LL });
            case CDF_REAL4:
            case CDF_FLOAT:
                return bytes_of(-1.0e30f);
            case CDF_REAL8:
            case CDF_DOUBLE:
                return bytes_of(-1.0e30);
            case CDF_EPOCH:
                return bytes_of(0.0);
            case CDF_EPOCH16:
                return std::vector<char>(2 * sizeof(double), 0);
            case CDF_CHAR:
            case CDF_UCHAR:
            {
                std::vector<char> text(num_elements, '\0');
                if (!text.empty())
                    text[0] = ' ';
                return text;
            }
            default:
                return {};
        }
    }

    // One element of `type`, from native byte order to the file's (a byte swap undoes itself).
    inline std::vector<char> to_file_byte_order(
        std::vector<char> value, CDF_Types type, cdf_encoding encoding)
    {
        data_t data = new_data_container(std::size(value), type);
        std::memcpy(data.bytes_ptr(), value.data(), std::size(value));
        data = load_values<false>(std::move(data), encoding);
        std::memcpy(value.data(), data.bytes_ptr(), std::size(value));
        return value;
    }

    // FILLVAL when the variable has one of its own type (NASA's library since 3.8), else the pad
    // value the file declares, else the default pad value of the type.
    template <typename VDR_t>
    missing_records_t missing_records_for(const VDR_t& vdr,
        const cdf_map<std::string, VariableAttribute>& attributes, cdf_encoding encoding)
    {
        const auto element_size
            = cdf_type_size(vdr.DataType) * static_cast<std::size_t>(std::max(vdr.NumElems, 1));
        missing_records_t missing { .repeat_previous = vdr.SRecords == 2, .value = {} };
        if (const auto fillval = attributes.find("FILLVAL"); fillval != std::cend(attributes)
            and cdf_type_size((*fillval->second).type()) == cdf_type_size(vdr.DataType)
            and (*fillval->second).bytes() == element_size)
        {
            const auto& value = *fillval->second;
            missing.value = to_file_byte_order(
                { value.bytes_ptr(), value.bytes_ptr() + value.bytes() }, vdr.DataType, encoding);
        }
        else if (vdr.Flags & 2 and std::size(vdr.PadValues) == element_size)
        {
            missing.value.assign(std::cbegin(vdr.PadValues), std::cend(vdr.PadValues));
        }
        else
        {
            missing.value = to_file_byte_order(
                default_pad_value(vdr.DataType, static_cast<std::size_t>(vdr.NumElems)),
                vdr.DataType, encoding);
        }
        return missing;
    }


    template <typename cdf_version_tag_t, typename stream_t>
    void count_blocks_in_vxr(stream_t& stream, const cdf_VXR_t<cdf_version_tag_t>& vxr,
        std::size_t& count, std::size_t limit)
    {
        for (int32_t i = 0; i < vxr.NusedEntries && count < limit; i++)
        {
            // The header alone gives the record type: fully loading a VVR/CVVR would copy its
            // data.
            cdf_mutable_variable_record_t<cdf_version_tag_t> rec {};
            if (!load_record(rec.header, stream, vxr.Offset[i]))
                continue;
            switch (rec.header.record_type)
            {
                case cdf_record_type::VVR:
                case cdf_record_type::CVVR:
                    ++count;
                    break;
                case cdf_record_type::VXR:
                    if (cdf_VXR_t<cdf_version_tag_t> sub; load_record(sub, stream, vxr.Offset[i]))
                    {
                        count_blocks_in_vxr<cdf_version_tag_t>(stream, sub, count, limit);
                        while (sub.VXRnext && count < limit)
                        {
                            load_record(sub, stream, sub.VXRnext);
                            count_blocks_in_vxr<cdf_version_tag_t>(stream, sub, count, limit);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }

    // Counts the leaf VVR/CVVR blocks reachable from a variable's VXR head, stopping
    // early once `limit` is reached (callers only need contiguous ⟺ count <= 1). Reads
    // index records only — no variable data — so it is cheap and works in lazy mode.
    template <typename cdf_version_tag_t, typename stream_t>
    std::size_t count_var_blocks(stream_t stream, std::size_t vxr_head, std::size_t limit = 2)
    {
        std::size_t count = 0UL;
        if (vxr_head != 0)
        {
            cdf_VXR_t<cdf_version_tag_t> vxr;
            if (load_record(vxr, stream, vxr_head))
            {
                count_blocks_in_vxr<cdf_version_tag_t>(stream, vxr, count, limit);
                while (vxr.VXRnext && count < limit)
                {
                    load_record(vxr, stream, vxr.VXRnext);
                    count_blocks_in_vxr<cdf_version_tag_t>(stream, vxr, count, limit);
                }
            }
        }
        return count;
    }

    template <bool iso_8859_1_to_utf8, typename stream_t, typename VDR_t>
    struct defered_variable_loader
    {
        defered_variable_loader(stream_t stream, cdf_encoding encoding, VDR_t vdr,
            uint32_t record_count, std::size_t record_size, cdf_compression_type compression,
            missing_records_t missing)
                : p_stream { stream }
                , p_encoding { encoding }
                , p_vdr { vdr }
                , p_record_count { record_count }
                , p_record_size { record_size }
                , p_compression { compression }
                , p_missing { std::move(missing) }
        {
        }

        inline data_t operator()()
        {
            return load_values<iso_8859_1_to_utf8>(
                load_var_data(this->p_stream, this->p_vdr, this->p_record_size,
                    this->p_record_count, p_compression, p_missing),
                this->p_encoding);
        }

    private:
        stream_t p_stream;
        cdf_encoding p_encoding;
        VDR_t p_vdr;
        uint32_t p_record_count;
        std::size_t p_record_size;
        cdf_compression_type p_compression;
        missing_records_t p_missing;
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
                    const std::size_t record_size = var_record_size(shape, vdr.DataType);
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
                            return static_cast<uint32_t>(MaxRec) + 1U;
                        }
                    }();
                    /*if ((vdr.DataType != CDF_Types::CDF_CHAR
                            and vdr.DataType != CDF_Types::CDF_UCHAR)
                        or !common::is_nrv(vdr))
                    {*/
                    shape.insert(std::cbegin(shape), record_count);
                    /*}*/
                    constexpr bool is_zvariable = (type == cdf_r_z::z);
                    auto block_counter
                        = [buffer = context.buffer,
                              vxr_head = static_cast<std::size_t>(vdr.VXRhead)]() -> std::size_t
                    { return count_var_blocks<cdf_version_tag_t>(buffer, vxr_head); };
                    auto missing = missing_records_for(
                        vdr, cdf.var_attributes[vdr.Num], context.encoding());
                    if (lazy_load)
                    {
                        common::add_lazy_variable(cdf, vdr.Name.value, vdr.Num,
                            lazy_data { defered_variable_loader<iso_8859_1_to_utf8,
                                            decltype(context.buffer), decltype(vdr)> {
                                            context.buffer, context.encoding(), vdr, record_count,
                                            record_size, compression_type, std::move(missing) },
                                vdr.DataType },
                            std::move(shape), is_nrv, compression_type, is_zvariable,
                            std::move(block_counter));
                    }
                    else
                    {
                        common::add_variable(cdf, vdr.Name.value, vdr.Num,
                            load_values<iso_8859_1_to_utf8>(
                                load_var_data(context.buffer, vdr, record_size, record_count,
                                    compression_type, missing),
                                context.encoding()),
                            std::move(shape), is_nrv, compression_type, is_zvariable,
                            std::move(block_counter));
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
