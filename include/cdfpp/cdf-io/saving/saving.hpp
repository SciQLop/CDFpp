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
#include "../compression.hpp"
#include "../desc-records.hpp"
#include "./buffers.hpp"
#include "./create_records.hpp"
#include "./layout_records.hpp"
#include "./link_records.hpp"
#include "./records-saving.hpp"
#include "cdfpp/cdf-enums.hpp"
#include "cdfpp/cdf-file.hpp"
#include "cdfpp/chrono/cdf-leap-seconds.h"
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

namespace saving
{
    [[nodiscard]] inline std::size_t estimate_size(const CDF& cdf)
    {
        auto var_size = std::accumulate(std::cbegin(cdf.variables), std::cend(cdf.variables), 0UL,
            [](std::size_t sz, const auto& node) { return sz + node.mapped().bytes(); });
        return var_size + (1 << 16);
    }


    template <typename T, typename U>
    void write_record(const record_wrapper<T>& r, U&& writer, std::size_t virtual_offset = 0)
    {
        auto offset = save_record(r.record, writer) + virtual_offset;
        assert(r.offset == offset - r.size);
    }

    template <typename T, typename U>
    void write_records(const T& items, U&& writer, std::size_t virtual_offset = 0)
    {
        for (auto& item : items)
        {
            write_record(item, writer, virtual_offset);
        }
    }

    template <typename U>
    void write_records(const Attribute* const attr,
        const std::vector<record_wrapper<cdf_AgrEDR_t<v3x_tag>>>& aedrs, U&& writer,
        std::size_t virtual_offset = 0)
    {
        for (auto& aedr : aedrs)
        {
            save_record(aedr.record, writer);
            const auto& values = (*attr)[aedr.record.Num];
            auto offset = writer.write(values.bytes_ptr(), values.bytes()) + virtual_offset;
            assert(offset - aedr.size == aedr.offset);
        }
    }

    template <typename U>
    void write_records(const std::vector<const VariableAttribute*> attrs,
        const std::vector<record_wrapper<cdf_AzEDR_t<v3x_tag>>>& aedrs, U&& writer,
        std::size_t virtual_offset = 0)
    {
        assert(std::size(attrs) == std::size(aedrs));
        for (auto i = 0UL; i < std::size(attrs); i++)
        {
            auto& aedr = aedrs[i];
            auto& attr = *attrs[i];
            save_record(aedr.record, writer);
            const auto& values = *attr;
            auto offset = writer.write(values.bytes_ptr(), values.bytes()) + virtual_offset;
            assert(offset - aedr.size == aedr.offset);
        }
    }

    template <typename U>
    void write_records(const Variable* const variable,
        const std::vector<typename variable_ctx::values_records_t>& values_records, U&& writer,
        std::size_t virtual_offset = 0)
    {
        const auto* data = variable->bytes_ptr();
        for (auto& values_record : values_records)
        {
            visit(
                values_record,
                [&data, &writer, virtual_offset](const record_wrapper<cdf_VVR_t<v3x_tag>>& vvr)
                {
                    const auto header_sz = record_size(vvr.record);
                    const auto len = vvr.size - header_sz;
                    auto offset = save_record(vvr.record, data, len, writer) + virtual_offset;
                    assert(offset - vvr.size == vvr.offset);
                    data += len;
                },
                [&writer, virtual_offset](const record_wrapper<cdf_CVVR_t<v3x_tag>>& cvvr)
                { write_record(cvvr, writer, virtual_offset); });
        }
    }

    template <typename T>
    void write_file_attributes(const std::vector<file_attribute_ctx>& attributes, T& writer,
        std::size_t virtual_offset = 0)
    {
        for (auto& attr_ctx : attributes)
        {
            write_record(attr_ctx.adr, writer, virtual_offset);
            write_records(attr_ctx.attr, attr_ctx.aedrs, writer, virtual_offset);
        }
    }

    template <typename T>
    void write_variables_attributes(const nomap<std::string, variable_attribute_ctx>& attributes,
        T& writer, std::size_t virtual_offset = 0)
    {
        for (auto& [name, attr_ctx] : attributes)
        {
            write_record(attr_ctx.adr, writer, virtual_offset);
            write_records(attr_ctx.attrs, attr_ctx.aedrs, writer, virtual_offset);
        }
    }

    template <typename T>
    void write_variables(
        const std::vector<variable_ctx>& variables, T& writer, std::size_t virtual_offset = 0)
    {
        for (auto& variable_ctx : variables)
        {
            write_record(variable_ctx.vdr, writer, virtual_offset);
            write_records(variable_ctx.vxrs, writer, virtual_offset);
            if (variable_ctx.cpr)
            {
                write_record(variable_ctx.cpr.value(), writer, virtual_offset);
            }
            write_records(
                variable_ctx.variable, variable_ctx.values_records, writer, virtual_offset);
        }
    }

