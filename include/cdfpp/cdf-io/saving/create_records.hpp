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

#include "../compression.hpp"
#include "../desc-records.hpp"
#include "./records-saving.hpp"
#include "cdfpp/cdf-enums.hpp"
#include "cdfpp/cdf-file.hpp"
#include "cdfpp/cdf-parallel.hpp"
#include <cpp_utils/containers/no_init_vector.hpp>
using cpp_utils::containers::no_init_vector;
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <utility>

namespace cdf::io
{

namespace saving
{

    inline record_wrapper<cdf_CPR_t<v3x_tag>> make_cpr(cdf_compression_type ct, int32_t gzip_level)
    {
        record_wrapper<cdf_CPR_t<v3x_tag>> cpr { { {}, ct, 0, 0, {} } };
        switch (ct)
        {
            case cdf_compression_type::rle_compression:
                cpr.record.pCount = 1;
                cpr.record.cParms.push_back(0);
                break;
            case cdf_compression_type::gzip_compression:
                cpr.record.pCount = 1;
                cpr.record.cParms.push_back(checked_gzip_level(gzip_level));
                break;
#ifdef CDFPP_USE_ZSTD
            case cdf_compression_type::zstd_compression:
                cpr.record.pCount = 1;
                cpr.record.cParms.push_back(zstd::compression_level);
                break;
#endif
#ifdef CDFPP_USE_BLOSC2
            case cdf_compression_type::blosc2_compression:
                cpr.record.pCount = 1;
                cpr.record.cParms.push_back(blosc2::compression_level);
                break;
#endif
            default:
                throw std::invalid_argument { "Unsupported compression algorithm" };
                break;
        }
        update_size(cpr);
        return cpr;
    }

    inline int32_t attribute_entry_num_elements(const data_t& entry)
    {
        return visit(
            entry, [](const cdf_none&) -> int32_t { return 0; },
            [](const auto& v) -> int32_t { return std::size(v); });
    }

    inline void create_file_attributes_records(const CDF& cdf, saving_context& svg_ctx)
    {
        for (const auto& [name, attribute] : cdf.attributes)
        {
            int32_t index = std::size(svg_ctx.body.file_attributes)
                + std::size(svg_ctx.body.variable_attributes);
            auto& fac = svg_ctx.body.file_attributes.emplace_back(file_attribute_ctx { index,
                &attribute,
                cdf_ADR_t<v3x_tag> { {}, 0, 0, cdf_attr_scope::global, index,
                    static_cast<int32_t>(attribute.size()),
                    static_cast<int32_t>(attribute.size()) - 1, { 0 }, 0, 0, -1, { -1 }, { name } },
                {} });
            update_size(fac.adr);
            int32_t value_index = 0UL;
            for (const auto& data : attribute)
            {
                auto& aedr = fac.aedrs.emplace_back(cdf_AgrEDR_t<v3x_tag> {
                    {}, 0, index, data.type(), value_index, 0, 0, 0, 0, -1, -1, {} });
                if (is_string(data.type()))
                {
                    aedr.record.NumStrings = visit(
                        data,
                        [](const no_init_vector<char>& v) -> uint32_t
                        {
                            return std::max(std::size_t { 1 },
                                static_cast<std::size_t>(
                                    std::count(std::cbegin(v), std::cend(v), '\n')));
                        },
                        [](const no_init_vector<unsigned char>& v) -> uint32_t
                        {
                            return std::max(std::size_t { 1 },
                                static_cast<std::size_t>(std::count(std::cbegin(v), std::cend(v),
                                    static_cast<unsigned char>('\n'))));
                        },
                        [](const auto&) -> uint32_t { return 0; });
                }
                aedr.record.NumElements = attribute_entry_num_elements(data);
                value_index += 1;
                update_size(aedr, aedr.record.NumElements * cdf_type_size(aedr.record.DataType));
            }
        }
    }

