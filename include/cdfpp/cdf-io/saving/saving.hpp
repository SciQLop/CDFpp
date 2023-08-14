#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#include "../common.hpp"
#include "../decompression.hpp"
#include "../desc-records.hpp"
#include "../endianness.hpp"
#include "./buffers.hpp"
#include "./records-saving.hpp"
#include "cdfpp/cdf-enums.hpp"
#include "cdfpp/cdf-file.hpp"
#include "cdfpp/no_init_vector.hpp"
#include "cdfpp_config.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <utility>

namespace cdf::io
{

namespace
{
    std::size_t estimate_size(const CDF& cdf)
    {
        auto var_size = std::accumulate(std::cbegin(cdf.variables), std::cend(cdf.variables), 0UL,
            [](std::size_t sz, const auto& node) { return sz + node.mapped().bytes(); });
        return var_size + (1 << 16);
    }

    void prepare_file_attributes(const CDF& cdf, saving_context& svg_ctx)
    {
        for (const auto& [name, attribute] : cdf.attributes)
        {
            int32_t index
                = std::size(svg_ctx.file_attributes) + std::size(svg_ctx.variable_attributes);
            auto& fac = svg_ctx.file_attributes.emplace_back(file_attribute_ctx { index, &attribute,
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
                        { return std::max(1L, std::count(std::cbegin(v), std::cend(v), '\n')); },
                        [](const no_init_vector<unsigned char>& v) -> uint32_t
                        {
                            return std::max(1L,
                                std::count(std::cbegin(v), std::cend(v),
                                    static_cast<unsigned char>('\n')));
                        },
                        [](const auto&) -> uint32_t { return 0; });
                }
                aedr.record.NumElements = visit(
                    data, [](const cdf_none&) -> uint32_t { return 0; },
                    [](const auto& v) -> uint32_t { return std::size(v); });
                value_index += 1;
                update_size(aedr, aedr.record.NumElements * cdf_type_size(aedr.record.DataType));
            }
        }
    }

    void prepare_variables_attributes(const variable_ctx& variable, saving_context& svg_ctx)
    {
        for (const auto& [name, attribute] : variable.variable->attributes)
        {
            if (svg_ctx.variable_attributes.count(name) == 0)
            {
                int32_t index
                    = std::size(svg_ctx.file_attributes) + std::size(svg_ctx.variable_attributes);
                svg_ctx.variable_attributes[name] = variable_attribute_ctx { index, {},
                    cdf_ADR_t<v3x_tag> {
                        {}, 0, 0, cdf_attr_scope::variable, index, 0, -1, 0, 0, 0, 0, 0, { name } },
                    {} };
            }
            auto& vac = svg_ctx.variable_attributes[name];
            update_size(vac.adr);
            vac.attrs.push_back(&attribute);
            const auto& data = attribute[0UL];
            auto& aedr = vac.aedrs.emplace_back(cdf_AzEDR_t<v3x_tag> { {}, 0, vac.adr.record.num,
                data.type(), static_cast<int32_t>(variable.number), 0, 0, 0, 0, 0, 0 });
            if (is_string(data.type()))
            {
                aedr.record.NumStrings = visit(
                    data,
                    [](const no_init_vector<char>& v) -> int32_t
                    { return std::max(1L, std::count(std::cbegin(v), std::cend(v), '\n')); },
                    [](const no_init_vector<unsigned char>& v) -> int32_t
                    {
                        return std::max(1L,
                            std::count(
                                std::cbegin(v), std::cend(v), static_cast<unsigned char>('\n')));
                    },
                    [](const auto&) -> int32_t { return 0; });
            }
            aedr.record.NumElements = visit(
                data, [](const cdf_none&) -> int32_t { return 0; },
                [](const auto& v) -> int32_t { return std::size(v); });
            update_size(aedr, aedr.record.NumElements * cdf_type_size(aedr.record.DataType));
            vac.adr.record.MAXzEntries = std::max(vac.adr.record.MAXzEntries, aedr.record.Num);
            vac.adr.record.NzEntries = std::size(vac.aedrs);
        }
    }

