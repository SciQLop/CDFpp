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
#include "../desc-records.hpp"
#include "cdfpp/attribute.hpp"
#include "cdfpp/cdf-helpers.hpp"
#include "cdfpp/variable.hpp"
#include <cpp_utils/serde/serde.hpp>
#include <algorithm>
#include <optional>
#include <utility>

namespace cdf::io
{

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

// NOTE: setting header.record_type here is load-bearing, not defensive. The old
// hand-rolled save engine wrote a record's on-disk record_type byte from the
// compile-time cdf_DR_header<version_t,record_t>::expected_record_type template
// parameter, never from this runtime member — so nothing ever needed to set it.
// cpp_utils::serde::serialize() has no equivalent special case: it writes whatever
// is in this runtime member as plain struct data. Every record must therefore reach
// its first save() with the correct record_type already in place, and this function
// runs on every record before it's ever saved.
template <typename T>
void update_size(record_wrapper<T>& record, std::size_t size_offset = 0)
{
    record.size = cpp_utils::serde::runtime_size(record.record) + size_offset;
    record.record.header.record_size = record.size;
    record.record.header.record_type = decltype(record.record.header)::expected_record_type;
}


inline void update_size(record_wrapper<cdf_CCR_t<v3x_tag>>& record, std::size_t size_offset = 0)
{
    record.size = std::size(record.record.data)
        + cpp_utils::serde::runtime_size(record.record.header) + sizeof(record.record.uSize)
        + sizeof(record.record.CPRoffset) + sizeof(record.record.rfuA) + size_offset;
    record.record.header.record_size = record.size;
    record.record.header.record_type = decltype(record.record.header)::expected_record_type;
}

inline void update_size(record_wrapper<cdf_CVVR_t<v3x_tag>>& record, std::size_t size_offset = 0)
{
    record.size = std::size(record.record.data)
        + cpp_utils::serde::runtime_size(record.record.header) + sizeof(record.record.rfuA)
        + sizeof(record.record.cSize) + size_offset;
    record.record.header.record_size = record.size;
    record.record.header.record_type = decltype(record.record.header)::expected_record_type;
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
    return std::visit(helpers::Visitor { lambdas... }, values_records);
}

template <typename... Ts>
auto visit(variable_ctx::values_records_t& values_records, Ts... lambdas)
{
    return std::visit(helpers::Visitor { lambdas... }, values_records);
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

template <typename T>
[[nodiscard]] constexpr std::size_t record_size(const T& s)
{
    return cpp_utils::serde::runtime_size(s);
}

template <typename T, typename writer_t>
std::size_t save_record(const T& s, writer_t& writer)
{
    return cpp_utils::serde::serialize(s, writer);
}

template <typename writer_t>
[[nodiscard]] std::size_t save_record(
    const cdf_VVR_t<v3x_tag>& s, const char* data, std::size_t len, writer_t& writer)
{
    cpp_utils::serde::serialize(s.header, writer);
    return writer.write(data, len);
}

}