    inline void create_variable_attributes_records(
        const variable_ctx& variable, saving_context& svg_ctx)
    {
        for (const auto& [name, attribute] : variable.variable->attributes)
        {
            if (svg_ctx.body.variable_attributes.count(name) == 0)
            {
                int32_t index = std::size(svg_ctx.body.file_attributes)
                    + std::size(svg_ctx.body.variable_attributes);
                svg_ctx.body.variable_attributes[name] = variable_attribute_ctx { index, {},
                    cdf_ADR_t<v3x_tag> { {}, 0, 0, cdf_attr_scope::variable, index, 0, -1, 0, 0, 0,
                        0, -1, { name } },
                    {} };
            }
            auto& vac = svg_ctx.body.variable_attributes[name];
            update_size(vac.adr);
            vac.attrs.push_back(&attribute);
            const auto& data = *attribute;
            auto& aedr = vac.aedrs.emplace_back(cdf_AzEDR_t<v3x_tag> { {}, 0, vac.adr.record.num,
                data.type(), static_cast<int32_t>(variable.number), 0, 0, 0, 0, -1, -1, {} });
            if (is_string(data.type()))
            {
                aedr.record.NumStrings = visit(
                    data,
                    [](const no_init_vector<char>& v) -> int32_t
                    {
                        return std::max(std::size_t { 1 },
                            static_cast<std::size_t>(
                                std::count(std::cbegin(v), std::cend(v), '\n')));
                    },
                    [](const no_init_vector<unsigned char>& v) -> int32_t
                    {
                        return std::max(std::size_t { 1 },
                            static_cast<std::size_t>(std::count(
                                std::cbegin(v), std::cend(v), static_cast<unsigned char>('\n'))));
                    },
                    [](const auto&) -> int32_t { return 0; });
            }
            aedr.record.NumElements = attribute_entry_num_elements(data);
            update_size(aedr, aedr.record.NumElements * cdf_type_size(aedr.record.DataType));
            vac.adr.record.MAXzEntries = std::max(vac.adr.record.MAXzEntries, aedr.record.Num);
            vac.adr.record.NzEntries = std::size(vac.aedrs);
        }
    }

    inline void populate_variable_geometry(const Variable& variable, cdf_zVDR_t<v3x_tag>& vdr)
    {
        if (is_string(variable.type()))
        {
            vdr.NumElems = variable.shape().back();
            vdr.zNumDims
                = std::max(int32_t { 0 }, static_cast<int32_t>(std::size(variable.shape())) - 2);
        }
        else
        {
            vdr.NumElems = 1;
            vdr.zNumDims
                = std::max(int32_t { 0 }, static_cast<int32_t>(std::size(variable.shape())) - 1);
        }
        if (vdr.zNumDims > 0)
        {
            vdr.zDimSizes.resize(vdr.zNumDims);
            vdr.DimVarys.resize(vdr.zNumDims);
            for (auto i = 0L; i < vdr.zNumDims; i++)
            {
                vdr.zDimSizes[i] = variable.shape()[i + 1];
                vdr.DimVarys[i] = -1;
            }
        }
        vdr.MaxRec = variable.len() - 1;
    }

    inline typename variable_ctx::values_records_t make_values_record(const Variable& v,
        const std::size_t records_in_vvr, const std::size_t record_size,
        const std::size_t first_record)
    {
        if (v.compression_type() == cdf_compression_type::no_compression)
        {
            auto vvr = record_wrapper<cdf_VVR_t<v3x_tag>> {};
            update_size(vvr, records_in_vvr * record_size);
            return vvr;
        }
        else
        {
            auto cvvr = record_wrapper<cdf_CVVR_t<v3x_tag>> {};
            auto compressed = compression::deflate(v.compression_type(), v.compression_level(),
                std::string_view {
                    v.bytes_ptr() + first_record * record_size, records_in_vvr * record_size },
                cdf_type_size(v.type()), record_size);
            cvvr.record.data.resize(std::size(compressed));
            std::memcpy(cvvr.record.data.data(), compressed.data(), std::size(compressed));
            cvvr.record.cSize = std::size(cvvr.record.data);
            update_size(cvvr);
            return cvvr;
        }
    }

    // Compressed variables are cut in blocks of about this size, so readers can decompress them
    // on several threads, or only the part they need. On CDAWeb data, the compressed size stays
    // within 0.1% of one block per variable, for gzip, zstd and blosc2.
    inline constexpr std::size_t compressed_block_bytes = 256 * 1024;
    // An arbitrary limit, for uncompressed variables.
    inline constexpr std::size_t uncompressed_block_bytes = 1 << 30;

    inline std::size_t record_size_of(const Variable& variable)
    {
        return std::max(std::size_t { 1 },
                   flat_size(std::cbegin(variable.shape()) + 1, std::cend(variable.shape())))
            * cdf_type_size(variable.type());
    }

