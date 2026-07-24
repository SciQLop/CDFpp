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

#include "../../cdf-enums.hpp"
#include "record_stream.hpp"
#include <cpp_utils/reflexion/field_name.hpp>
#include <cpp_utils/serde/serde.hpp>
#include <cstddef>
#include <ostream>
#include <tuple>
#include <utility>

/*
 * Generic, per-type-printer-free record dump: every field of every record type is
 * printed as "Name: value" purely from cpp_utils::reflexion::field_name - the same
 * mechanism cpp_utils::serde already uses to (de)serialize these exact structs, so no
 * record type needs its own hand-written printer. Only a handful of *field kinds* (not
 * record types) need special-casing: reserved (unused<T>) fields, bounded strings,
 * variable-length arrays, and the header field itself (printed once, up front, instead
 * of as an opaque nested sub-object - every record type here has it as field 0).
 */

namespace cdf::io::debug::details
{
    template <typename T>
    void print_scalar(std::ostream& os, const T& value)
    {
        if constexpr (std::is_same_v<T, cdf_encoding>)
            os << cdf_encoding_str(value);
        else if constexpr (std::is_same_v<T, cdf_attr_scope>)
            os << cdf_attr_scope_str(value);
        else if constexpr (std::is_same_v<T, CDF_Types>)
            os << cdf_type_str(value);
        else if constexpr (std::is_same_v<T, cdf_compression_type>)
            os << cdf_compression_type_str(value);
        else if constexpr (std::is_same_v<T, cdf_record_type>)
            os << cdf_record_type_str(value);
        else
            os << value;
    }

    template <typename T>
    void print_field_value(std::ostream& os, const T& field)
    {
        if constexpr (cpp_utils::serde::unused_field<T>)
        {
            print_scalar(os, field.value);
            os << " (reserved)";
        }
        else if constexpr (cpp_utils::serde::bounded_string_field<T>)
        {
            os << '"' << field.value << '"';
        }
        else if constexpr (cpp_utils::serde::dynamic_array_bytes_field<T>)
        {
            os << '<' << field.size() << " bytes>";
        }
        else if constexpr (cpp_utils::serde::dynamic_array_field<T>)
        {
            os << '[';
            bool first = true;
            for (const auto& v : field)
            {
                if (!first)
                    os << ", ";
                print_scalar(os, v);
                first = false;
            }
            os << ']';
        }
        else
        {
            print_scalar(os, field);
        }
    }

    // field 0 is always `header` (record_size/record_type), already printed by
    // print_record's own header line - skip it here rather than recursing into it as
    // a nested composite.
    template <typename record_t, std::size_t I, typename field_t>
    void print_one_named_field(std::ostream& os, const field_t& field)
    {
        if constexpr (I > 0)
        {
            os << "  " << cpp_utils::reflexion::field_name<record_t, I> << ": ";
            print_field_value(os, field);
            os << '\n';
        }
    }

    template <typename record_t, typename... Fields>
    void print_named_fields(const record_t&, std::ostream& os, Fields&... fields)
    {
        auto as_tuple = std::tie(fields...);
        [&]<std::size_t... Is>(std::index_sequence<Is...>)
        {
            (print_one_named_field<record_t, Is>(os, std::get<Is>(as_tuple)), ...);
        }(std::index_sequence_for<Fields...> {});
    }
}

SPLIT_FIELDS(inline void, cdf_io_debug_print_named_fields,
    cdf::io::debug::details::print_named_fields, const);

namespace cdf::io::debug
{

template <typename record_t>
void print_record(std::ostream& os, std::size_t offset, const record_t& r)
{
    os << '@' << offset << ' ' << cdf_record_type_str(r.header.record_type)
       << " (record_size=" << r.header.record_size << ")\n";
    cdf_io_debug_print_named_fields(r, os);
}

inline void print_record(std::ostream& os, std::size_t offset, const undecoded_record_t& r)
{
    os << '@' << offset << ' ' << cdf_record_type_str(r.type) << " (record_size=" << r.record_size
       << ") [not decoded]\n";
}

} // namespace cdf::io::debug
