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

#include <cdfpp/cdf-io/debug/nasa_compat_repr.hpp>
#include <cdfpp/cdf-io/debug/record_stream.hpp>
#include <cpp_utils/reflexion/field_name.hpp>
#include <cpp_utils/serde/serde.hpp>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <tuple>
#include <utility>

/*
 * Structured Python access to the same physical-order record walk cdfdump uses:
 * every record becomes (offset, type_name, {field_name: value}), built straight from
 * cpp_utils::reflexion::field_name - no per-record-type conversion code, mirroring how
 * record_repr.hpp prints the same records generically in C++.
 */

namespace pycdfpp::debug::details
{

namespace py = pybind11;

template <typename T>
py::object scalar_to_py(const T& value)
{
    using namespace cdf;
    if constexpr (std::is_same_v<T, cdf_encoding>)
        return py::cast(cdf_encoding_str(value));
    else if constexpr (std::is_same_v<T, cdf_attr_scope>)
        return py::cast(cdf_attr_scope_str(value));
    else if constexpr (std::is_same_v<T, CDF_Types>)
        return py::cast(std::string(cdf_type_str(value)));
    else if constexpr (std::is_same_v<T, cdf_compression_type>)
        return py::cast(cdf_compression_type_str(value));
    else if constexpr (std::is_same_v<T, cdf_record_type>)
        return py::cast(cdf_record_type_str(value));
    else
        return py::cast(value);
}

template <typename T>
py::object field_value_to_py(const T& field)
{
    if constexpr (cpp_utils::serde::unused_field<T>)
        return field_value_to_py(field.value);
    else if constexpr (cpp_utils::serde::bounded_string_field<T>)
        return py::cast(field.value);
    else if constexpr (cpp_utils::serde::dynamic_array_bytes_field<T>)
        return py::bytes(field.data(), field.size());
    else if constexpr (cpp_utils::serde::dynamic_array_field<T>)
    {
        py::list l;
        for (const auto& v : field)
            l.append(scalar_to_py(v));
        return l;
    }
    else
        return scalar_to_py(field);
}

// field 0 is always `header` (record_size/record_type) - surfaced separately as this
// record's (offset, type_name), not as a dict entry.
template <typename record_t, std::size_t I, typename field_t>
void add_one_field(py::dict& d, const field_t& field)
{
    if constexpr (I > 0)
    {
        constexpr auto name = cpp_utils::reflexion::field_name<record_t, I>;
        d[py::str(name.data(), name.size())] = field_value_to_py(field);
    }
}

template <typename record_t, typename... Fields>
py::dict fields_to_dict(const record_t&, Fields&... fields)
{
    py::dict d;
    auto as_tuple = std::tie(fields...);
    [&]<std::size_t... Is>(std::index_sequence<Is...>)
    { (add_one_field<record_t, Is>(d, std::get<Is>(as_tuple)), ...); }(
        std::index_sequence_for<Fields...> {});
    return d;
}

}

SPLIT_FIELDS(
    inline pybind11::dict, pycdfpp_debug_fields_to_dict, pycdfpp::debug::details::fields_to_dict, const);

namespace pycdfpp::debug
{

template <typename T>
void def_debug_wrapper(T& mod)
{
    namespace py = pybind11;
    mod.def(
        "debug_for_each_record",
        [](const std::string& path)
        {
            py::list result;
            cdf::io::debug::for_each_record(path,
                [&](std::size_t offset, const auto& record)
                {
                    using record_t = std::decay_t<decltype(record)>;
                    if constexpr (std::is_same_v<record_t, cdf::io::debug::undecoded_record_t>)
                    {
                        result.append(py::make_tuple(
                            offset, cdf::cdf_record_type_str(record.type), py::dict {}));
                    }
                    else
                    {
                        result.append(py::make_tuple(offset,
                            cdf::cdf_record_type_str(record.header.record_type),
                            pycdfpp_debug_fields_to_dict(record)));
                    }
                });
            return result;
        },
        py::arg("path"),
        R"pbdoc(
            Walk a CDF file's records in physical disk order (record_size stepping from
            one header to the next), not the reconstructed logical variable/attribute
            graph load() gives you. Surfaces records the semantic loader silently
            discards (UIR - freed space CDF leaves behind rather than compacting) and
            tolerates record types not yet modeled (SPR) by skipping them via their
            declared size.

            Parameters
            ----------
            path : str
                Path to the CDF file.

            Returns
            -------
            list of tuple[int, str, dict]
                One entry per on-disk record, in file order: (byte offset, record type
                name, {field_name: value}). A structurally corrupted record aborts the
                walk after printing a diagnostic to stderr (same default policy as the
                C++ API).
        )pbdoc");

    mod.def(
        "nasa_compat_dump",
        [](const std::string& path, int radix, bool show_data, bool summary)
        {
            cdf::io::debug::nasa::dump_options opts { radix, show_data };
            return cdf::io::debug::nasa::dump(path, opts, summary);
        },
        py::arg("path"), py::arg("radix") = 10, py::arg("show_data") = false,
        py::arg("summary") = false,
        R"pbdoc(
            Dump a CDF file's records in NASA's own cdfirsdump (-full -nopage) text
            format, byte-for-byte (verified against real captures of that tool's own
            output - see tests/nasa_compat_repr). Built on the same physical-order walk
            as for_each_record, not load()'s reconstructed view.

            Parameters
            ----------
            path : str
                Path to the CDF file.
            radix : int, default 10
                10 (decimal) or 16 (hex, "0x" + 16 uppercase digits) for record offsets.
            show_data : bool, default False
                Hex-dump VVR/CVVR payload bytes (cdfirsdump's -data).
            summary : bool, default False
                Append the closing record-type summary table (cdfirsdump's default
                -summary behavior; this binding defaults to False to match this
                project's own existing default, not NASA's).

            Returns
            -------
            str
                The full dump text, ready to print or write to a file.
        )pbdoc");

    mod.def(
        "nasa_compat_dump_from_offset",
        [](const std::string& path, std::size_t offset, int radix, bool show_data)
        {
            cdf::io::debug::nasa::dump_options opts { radix, show_data };
            return cdf::io::debug::nasa::dump_from_offset(path, offset, opts);
        },
        py::arg("path"), py::arg("offset"), py::arg("radix") = 10, py::arg("show_data") = false,
        R"pbdoc(
            Same as nasa_compat_dump, but starts the walk at a given byte offset instead
            of the file start - no magic-number preamble is printed (matching a real
            cdfirsdump -offset capture).

            Parameters
            ----------
            path : str
                Path to the CDF file.
            offset : int
                Byte offset to start the walk at.
            radix : int, default 10
                10 (decimal) or 16 (hex) for record offsets.
            show_data : bool, default False
                Hex-dump VVR/CVVR payload bytes.

            Returns
            -------
            str
                The dump text from that offset onward.
        )pbdoc");

    mod.def(
        "nasa_compat_dump_brief", [](const std::string& path)
        { return cdf::io::debug::nasa::dump_brief(path); }, py::arg("path"),
        R"pbdoc(
            Dump only the record-type summary table (cdfirsdump's -brief, the real
            tool's own default level), byte-for-byte, including its "(+16 if with
            checksum)" quirk which is always shown at brief level regardless of the
            file (see nasa_compat_repr.hpp's own notes on why).

            Parameters
            ----------
            path : str
                Path to the CDF file.

            Returns
            -------
            str
                The banner + summary table text.
        )pbdoc");
}

}
