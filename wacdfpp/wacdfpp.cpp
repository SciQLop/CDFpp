/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2024, Laboratory of Plasma Physics- CNRS
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is furnished to do
-- so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
-- INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
-- PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
-- HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
-- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
-- SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#include <cdfpp/cdf.hpp>
#include <cdfpp/cdf-io/saving/saving.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <cstdint>
#include <string>
#include <vector>

namespace em = emscripten;

namespace
{

em::val typed_array_view(const char* ptr, std::size_t byte_count, cdf::CDF_Types type)
{
    using enum cdf::CDF_Types;
    switch (type)
    {
        case CDF_INT1:
        case CDF_BYTE:
            return em::val(em::typed_memory_view(byte_count, reinterpret_cast<const int8_t*>(ptr)));
        case CDF_UINT1:
            return em::val(
                em::typed_memory_view(byte_count, reinterpret_cast<const uint8_t*>(ptr)));
        case CDF_INT2:
            return em::val(em::typed_memory_view(
                byte_count / 2, reinterpret_cast<const int16_t*>(ptr)));
        case CDF_UINT2:
            return em::val(em::typed_memory_view(
                byte_count / 2, reinterpret_cast<const uint16_t*>(ptr)));
        case CDF_INT4:
            return em::val(em::typed_memory_view(
                byte_count / 4, reinterpret_cast<const int32_t*>(ptr)));
        case CDF_UINT4:
            return em::val(em::typed_memory_view(
                byte_count / 4, reinterpret_cast<const uint32_t*>(ptr)));
        case CDF_INT8:
        case CDF_TIME_TT2000:
            return em::val(em::typed_memory_view(
                byte_count / 8, reinterpret_cast<const int64_t*>(ptr)));
        case CDF_FLOAT:
        case CDF_REAL4:
            return em::val(
                em::typed_memory_view(byte_count / 4, reinterpret_cast<const float*>(ptr)));
        case CDF_DOUBLE:
        case CDF_REAL8:
        case CDF_EPOCH:
            return em::val(em::typed_memory_view(
                byte_count / 8, reinterpret_cast<const double*>(ptr)));
        case CDF_EPOCH16:
            return em::val(em::typed_memory_view(
                byte_count / 8, reinterpret_cast<const double*>(ptr)));
        case CDF_CHAR:
        case CDF_UCHAR:
            return em::val(
                em::typed_memory_view(byte_count, reinterpret_cast<const uint8_t*>(ptr)));
        case CDF_NONE:
            return em::val::undefined();
    }
}

em::val data_to_string_or_view(const cdf::data_t& data)
{
    auto ptr = data.bytes_ptr();
    if (ptr == nullptr)
        return em::val::undefined();
    if (cdf::is_string(data.type()))
        return em::val(std::string(ptr, data.bytes()));
    return typed_array_view(ptr, data.bytes(), data.type());
}

em::val to_js_string_array(const auto& map)
{
    auto arr = em::val::array();
    for (const auto& [name, _] : map)
        arr.call<void>("push", name);
    return arr;
}

} // namespace


struct CdfFile
{
    std::optional<cdf::CDF> cdf;

    bool is_valid() const { return cdf.has_value(); }

    em::val variable_names() const
    {
        if (!cdf)
            return em::val::array();
        return to_js_string_array(cdf->variables);
    }

    em::val attribute_names() const
    {
        if (!cdf)
            return em::val::array();
        return to_js_string_array(cdf->attributes);
    }

    em::val get_variable(const std::string& name)
    {
        if (!cdf)
            return em::val::undefined();
        auto it = cdf->variables.find(name);
        if (it == cdf->variables.end())
            return em::val::undefined();

        auto& var = it->second;
        auto obj = em::val::object();
        obj.set("name", var.name());
        obj.set("type", static_cast<int>(var.type()));
        obj.set("type_name", std::string(cdf::cdf_type_str(var.type())));
        obj.set("is_nrv", var.is_nrv());
        obj.set("compression", std::string(cdf::cdf_compression_type_str(var.compression_type())));
        obj.set("values_loaded", var.values_loaded());

        auto& shape = var.shape();
        auto js_shape = em::val::array();
        for (std::size_t i = 0; i < std::size(shape); ++i)
            js_shape.call<void>("push", shape[i]);
        obj.set("shape", js_shape);

        obj.set("attribute_names", to_js_string_array(var.attributes));

        // Lazily provide attribute getter as a plain object
        auto attrs = em::val::object();
        for (const auto& [aname, attr] : var.attributes)
            attrs.set(aname, data_to_string_or_view(attr.value()));
        obj.set("attributes", attrs);

        // values: zero-copy typed array view into WASM memory
        var.load_values();
        auto ptr = var.bytes_ptr();
        if (ptr != nullptr)
        {
            auto byte_count = var.bytes();
            obj.set("values", typed_array_view(ptr, byte_count, var.type()));
            // copy_values: owned copy safe to keep after CdfFile is freed
            obj.set("copy_values", typed_array_view(ptr, byte_count, var.type())
                                       .call<em::val>("slice"));
        }
        else
        {
            obj.set("values", em::val::undefined());
            obj.set("copy_values", em::val::undefined());
        }

        return obj;
    }

