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
#include <cpp_utils/containers/no_init_vector.hpp>
using cpp_utils::containers::no_init_vector;
#include "cdfpp_config.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <string_view>
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
    std::size_t write_value(const data_t& value, const file_layout& layout, U&& writer)
    {
        if (has_host_byte_order(layout.encoding))
            return writer.write(value.bytes_ptr(), value.bytes());
        const auto in_file_layout = layout.value(value);
        return writer.write(in_file_layout.bytes_ptr(), in_file_layout.bytes());
    }

    template <typename U>
    void write_records(const Attribute* const attr,
        const std::vector<record_wrapper<cdf_AgrEDR_t<v3x_tag>>>& aedrs, const file_layout& layout,
        U&& writer, std::size_t virtual_offset = 0)
    {
        for (auto& aedr : aedrs)
        {
            save_record(aedr.record, writer);
            const auto& values = (*attr)[aedr.record.Num];
            auto offset = write_value(values, layout, writer) + virtual_offset;
            assert(offset - aedr.size == aedr.offset);
        }
    }

    template <typename U>
    void write_records(const std::vector<const VariableAttribute*> attrs,
        const std::vector<record_wrapper<cdf_AzEDR_t<v3x_tag>>>& aedrs, const file_layout& layout,
        U&& writer, std::size_t virtual_offset = 0)
    {
        assert(std::size(attrs) == std::size(aedrs));
        for (auto i = 0UL; i < std::size(attrs); i++)
        {
            auto& aedr = aedrs[i];
            auto& attr = *attrs[i];
            save_record(aedr.record, writer);
            const auto& values = *attr;
            auto offset = write_value(values, layout, writer) + virtual_offset;
            assert(offset - aedr.size == aedr.offset);
        }
    }

    // Converted a chunk at a time, so a variable in another layout costs no copy of its values.
    inline constexpr std::size_t layout_chunk_bytes = 1 << 20;

    template <typename U>
    void write_in_file_layout(const Variable& variable, std::size_t first, std::size_t count,
        const file_layout& layout, U&& writer)
    {
        const auto record_size = record_size_of(variable);
        const auto per_chunk = std::max(std::size_t { 1 }, layout_chunk_bytes / record_size);
        for (auto chunk = first; chunk < first + count; chunk += per_chunk)
        {
            const auto in_chunk = std::min(per_chunk, first + count - chunk);
            const auto records = layout.records(variable, chunk, in_chunk, record_size);
            writer.write(records.bytes_ptr(), records.bytes());
        }
    }

    template <typename U>
    void write_records(const Variable* const variable,
        const std::vector<typename variable_ctx::values_records_t>& values_records,
        const file_layout& layout, U&& writer, std::size_t virtual_offset = 0)
    {
        const auto* data = variable->bytes_ptr();
        for (auto& values_record : values_records)
        {
            visit(
                values_record,
                [&](const record_wrapper<cdf_VVR_t<v3x_tag>>& vvr)
                {
                    const auto header_sz = record_size(vvr.record);
                    const auto len = vvr.size - header_sz;
                    if (layout.matches_memory())
                        save_record(vvr.record, data, len, writer);
                    else
                    {
                        save_record(vvr.record, writer);
                        const auto record_bytes = record_size_of(*variable);
                        write_in_file_layout(*variable,
                            static_cast<std::size_t>(data - variable->bytes_ptr()) / record_bytes,
                            len / record_bytes, layout, writer);
                    }
                    assert(writer.offset() + virtual_offset - vvr.size == vvr.offset);
                    data += len;
                },
                [&writer, virtual_offset](const record_wrapper<cdf_CVVR_t<v3x_tag>>& cvvr)
                { write_record(cvvr, writer, virtual_offset); });
        }
    }

    template <typename T>
    void write_file_attributes(const std::vector<file_attribute_ctx>& attributes,
        const file_layout& layout, T& writer, std::size_t virtual_offset = 0)
    {
        for (auto& attr_ctx : attributes)
        {
            write_record(attr_ctx.adr, writer, virtual_offset);
            write_records(attr_ctx.attr, attr_ctx.aedrs, layout, writer, virtual_offset);
        }
    }

    template <typename T>
    void write_variables_attributes(const nomap<std::string, variable_attribute_ctx>& attributes,
        const file_layout& layout, T& writer, std::size_t virtual_offset = 0)
    {
        for (auto& [name, attr_ctx] : attributes)
        {
            write_record(attr_ctx.adr, writer, virtual_offset);
            write_records(attr_ctx.attrs, attr_ctx.aedrs, layout, writer, virtual_offset);
        }
    }

    template <typename T>
    void write_variables(const std::vector<variable_ctx>& variables, const file_layout& layout,
        T& writer, std::size_t virtual_offset = 0)
    {
        for (auto& variable_ctx : variables)
        {
            write_record(variable_ctx.vdr, writer, virtual_offset);
            write_records(variable_ctx.vxrs, writer, virtual_offset);
            if (variable_ctx.cpr)
            {
                write_record(variable_ctx.cpr.value(), writer, virtual_offset);
            }
            write_records(variable_ctx.variable, variable_ctx.values_records, layout, writer,
                virtual_offset);
        }
    }

    template <typename T>
    void write_body(const cdf_body& body, T& writer, std::size_t virtual_offset = 0)
    {
        write_record(body.cdr, writer, virtual_offset);
        write_record(body.gdr, writer, virtual_offset);
        write_file_attributes(body.file_attributes, body.layout, writer, virtual_offset);
        write_variables(body.variables, body.layout, writer, virtual_offset);
        write_variables_attributes(body.variable_attributes, body.layout, writer, virtual_offset);
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

    // CDF Internal Format Description, CDR Flags: bit 0 row majority, bit 1 single file.
    [[nodiscard]] constexpr int32_t cdr_flags(const CDF& cdf) noexcept
    {
        return (cdf.majority == cdf_majority::row ? 1 : 0) | 2;
    }

    [[nodiscard]] inline saving_context make_saving_context(const CDF& cdf)
    {
        saving_context svg_ctx;
        svg_ctx.compression = cdf.compression;
        svg_ctx.compression_level = cdf.compression_level;
        svg_ctx.body.layout = { cdf.majority, cdf.encoding };
        if (cdf.compression == cdf_compression_type::no_compression)
        {
            svg_ctx.magic = { 0xCDF30001, 0x0000FFFF };
        }
        else
        {
            svg_ctx.magic = { 0xCDF30001, 0xCCCC0001 };
            svg_ctx.ccr = record_wrapper<cdf_CCR_t<v3x_tag>> { { {}, 0, 0, 0, {} } };
            svg_ctx.cpr = make_cpr(cdf.compression, cdf.compression_level);
        }
        svg_ctx.body.cdr.record
            = cdf_CDR_t<v3x_tag> { {}, 0, 3, 8, cdf.encoding, cdr_flags(cdf), 0, 0, 0, 2, -1, { R"(
Common Data Format (CDF)\nhttps://cdf.gsfc.nasa.gov
Space Physics Data Facility
NASA/Goddard Space Flight Center
Greenbelt, Maryland 20771 USA
(User support: gsfc-cdf-support@lists.nasa.gov)
)" } };
        svg_ctx.body.gdr.record = { {}, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0,
            chrono::leap_seconds::last_updated, { -1 }, {} };
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
            svg_ctx.ccr->record.data.reserve(svg_ctx.body.gdr.record.eof);
            buffers::vector_writer writer { svg_ctx.ccr->record.data };
            write_body(svg_ctx.body, writer, 8);
            svg_ctx.ccr->record.uSize = std::size(writer.data);
            auto compressed = compression::deflate(svg_ctx.compression, svg_ctx.compression_level,
                std::string_view { writer.data.data(), std::size(writer.data) }, 1, 1);
            svg_ctx.ccr->record.data.resize(std::size(compressed));
            std::memcpy(svg_ctx.ccr->record.data.data(), compressed.data(), std::size(compressed));
            update_size(svg_ctx.ccr.value());
            svg_ctx.cpr->offset = svg_ctx.ccr->offset + svg_ctx.ccr->size;
            svg_ctx.ccr->record.CPRoffset = svg_ctx.cpr->offset;
        }
    }


    [[nodiscard]] inline saving_context build_records(const CDF& cdf)
    {
        check_attribute_scopes(cdf);
        saving_context svg_ctx = make_saving_context(cdf);
        create_file_attributes_records(cdf, svg_ctx);
        create_variables_records(cdf, svg_ctx);
        auto eof = map_records(svg_ctx);
        link_records(svg_ctx);
        update_gdr(svg_ctx, eof);
        apply_compression(svg_ctx);
        return svg_ctx;
    }

    template <typename T>
    [[nodiscard]] bool impl_save(const CDF& cdf, T& writer)
    {
        auto svg_ctx = build_records(cdf);
        write_records(svg_ctx, writer);
        return true;
    }


} // namespace


// Every value is read and every record built before the file is opened: opening truncates it,
// and a lazily loaded CDF may still be reading its values from that very file. Writing needs
// all values anyway, so this doesn't raise peak memory.
// Returns false when the file can't be opened or written; throws std::invalid_argument, before
// opening it, when an attribute name is both global and variable (see check_attribute_scopes).
[[nodiscard]] inline bool save(const CDF& cdf, const std::string& path)
{
    for (const auto& [_, variable] : cdf.variables)
        variable.load_values();
    auto svg_ctx = saving::build_records(cdf);
    buffers::file_writer writer { path };
    if (!writer.is_open())
        return false;
    saving::write_records(svg_ctx, writer);
    writer.os.flush();
    return !writer.os.fail();
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