    template <typename T>
    void write_body(const cdf_body& body, T& writer, std::size_t virtual_offset = 0)
    {
        write_record(body.cdr, writer, virtual_offset);
        write_record(body.gdr, writer, virtual_offset);
        write_file_attributes(body.file_attributes, writer, virtual_offset);
        write_variables(body.variables, writer, virtual_offset);
        write_variables_attributes(body.variable_attributes, writer, virtual_offset);
    }

    template <typename T>
    void write_records(saving_context& svg_ctx, T& writer)
    {
        save_record(svg_ctx.magic, writer);
        if (svg_ctx.compression == cdf_compression_type::no_compression)
        {
            write_body(svg_ctx.body, writer);
        }
        else
        {
            write_record(svg_ctx.ccr.value(), writer);
            write_record(svg_ctx.cpr.value(), writer);
        }
    }

    [[nodiscard]] inline saving_context make_saving_context(const CDF& cdf)
    {
        saving_context svg_ctx;
        svg_ctx.compression = cdf.compression;
        if (cdf.compression == cdf_compression_type::no_compression)
        {
            svg_ctx.magic = { 0xCDF30001, 0x0000FFFF };
        }
        else
        {
            svg_ctx.magic = { 0xCDF30001, 0xCCCC0001 };
            svg_ctx.ccr = record_wrapper<cdf_CCR_t<v3x_tag>> { { {}, 0, 0, 0, {} } };
            svg_ctx.cpr = make_cpr(cdf.compression);
        }
        svg_ctx.body.cdr.record
            = cdf_CDR_t<v3x_tag> { {}, 0, 3, 8, CDFpp_ENCODING, 3, 0, 0, 0, 2, 0, { R"(
Common Data Format (CDF)\nhttps://cdf.gsfc.nasa.gov
Space Physics Data Facility
NASA/Goddard Space Flight Center
Greenbelt, Maryland 20771 USA
(User support: gsfc-cdf-support@lists.nasa.gov)
)" } };
        svg_ctx.body.gdr.record = { {}, 0, 0, 0, 0, 0, 0, 0xFFFF, 0, 0, 0, 0,
            chrono::leap_seconds::last_updated, { 0xFFFF }, {} };
        update_size(svg_ctx.body.cdr);
        update_size(svg_ctx.body.gdr);
        return svg_ctx;
    }

    inline void update_gdr(saving_context& svg_ctx, std::size_t eof)
    {
        svg_ctx.body.gdr.record.NzVars = std::size(svg_ctx.body.variables);
        svg_ctx.body.gdr.record.NumAttr
            = std::size(svg_ctx.body.file_attributes) + std::size(svg_ctx.body.variable_attributes);
        svg_ctx.body.gdr.record.eof = eof;
    }

    inline void apply_compression(saving_context& svg_ctx)
    {
        if (svg_ctx.ccr and svg_ctx.cpr)
        {
            svg_ctx.ccr->record.data.values.reserve(svg_ctx.body.gdr.record.eof);
            buffers::vector_writer writer { svg_ctx.ccr->record.data.values };
            write_body(svg_ctx.body, writer, 8);
            svg_ctx.ccr->record.uSize = std::size(writer.data);
            svg_ctx.ccr->record.data.values
                = compression::deflate(svg_ctx.compression, writer.data);
            update_size(svg_ctx.ccr.value());
            svg_ctx.cpr->offset = svg_ctx.ccr->offset + svg_ctx.ccr->size;
            svg_ctx.ccr->record.CPRoffset = svg_ctx.cpr->offset;
        }
    }


    template <typename T>
    [[nodiscard]] bool impl_save(const CDF& cdf, T& writer)
    {
        saving_context svg_ctx = make_saving_context(cdf);
        create_file_attributes_records(cdf, svg_ctx);
        create_variables_records(cdf, svg_ctx);
        auto eof = map_records(svg_ctx);
        link_records(svg_ctx);
        update_gdr(svg_ctx, eof);
        apply_compression(svg_ctx);
        write_records(svg_ctx, writer);
        return true;
    }

} // namespace


[[nodiscard]] inline bool save(const CDF& cdf, const std::string& path)
{
    buffers::file_writer writer { path };
    return saving::impl_save(cdf, writer);
}

[[nodiscard]] inline no_init_vector<char> save(const CDF& cdf)
{
    no_init_vector<char> data;
    data.reserve(saving::estimate_size(cdf));
    buffers::vector_writer writer { data };
    if (saving::impl_save(cdf, writer))
        return data;
    return {};
}

}