    em::val get_attribute(const std::string& name) const
    {
        if (!cdf)
            return em::val::undefined();
        auto it = cdf->attributes.find(name);
        if (it == cdf->attributes.end())
            return em::val::undefined();

        auto& attr = it->second;
        auto obj = em::val::object();
        obj.set("name", attr.name);
        auto entries = em::val::array();
        for (std::size_t i = 0; i < attr.size(); ++i)
            entries.call<void>("push", data_to_string_or_view(attr[i]));
        obj.set("entries", entries);
        return obj;
    }

    std::string majority() const
    {
        if (!cdf)
            return "unknown";
        return cdf::cdf_majority_str(cdf->majority);
    }

    std::string compression() const
    {
        if (!cdf)
            return "unknown";
        return cdf::cdf_compression_type_str(cdf->compression);
    }

    em::val save_to_bytes() const
    {
        if (!cdf)
            return em::val::undefined();
        auto data = cdf::io::save(*cdf);
        if (std::size(data) == 0)
            return em::val::undefined();
        // Create an owned Uint8Array copy (the local vector dies after return)
        return em::val(em::typed_memory_view(std::size(data),
            reinterpret_cast<const uint8_t*>(data.data())))
            .call<em::val>("slice");
    }
};

CdfFile load_cdf(em::val js_array)
{
    CdfFile result;
    try
    {
        auto length = js_array["length"].as<std::size_t>();
        std::vector<char> buffer(length);
        em::val dest(em::typed_memory_view(length, reinterpret_cast<uint8_t*>(buffer.data())));
        dest.call<void>("set", js_array);
        result.cdf = cdf::io::load(std::move(buffer), true, true);
    }
    catch (const std::exception& e)
    {
        em::val::global("console").call<void>("error",
            std::string("CDFpp load error: ") + e.what());
    }
    catch (...)
    {
        em::val::global("console").call<void>("error",
            std::string("CDFpp load error: unknown exception"));
    }
    return result;
}


EMSCRIPTEN_BINDINGS(cdfpp)
{
    em::enum_<cdf::CDF_Types>("DataType")
        .value("CDF_NONE", cdf::CDF_Types::CDF_NONE)
        .value("CDF_INT1", cdf::CDF_Types::CDF_INT1)
        .value("CDF_INT2", cdf::CDF_Types::CDF_INT2)
        .value("CDF_INT4", cdf::CDF_Types::CDF_INT4)
        .value("CDF_INT8", cdf::CDF_Types::CDF_INT8)
        .value("CDF_UINT1", cdf::CDF_Types::CDF_UINT1)
        .value("CDF_UINT2", cdf::CDF_Types::CDF_UINT2)
        .value("CDF_UINT4", cdf::CDF_Types::CDF_UINT4)
        .value("CDF_BYTE", cdf::CDF_Types::CDF_BYTE)
        .value("CDF_FLOAT", cdf::CDF_Types::CDF_FLOAT)
        .value("CDF_REAL4", cdf::CDF_Types::CDF_REAL4)
        .value("CDF_DOUBLE", cdf::CDF_Types::CDF_DOUBLE)
        .value("CDF_REAL8", cdf::CDF_Types::CDF_REAL8)
        .value("CDF_EPOCH", cdf::CDF_Types::CDF_EPOCH)
        .value("CDF_EPOCH16", cdf::CDF_Types::CDF_EPOCH16)
        .value("CDF_TIME_TT2000", cdf::CDF_Types::CDF_TIME_TT2000)
        .value("CDF_CHAR", cdf::CDF_Types::CDF_CHAR)
        .value("CDF_UCHAR", cdf::CDF_Types::CDF_UCHAR);

    em::enum_<cdf::cdf_majority>("Majority")
        .value("row", cdf::cdf_majority::row)
        .value("column", cdf::cdf_majority::column);

    em::enum_<cdf::cdf_compression_type>("CompressionType")
        .value("none", cdf::cdf_compression_type::no_compression)
        .value("rle", cdf::cdf_compression_type::rle_compression)
        .value("huffman", cdf::cdf_compression_type::huff_compression)
        .value("adaptive_huffman", cdf::cdf_compression_type::ahuff_compression)
        .value("gzip", cdf::cdf_compression_type::gzip_compression);

    em::class_<CdfFile>("CdfFile")
        .function("is_valid", &CdfFile::is_valid)
        .function("variable_names", &CdfFile::variable_names)
        .function("attribute_names", &CdfFile::attribute_names)
        .function("get_variable", &CdfFile::get_variable)
        .function("get_attribute", &CdfFile::get_attribute)
        .function("majority", &CdfFile::majority)
        .function("compression", &CdfFile::compression)
        .function("save", &CdfFile::save_to_bytes);

    em::function("load", &load_cdf);
    em::function("type_name",
        +[](cdf::CDF_Types type) { return std::string(cdf::cdf_type_str(type)); });
    em::function("type_size", &cdf::cdf_type_size);
}