    void prepare_variables(const CDF& cdf, saving_context& svg_ctx)
    {
        for (const auto& [name, variable] : cdf.variables)
        {
            int32_t index = std::size(svg_ctx.variables);
            auto& var_ctx = svg_ctx.variables.emplace_back(variable_ctx { index, &variable,
                cdf_zVDR_t<v3x_tag> { {}, 0, variable.type(), 0, 0, 0, !variable.is_nrv(), 0, 0, 0,
                    0, 0, index, 0, 0, { name }, 0, {}, {}, {} },
                {}, {} });

            if (is_string(variable.type()))
            {
                var_ctx.vdr.record.NumElems = variable.shape().back();
                var_ctx.vdr.record.zNumDims = std::max(0UL, std::size(variable.shape()) - 2);
            }
            else
            {
                var_ctx.vdr.record.NumElems = 1;
                var_ctx.vdr.record.zNumDims = std::max(0UL, std::size(variable.shape()) - 1);
            }
            if (var_ctx.vdr.record.zNumDims > 0)
            {
                var_ctx.vdr.record.zDimSizes.values.resize(var_ctx.vdr.record.zNumDims);
                var_ctx.vdr.record.DimVarys.values.resize(var_ctx.vdr.record.zNumDims);
                for (auto i = 0L; i < var_ctx.vdr.record.zNumDims; i++)
                {
                    var_ctx.vdr.record.zDimSizes.values[i] = variable.shape()[i + 1];
                    var_ctx.vdr.record.DimVarys.values[i] = -1;
                }
            }
            var_ctx.vdr.record.MaxRec = variable.len() - 1;
            update_size(var_ctx.vdr);

            auto& vxr = var_ctx.vxrs.emplace_back(cdf_VXR_t<v3x_tag> { {}, 0, 0, 0, {}, {}, {} });
            const auto var_record_size
                = flat_size(std::cbegin(variable.shape()) + 1, std::cend(variable.shape()))
                * cdf_type_size(variable.type());
            {
                auto records = variable.len();
                auto first_record = 0;
                while (records > 0)
                {
                    // this is an arbitrary decision to limit VVRs to 1GB
                    auto records_in_vvr = std::min((1 << 30) / var_record_size, records);
                    auto& vvr = var_ctx.vvrs.emplace_back(cdf_VVR_t<v3x_tag> {
                        {},
                    });
                    vxr.record.First.values.push_back(first_record);
                    vxr.record.Last.values.push_back(first_record + records_in_vvr - 1);
                    update_size(vvr, records_in_vvr * var_record_size);
                    first_record += records_in_vvr;
                    records -= records_in_vvr;
                }
            }
            vxr.record.Offset.values.resize(std::size(vxr.record.First.values));
            vxr.record.Nentries = std::size(vxr.record.First.values);
            vxr.record.NusedEntries = std::size(vxr.record.First.values);
            update_size(vxr);
            prepare_variables_attributes(var_ctx, svg_ctx);
        }
    }

    template <typename T, typename U>
    std::size_t layout(T&& items, std::size_t offset, U&& function)
    {
        for (auto& item : items)
        {
            item.offset = offset;
            offset += item.size;
            function(item, offset);
        }
        return offset;
    }

    template <typename T>
    std::size_t layout(T&& items, std::size_t offset)
    {
        for (auto& item : items)
        {
            item.offset = offset;
            offset += item.size;
        }
        return offset;
    }

    [[nodiscard]] std::size_t map_records(saving_context& svg_ctx)
    {
        svg_ctx.cdr.offset = 8;
        svg_ctx.gdr.offset = 8 + svg_ctx.cdr.size;
        auto offset = svg_ctx.gdr.offset + svg_ctx.gdr.size;
        for (auto& fac : svg_ctx.file_attributes)
        {
            fac.adr.offset = offset;
            offset += fac.adr.size;
            offset = layout(fac.aedrs, offset);
        }
        for (auto& vc : svg_ctx.variables)
        {
            vc.vdr.offset = offset;
            offset += vc.vdr.size;
            offset = layout(vc.vxrs, offset);
            offset = layout(vc.vvrs, offset);
        }
        for (auto& [name, vac] : svg_ctx.variable_attributes)
        {
            vac.adr.offset = offset;
            offset += vac.adr.size;
            offset = layout(vac.aedrs, offset);
        }
        return offset;
    }

