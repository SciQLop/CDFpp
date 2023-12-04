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
#pragma once

#include "../common.hpp"
#include "../desc-records.hpp"
#include "../endianness.hpp"
#include "../reflection.hpp"
#include "../special-fields.hpp"
#include "cdfpp/attribute.hpp"
#include "cdfpp/cdf-helpers.hpp"
#include "cdfpp/variable.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <utility>

namespace cdf::io
{

SPLIT_FIELDS_FW_DECL([[nodiscard]] constexpr std::size_t, record_size, const);


template <typename T>
struct record_wrapper
{
    T record;
    std::size_t size;
    std::size_t offset;
    record_wrapper(T&& r) : record { std::move(r) }, size { 0 }, offset { 0 } { }

    template <typename... Args>
    record_wrapper(Args... args) : record { std::forward<Args>(args)... }, size { 0 }, offset { 0 }
    {
    }
};

template <typename T>
void update_size(record_wrapper<T>& record, std::size_t size_offset = 0)
{
    record.size = record_size(record.record) + size_offset;
    record.record.header.record_size = record.size;
}


void update_size(record_wrapper<cdf_CCR_t<v3x_tag>>& record, std::size_t size_offset = 0)
{
    record.size = std::size(record.record.data.values) + record_size(record.record.header)
        + sizeof(record.record.uSize) + sizeof(record.record.CPRoffset) + sizeof(record.record.rfuA)
        + size_offset;
    record.record.header.record_size = record.size;
}

void update_size(record_wrapper<cdf_CVVR_t<v3x_tag>>& record, std::size_t size_offset = 0)
{
    record.size = std::size(record.record.data.values) + record_size(record.record.header)
        + sizeof(record.record.rfuA) + sizeof(record.record.cSize) + size_offset;
    record.record.header.record_size = record.size;
}

struct file_attribute_ctx
{
    int32_t number;
    const Attribute* attr;
    record_wrapper<cdf_ADR_t<v3x_tag>> adr;
    std::vector<record_wrapper<cdf_AgrEDR_t<v3x_tag>>> aedrs;
};

struct variable_attribute_ctx
{
    int32_t number;
    std::vector<const VariableAttribute*> attrs;
    record_wrapper<cdf_ADR_t<v3x_tag>> adr;
    std::vector<record_wrapper<cdf_AzEDR_t<v3x_tag>>> aedrs;
};

struct variable_ctx
{
    cdf_compression_type compression = cdf_compression_type::no_compression;
    using values_records_t
        = std::variant<record_wrapper<cdf_VVR_t<v3x_tag>>, record_wrapper<cdf_CVVR_t<v3x_tag>>>;
    int32_t number;
    const Variable* variable;
    record_wrapper<cdf_zVDR_t<v3x_tag>> vdr;
    std::vector<record_wrapper<cdf_VXR_t<v3x_tag>>> vxrs;
    std::vector<values_records_t> values_records;
    std::optional<record_wrapper<cdf_CPR_t<v3x_tag>>> cpr = std::nullopt;
};

template <typename... Ts>
auto visit(const variable_ctx::values_records_t& values_records, Ts... lambdas)
{
    return std::visit(helpers::make_visitor(lambdas...), values_records);
}

template <typename... Ts>
auto visit(variable_ctx::values_records_t& values_records, Ts... lambdas)
{
    return std::visit(helpers::make_visitor(lambdas...), values_records);
}

struct cdf_body
{
    record_wrapper<cdf_CDR_t<v3x_tag>> cdr;
    record_wrapper<cdf_GDR_t<v3x_tag>> gdr;
    std::vector<file_attribute_ctx> file_attributes;
    nomap<std::string, variable_attribute_ctx> variable_attributes;
    std::vector<variable_ctx> variables;
};

struct saving_context
{
    cdf_compression_type compression = cdf_compression_type::no_compression;
    common::magic_numbers_t magic;
    std::optional<record_wrapper<cdf_CCR_t<v3x_tag>>> ccr;
    std::optional<record_wrapper<cdf_CPR_t<v3x_tag>>> cpr;
    cdf_body body;
};


template <typename record_t, typename T>
[[nodiscard]] constexpr std::size_t field_size(const record_t& s, T& field)
{
    if constexpr (is_string_field_v<T>)
    {
        return T::max_len;
    }
    else
    {
        if constexpr (is_table_field_v<T>)
        {
            return s.size(field);
        }
        else
        {
            return sizeof(T);
        }
    }
}

template <typename record_t, typename T>
[[nodiscard]] constexpr inline std::size_t fields_size(const record_t& s, T&& field)
{
    using Field_t = std::remove_cv_t<std::remove_reference_t<T>>;
    constexpr std::size_t count = count_members<Field_t>;
    if constexpr (std::is_compound_v<Field_t> && (count > 1)
        && (not is_string_field_v<Field_t>)&&(not is_table_field_v<Field_t>))
        return record_size(field);
    else
        return field_size(s, std::forward<T>(field));
}

template <typename record_t, typename T, typename... Ts>
[[nodiscard]] constexpr inline std::size_t fields_size(const record_t& s, T&& field, Ts&&... fields)
{
    return fields_size(s, std::forward<T>(field)) + fields_size(s, std::forward<Ts>(fields)...);
}

SPLIT_FIELDS([[nodiscard]] constexpr std::size_t, record_size, fields_size, const);

template <typename T, typename U>
std::size_t save_record(const T& s, U& writer);

template <typename writer_t, typename T>
inline std::size_t save_field(writer_t& writer, const T& field)
{
    using Field_t = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (is_string_field_v<T>)
    {
        writer.write(field.value.data(), field.value.length());
        return writer.fill('\0', T::max_len - field.value.length());
    }
    else
    {
        if constexpr (is_table_field_v<T>)
        {
            if constexpr (sizeof(typename Field_t::value_type) == 1)
            {
                writer.write(
                    reinterpret_cast<const char*>(field.values.data()), std::size(field.values));
            }
            else
            {
                for (auto& v : field.values)
                {
                    auto rv = endianness::decode<endianness::big_endian_t,
                        typename Field_t::value_type>(&v);
                    writer.write(
                        reinterpret_cast<const char*>(&rv), sizeof(typename Field_t::value_type));
                }
            }

            return writer.offset();
        }
        else
        {
            auto v = endianness::decode<endianness::big_endian_t, Field_t>(&field);
            return writer.write(reinterpret_cast<char*>(&v), sizeof(Field_t));
        }
    }
}

template <typename strurt_t, cdf_record_type record_type, typename U>
std::size_t save_header(const strurt_t& s, const cdf_DR_header<v3x_tag, record_type>& h, U& writer)
{
    save_field(
        writer, std::max(static_cast<decltype(h.record_size)>(record_size(s)), h.record_size));
    return save_field(writer, record_type);
}


template <typename strurt_t, typename writer_t, typename T>
inline std::size_t save_fields(const strurt_t& s, writer_t& writer, const T& field)
{
    using Field_t = std::remove_cv_t<std::remove_reference_t<T>>;
    static constexpr std::size_t count = count_members<Field_t>;
    if constexpr (is_cdf_DR_header_v<Field_t>)
        return save_header(s, field, writer);
    else if constexpr (std::is_compound_v<Field_t> && (count > 1)
        && (not is_string_field_v<Field_t>)&&(not is_table_field_v<Field_t>))
        return save_record(field, writer);
    else
        return save_field(writer, field);
}

template <typename strurt_t, typename writer_t, typename T, typename... Ts>
inline std::size_t save_fields(
    const strurt_t& s, writer_t& writer, const T& field, const Ts&... fields)
{
    save_fields(s, writer, field);
    return save_fields(s, writer, fields...);
}


SPLIT_FIELDS(std::size_t, _save_record, save_fields, const);

template <typename T, typename U>
std::size_t save_record(const T& s, U& writer)
{
    return _save_record(s, writer);
}

template <typename U>
[[nodiscard]] std::size_t save_record(
    const cdf_VVR_t<v3x_tag>& s, const char* data, std::size_t len, U& writer)
{
    save_field(writer, record_size(s.header) + len);
    save_field(writer, cdf_record_type::VVR);
    return writer.write(data, len);
}

}