    inline std::size_t records_per_block(const Variable& variable, std::size_t record_size)
    {
        const auto block_bytes = variable.compression_type() == cdf_compression_type::no_compression
            ? uncompressed_block_bytes
            : compressed_block_bytes;
        return std::max(std::size_t { 1 }, block_bytes / record_size);
    }

    // NASA's library reports a VXR with more than 10 entries as corrupted; it writes 7 itself
    // (see NUM_VXR_ENTRIES in cdflib's cdfwrite.py). Longer variables get a chain of VXRs.
    inline constexpr std::size_t max_vxr_entries = 7;

    inline void create_vxrs(variable_ctx& var_ctx, std::size_t records, std::size_t per_block)
    {
        const std::size_t blocks = (records + per_block - 1) / per_block;
        for (std::size_t block = 0; block < blocks; ++block)
        {
            if (block % max_vxr_entries == 0)
                var_ctx.vxrs.emplace_back(cdf_VXR_t<v3x_tag> { {}, 0, 0, 0, {}, {}, {} });
            auto& vxr = var_ctx.vxrs.back().record;
            vxr.First.push_back(static_cast<int32_t>(block * per_block));
            vxr.Last.push_back(static_cast<int32_t>(std::min(records, (block + 1) * per_block) - 1));
        }
        for (auto& vxr : var_ctx.vxrs)
        {
            vxr.record.Offset.resize(std::size(vxr.record.First));
            vxr.record.Nentries = static_cast<int32_t>(std::size(vxr.record.First));
            vxr.record.NusedEntries = vxr.record.Nentries;
            update_size(vxr);
        }
    }

    inline void create_values_records(const Variable& variable, variable_ctx& var_ctx,
        std::size_t record_size, std::size_t per_block)
    {
        const std::size_t records = variable.len();
        create_vxrs(var_ctx, records, per_block);
        const std::size_t blocks = (records + per_block - 1) / per_block;
        var_ctx.values_records.resize(blocks);
        const bool worth_threads = variable.compression_type() != cdf_compression_type::no_compression
            && records * record_size >= parallel::min_bytes_worth_threads;
        parallel::for_each_index(blocks, worth_threads ? parallel::hardware_threads() : 1,
            [&](std::size_t block)
            {
                const auto first = block * per_block;
                const auto count = std::min(records, first + per_block) - first;
                var_ctx.values_records[block]
                    = make_values_record(variable, count, record_size, first);
            });
    }

    inline void create_variables_records(const CDF& cdf, saving_context& svg_ctx)
    {
        for (const auto& [name, variable] : cdf.variables)
        {
            int32_t index = std::size(svg_ctx.body.variables);
            auto& var_ctx = svg_ctx.body.variables.emplace_back(
                variable_ctx { .compression = variable.compression_type(),
                    .number = index,
                    .variable = &variable,
                    .vdr = cdf_zVDR_t<v3x_tag> { .header = {},
                        .VDRnext = 0,
                        .DataType = variable.type(),
                        .MaxRec = 0,
                        .VXRhead = 0,
                        .VXRtail = 0,
                        .Flags = !variable.is_nrv(),
                        .SRecords = static_cast<int32_t>(variable.sparse_records()),
                        .rfuB = { 0 },
                        .rfuC = { -1 },
                        .rfuF = { -1 },
                        .NumElems = 0,
                        .Num = index,
                        .CPRorSPRoffset = 0,
                        .BlockingFactor = 0,
                        .Name = { name },
                        .zNumDims = 0,
                        .zDimSizes = {},
                        .DimVarys = {},
                        .PadValues = {} },
                    .vxrs = {},
                    .values_records {},
                    .cpr = std::nullopt });

            populate_variable_geometry(variable, var_ctx.vdr.record);
            // An empty variable may have no shape at all, hence no record size.
            const auto record_size = variable.len() ? record_size_of(variable) : 0;
            const auto per_block = variable.len() ? records_per_block(variable, record_size) : 0;
            if (variable.compression_type() != cdf_compression_type::no_compression)
            {
                var_ctx.cpr = make_cpr(variable.compression_type(), variable.compression_level());
                var_ctx.vdr.record.Flags |= 1 << 2;
                var_ctx.vdr.record.BlockingFactor = static_cast<int32_t>(per_block);
            }
            update_size(var_ctx.vdr);
            if (variable.len())
                create_values_records(variable, var_ctx, record_size, per_block);
            create_variable_attributes_records(var_ctx, svg_ctx);
        }
    }


} // namespace


}