    template <typename T>
    void link_aedrs(T& fac)
    {
        if constexpr (std::is_same_v<T, file_attribute_ctx>)
            fac.adr.record.AgrEDRhead = fac.aedrs.front().offset;
        else
            fac.adr.record.AzEDRhead = fac.aedrs.front().offset;
        auto last_offset = 0;
        std::for_each(std::rbegin(fac.aedrs), std::rend(fac.aedrs),
            [&last_offset](auto& aedr)
            {
                aedr.record.AEDRnext = last_offset;
                last_offset = aedr.offset;
            });
    }

    void link_adrs(saving_context& svg_ctx)
    {
        auto last_offset = 0;
        std::for_each(std::rbegin(svg_ctx.variable_attributes),
            std::rend(svg_ctx.variable_attributes),
            [&last_offset](auto& node)
            {
                auto& [name, vac] = node;
                vac.adr.record.ADRnext = last_offset;
                last_offset = vac.adr.offset;
                link_aedrs(vac);
            });
        for (auto i = 0UL; i < std::size(svg_ctx.file_attributes) - 1; i++)
        {
            svg_ctx.file_attributes[i].adr.record.ADRnext
                = svg_ctx.file_attributes[i + 1].adr.offset;
            link_aedrs(svg_ctx.file_attributes[i]);
        }
        if (std::size(svg_ctx.file_attributes))
        {
            svg_ctx.file_attributes.back().adr.record.ADRnext = last_offset;
            link_aedrs(svg_ctx.file_attributes.back());
        }
    }

    inline void link_vxrs(variable_ctx& vc)
    {
        auto last_offset = 0;
        auto last_vvr = vc.vvrs.rbegin();
        std::for_each(std::rbegin(vc.vxrs), std::rend(vc.vxrs),
            [&last_offset, &last_vvr](record_wrapper<cdf_VXR_t<v3x_tag>>& vxr)
            {
                vxr.record.VXRnext = last_offset;
                last_offset = vxr.offset;
                std::for_each(std::rbegin(vxr.record.Offset.values),
                    std::rend(vxr.record.Offset.values),
                    [&last_vvr](auto& offset)
                    {
                        offset = last_vvr->offset;
                        last_vvr += 1;
                    });
            });
    }

    inline void link_vdrs(saving_context& svg_ctx)
    {
        auto last_offset = 0;
        std::for_each(std::rbegin(svg_ctx.variables), std::rend(svg_ctx.variables),
            [&last_offset](variable_ctx& vc)
            {
                vc.vdr.record.VDRnext = last_offset;
                last_offset = vc.vdr.offset;
                if (std::size(vc.vxrs) >= 1)
                {
                    vc.vdr.record.VXRhead = vc.vxrs.front().offset;
                    vc.vdr.record.VXRtail = vc.vxrs.back().offset;
                    link_vxrs(vc);
                }
            });
    }

    void link_records(saving_context& svg_ctx)
    {
        svg_ctx.cdr.record.GDRoffset = svg_ctx.gdr.offset;
        if (std::size(svg_ctx.file_attributes))
        {
            svg_ctx.gdr.record.ADRhead = svg_ctx.file_attributes.front().adr.offset;
        }
        else
        {
            if (std::size(svg_ctx.variable_attributes))
            {
                svg_ctx.gdr.record.ADRhead
                    = std::cbegin(svg_ctx.variable_attributes)->second.adr.offset;
            }
        }
        if (std::size(svg_ctx.variables))
        {
            svg_ctx.gdr.record.zVDRhead = svg_ctx.variables.front().vdr.offset;
        }
        link_adrs(svg_ctx);
        link_vdrs(svg_ctx);
    }

    template <typename T, typename U>
    void write_records(saving_context&, T&& items, U&& writer)
    {
        for (auto& item : items)
        {
            auto offset = save_record(item.record, writer);
            assert(item.offset == offset - item.size);
        }
    }

    template <typename U>
    void write_records(const Attribute* const attr,
        std::vector<record_wrapper<cdf_AgrEDR_t<v3x_tag>>>& aedrs, U&& writer)
    {
        for (auto& aedr : aedrs)
        {
            save_record(aedr.record, writer);
            const auto& values = (*attr)[aedr.record.Num];
            auto offset = writer.write(values.bytes_ptr(), values.bytes());
            assert(offset - aedr.size == aedr.offset);
        }
    }

