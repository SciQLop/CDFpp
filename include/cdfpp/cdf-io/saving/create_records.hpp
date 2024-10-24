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
#include "cdfpp/no_init_vector.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <utility>

namespace cdf::io
{

namespace saving
{

    record_wrapper<cdf_CPR_t<v3x_tag>> make_cpr(cdf_compression_type ct)
    {
        record_wrapper<cdf_CPR_t<v3x_tag>> cpr { { {}, ct, 0, 0, {} } };
        switch (ct)
        {
            case cdf_compression_type::rle_compression:
                break;
            case cdf_compression_type::gzip_compression:
                cpr.record.pCount = 1;
                cpr.record.cParms.values.push_back(9);
                break;
            default:
                throw std::invalid_argument { "Unsupported compression algorithm" };
                break;
        }
        update_size(cpr);
        return cpr;
    }

    int32_t attribute_entry_num_elements(const data_t& entry)
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
            auto& fac
                = svg_ctx.body.file_attributes.emplace_back(file_attribute_ctx { index, &attribute,
                    cdf_ADR_t<v3x_tag> { {}, 0, 0, cdf_attr_scope::global, index,
                        static_cast<int32_t>(attribute.size()),
                        static_cast<int32_t>(attribute.size()) - 1, 0, 0, 0, -1, 0, { name } },
                    {} });
            update_size(fac.adr);
            int32_t value_index = 0UL;
            for (const auto& data : attribute)
            {
                auto& aedr = fac.aedrs.emplace_back(cdf_AgrEDR_t<v3x_tag> {
                    {}, 0, index, data.type(), value_index, 0, 0, 0, 0, 0, 0 });
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
                    cdf_ADR_t<v3x_tag> {
                        {}, 0, 0, cdf_attr_scope::variable, index, 0, -1, 0, 0, 0, 0, 0, { name } },
                    {} };
            }
            auto& vac = svg_ctx.body.variable_attributes[name];
            update_size(vac.adr);
            vac.attrs.push_back(&attribute);
            const auto& data = *attribute;
            auto& aedr = vac.aedrs.emplace_back(cdf_AzEDR_t<v3x_tag> { {}, 0, vac.adr.record.num,
                data.type(), static_cast<int32_t>(variable.number), 0, 0, 0, 0, 0, 0 });
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
            vdr.zDimSizes.values.resize(vdr.zNumDims);
            vdr.DimVarys.values.resize(vdr.zNumDims);
            for (auto i = 0L; i < vdr.zNumDims; i++)
            {
                vdr.zDimSizes.values[i] = variable.shape()[i + 1];
                vdr.DimVarys.values[i] = -1;
            }
        }
        vdr.MaxRec = variable.len() - 1;
    }

    typename variable_ctx::values_records_t make_values_record(const Variable& v,
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
            cvvr.record.data.values = compression::deflate(v.compression_type(),
                std::string_view {
                    v.bytes_ptr() + first_record * record_size, records_in_vvr * record_size });
            cvvr.record.cSize = std::size(cvvr.record.data.values);
            update_size(cvvr);
            return cvvr;
        }
    }

    inline void create_variables_records(const CDF& cdf, saving_context& svg_ctx)
    {
        for (const auto& [name, variable] : cdf.variables)
        {
            int32_t index = std::size(svg_ctx.body.variables);
            auto& var_ctx = svg_ctx.body.variables.emplace_back(
                variable_ctx { variable.compression_type(), index, &variable,
                    cdf_zVDR_t<v3x_tag> { {}, 0, variable.type(), 0, 0, 0, !variable.is_nrv(), 0, 0,
                        -1, -1, 0, index, 0, 0, { name }, 0, {}, {}, {} },
                    {}, {} });

            populate_variable_geometry(variable, var_ctx.vdr.record);
            if (variable.compression_type() != cdf_compression_type::no_compression)
            {
                var_ctx.cpr = make_cpr(variable.compression_type());
                var_ctx.vdr.record.Flags |= 1 << 2;
                var_ctx.vdr.record.BlockingFactor = 0x40;
            }
            update_size(var_ctx.vdr);
            if (variable.len())
            {
                auto& vxr
                    = var_ctx.vxrs.emplace_back(cdf_VXR_t<v3x_tag> { {}, 0, 0, 0, {}, {}, {} });
                const auto var_record_size
                    = std::max(std::size_t { 1 },
                          flat_size(std::cbegin(variable.shape()) + 1, std::cend(variable.shape())))
                    * cdf_type_size(variable.type());
                {
                    auto records = variable.len();
                    auto first_record = 0;
                    while (records > 0)
                    {
                        // this is an arbitrary decision to limit VVRs to 1GB
                        auto records_in_vvr
                            = std::min(static_cast<std::size_t>((1 << 30) / var_record_size),
                                static_cast<std::size_t>(records));
                        var_ctx.values_records.emplace_back(make_values_record(
                            variable, records_in_vvr, var_record_size, first_record));
                        vxr.record.First.values.push_back(first_record);
                        vxr.record.Last.values.push_back(first_record + records_in_vvr - 1);
                        first_record += records_in_vvr;
                        records -= records_in_vvr;
                    }
                }
                vxr.record.Offset.values.resize(std::size(vxr.record.First.values));
                vxr.record.Nentries = std::size(vxr.record.First.values);
                vxr.record.NusedEntries = std::size(vxr.record.First.values);
                update_size(vxr);
            }
            create_variable_attributes_records(var_ctx, svg_ctx);
        }
    }


} // namespace


}