    template <typename U>
    void write_records(std::vector<const Attribute*> attrs,
        std::vector<record_wrapper<cdf_AzEDR_t<v3x_tag>>>& aedrs, U&& writer)
    {
        assert(std::size(attrs) == std::size(aedrs));
        for (auto i = 0UL; i < std::size(attrs); i++)
        {
            auto& aedr = aedrs[i];
            auto& attr = *attrs[i];
            save_record(aedr.record, writer);
            const auto& values = attr[0];
            auto offset = writer.write(values.bytes_ptr(), values.bytes());
            assert(offset - aedr.size == aedr.offset);
        }
    }

    template <typename U>
    void write_records(const Variable* const variable,
        std::vector<record_wrapper<cdf_VVR_t<v3x_tag>>>& vvrs, U&& writer)
    {
        const auto* data = variable->bytes_ptr();
        for (auto& vvr : vvrs)
        {
            const auto header_sz = record_size(vvr.record);
            const auto len = vvr.size - header_sz;
            auto offset = save_record(vvr.record, data, len, writer);
            assert(offset - vvr.size == vvr.offset);
            data += len;
        }
    }

    template <typename T>
    void write_records(saving_context& svg_ctx, T& writer)
    {
        save_record(svg_ctx.magic, writer);
        {
            auto offset = save_record(svg_ctx.cdr.record, writer);
            assert(offset - svg_ctx.cdr.size == svg_ctx.cdr.offset);
        }
        {
            auto offset = save_record(svg_ctx.gdr.record, writer);
            assert(offset - svg_ctx.gdr.size == svg_ctx.gdr.offset);
        }
        for (auto& fac : svg_ctx.file_attributes)
        {
            auto offset = save_record(fac.adr.record, writer);
            assert(offset - fac.adr.size == fac.adr.offset);
            write_records(fac.attr, fac.aedrs, writer);
        }
        for (auto& vc : svg_ctx.variables)
        {
            auto offset = save_record(vc.vdr.record, writer);
            assert(offset - vc.vdr.size == vc.vdr.offset);
            write_records(svg_ctx, vc.vxrs, writer);
            write_records(vc.variable, vc.vvrs, writer);
        }
        for (auto& [name, vac] : svg_ctx.variable_attributes)
        {
            auto offset = save_record(vac.adr.record, writer);
            assert(offset - vac.adr.size == vac.adr.offset);
            write_records(vac.attrs, vac.aedrs, writer);
        }
    }

    saving_context make_saving_context(const CDF& cdf)
    {
        saving_context svg_ctx;
        svg_ctx.magic = { 0xCDF30001, 0x0000FFFF };
        svg_ctx.cdr.record
            = cdf_CDR_t<v3x_tag> { {}, 0, 3, 8, CDFpp_ENCODING, 3, 0, 0, 0, 2, 0, { "" } };
        svg_ctx.gdr.record = { {}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {} };
        update_size(svg_ctx.cdr);
        update_size(svg_ctx.gdr);
        return svg_ctx;
    }

    void update_gdr(saving_context& svg_ctx, std::size_t eof)
    {
        svg_ctx.gdr.record.NzVars = std::size(svg_ctx.variables);
        svg_ctx.gdr.record.NumAttr
            = std::size(svg_ctx.file_attributes) + std::size(svg_ctx.variable_attributes);
        svg_ctx.gdr.record.eof = eof;
    }

    template <typename T>
    [[nodiscard]] bool uncompressed_impl_save(const CDF& cdf, T& writer)
    {
        saving_context svg_ctx = make_saving_context(cdf);
        prepare_file_attributes(cdf, svg_ctx);
        prepare_variables(cdf, svg_ctx);
        auto eof = map_records(svg_ctx);
        link_records(svg_ctx);
        update_gdr(svg_ctx, eof);
        write_records(svg_ctx, writer);
        return true;
    }


    template <typename T>
    [[nodiscard]] bool impl_save(const CDF& cdf, T& writer)
    {
        if (cdf.compression == cdf_compression_type::no_compression)
        {
            return uncompressed_impl_save(cdf, writer);
        }
        return false;
    }

} // namespace


[[nodiscard]] bool save(const CDF& cdf, const std::string& path)
{
    buffers::file_writer writer { path };
    return impl_save(cdf, writer);
}

[[nodiscard]] no_init_vector<char> save(const CDF& cdf)
{
    no_init_vector<char> data;
    data.reserve(estimate_size(cdf));
    buffers::in_memory_writer writer { data };
    if (impl_save(cdf, writer))
        return data;
    return {};
}

}
