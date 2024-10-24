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

#include "../desc-records.hpp"
#include "../endianness.hpp"
#include "../reflection.hpp"
#include "../special-fields.hpp"
#include "./buffers.hpp"
#include "cdfpp/cdf-helpers.hpp"
#include <functional>
#include <string>
#include <variant>
namespace cdf::io
{

template <typename buffer_t, typename version_t>
struct parsing_context_t
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using version_tag = version_t;
    buffer_t buffer;
    cdf_CDR_t<version_t> cdr;
    cdf_GDR_t<version_t> gdr;
    cdf_majority majority;
    cdf_compression_type compression_type;

    parsing_context_t(buffer_t&& buff, cdf_compression_type compression_type)
            : buffer { std::move(buff) }, cdr {}, gdr {}, compression_type { compression_type }
    {
    }
    inline cdf_encoding encoding() { return cdr.Encoding; }
    inline std::tuple<uint32_t, uint32_t, uint32_t> distribution_version()
    {
        return { cdr.Version, cdr.Release, cdr.Increment };
    }
};

SPLIT_FIELDS_FW_DECL(std::size_t, load_record, );

template <typename record_t, typename parsing_context_t, typename T>
inline std::size_t load_field(
    const record_t& r, parsing_context_t& parsing_context, std::size_t offset, T& field)
{
    if constexpr (is_string_field_v<T>)
    {
        std::size_t size = 0;
        for (; size < T::max_len; size++)
        {
            if (buffers::get_data_ptr(parsing_context)[offset + size] == '\0')
                break;
        }
        field.value = std::string { buffers::get_data_ptr(parsing_context) + offset, size };
        return offset + T::max_len;
    }
    else
    {
        if constexpr (is_table_field_v<T>)
        {
            const auto bytes = r.size(field);
            field.values.resize(bytes / sizeof(typename T::value_type));
            if (bytes > 0)
            {
                std::memcpy(
                    field.values.data(), buffers::get_data_ptr(parsing_context) + offset, bytes);
                cdf::endianness::decode_v<endianness::big_endian_t>(
                    field.values.data(), bytes / sizeof(typename T::value_type));
            }
            return offset + bytes;
        }
        else
        {
            field = cdf::endianness::decode<endianness::big_endian_t, T>(
                buffers::get_data_ptr(parsing_context) + offset);
            return offset + sizeof(T);
        }
    }
}
template <typename record_t, typename parsing_context_t, typename T>
inline std::size_t load_field(const record_t& r, parsing_context_t& parsing_context,
    std::size_t offset, unused_field<T>& field)
{
    (void)r;
    (void)parsing_context;
    if constexpr (is_string_field_v<T>)
    {
        return offset + T::max_len;
    }
    else
    {
        if constexpr (is_table_field_v<T>)
        {
            return offset + r.size(field.value);
        }
        else
        {
            return offset + sizeof(T);
        }
    }
}


template <typename parsing_context_t, typename version_t>
inline std::size_t load_field(const cdf_rVDR_t<version_t>& r, parsing_context_t& parsing_context,
    std::size_t offset, decltype(std::declval<cdf_rVDR_t<version_t>>().DimVarys)& field)
{
    using T = decltype(std::declval<cdf_rVDR_t<version_t>>().DimVarys);

    const auto bytes = r.size(field, parsing_context.gdr.rNumDims);
    field.values.resize(bytes / sizeof(typename T::value_type));
    if (bytes > 0)
    {
        std::memcpy(field.values.data(), buffers::get_data_ptr(parsing_context) + offset, bytes);
        cdf::endianness::decode_v<endianness::big_endian_t>(
            field.values.data(), bytes / sizeof(typename T::value_type));
    }
    return offset + bytes;
}

template <typename record_t, typename parsing_context_t, typename T>
inline std::size_t load_fields(const record_t& r, parsing_context_t& parsing_context,
    [[maybe_unused]] std::size_t offset, T&& field)
{
    using Field_t = std::remove_cv_t<std::remove_reference_t<T>>;
    static constexpr std::size_t count = count_members<Field_t>;
    if constexpr (std::is_compound_v<Field_t> && (count > 1) && (not is_string_field_v<Field_t>)
        && (not is_table_field_v<Field_t>))
        return load_record(field, parsing_context, offset);
    else
        return load_field(r, parsing_context, offset, std::forward<T>(field));
}

template <typename record_t, typename parsing_context_t, typename T, typename... Ts>
inline std::size_t load_fields(const record_t& r, parsing_context_t& parsing_context,
    [[maybe_unused]] std::size_t offset, T&& field, Ts&&... fields)
{
    offset = load_fields(r, parsing_context, offset, std::forward<T>(field));
    return load_fields(r, parsing_context, offset, std::forward<Ts>(fields)...);
}

SPLIT_FIELDS(std::size_t, load_record, load_fields, );


template <typename version_t>
struct cdf_mutable_variable_record_t
{
    inline static constexpr bool v3 = is_v3_v<version_t>;
    using vvr_t = cdf_VVR_t<version_t>;
    using cvvr_t = cdf_CVVR_t<version_t>;
    using vxr_t = cdf_VXR_t<version_t>;

    std::variant<std::monostate, vvr_t, cvvr_t, vxr_t> actual_record;

    cdf_DR_header<version_t, cdf_record_type::UIR> header;

    template <typename... Ts>
    auto visit(Ts... lambdas) const
    {
        return std::visit(helpers::make_visitor(lambdas...), actual_record);
    }
};

template <typename T, typename U>
std::size_t load_mut_record(
    cdf_mutable_variable_record_t<T>& s, const U& parsing_context, std::size_t offset)
{
    using mutable_record = cdf_mutable_variable_record_t<T>;
    load_record(s.header, parsing_context, offset);
    switch (s.header.record_type)
    {
        case cdf_record_type::CVVR:
            s.actual_record.template emplace<typename mutable_record::cvvr_t>();
            return load_record(std::get<typename mutable_record::cvvr_t>(s.actual_record),
                parsing_context, offset);
            break;
        case cdf_record_type::VVR:
            s.actual_record.template emplace<typename mutable_record::vvr_t>();
            return load_record(
                std::get<typename mutable_record::vvr_t>(s.actual_record), parsing_context, offset);
            break;
        case cdf_record_type::VXR:
            s.actual_record.template emplace<typename mutable_record::vxr_t>();
            return load_record(
                std::get<typename mutable_record::vxr_t>(s.actual_record), parsing_context, offset);
            break;
        default:
            return 0;
            break;
    }
}

template <typename value_t, typename parsing_context_t, typename... Args>
struct blk_iterator
{
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::pair<std::size_t, value_t>;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = value_type&;

    std::size_t offset;
    value_type block;
    parsing_context_t& parsing_context;
    std::function<std::size_t(value_t&)> next;
    std::tuple<Args...> load_opt_args;

    blk_iterator(std::size_t offset, parsing_context_t& parsing_context,
        std::function<std::size_t(value_t&)>&& next, Args... args)
            : offset { offset }
            , block {}
            , parsing_context { parsing_context }
            , next { std::move(next) }
            , load_opt_args { args... }
    {
        if (offset != 0)
            wrapper_load(offset, std::make_index_sequence<sizeof...(Args)> {});
    }

    auto operator==(const blk_iterator& other) { return other.offset == offset; }

    bool operator!=(const blk_iterator& other) { return not(*this == other); }

    blk_iterator& operator+(int n)
    {
        step_forward(n);
        return *this;
    }

    blk_iterator& operator+=(int n)
    {
        step_forward(n);
        return *this;
    }

    blk_iterator& operator++(int n)
    {
        step_forward(n);
        return *this;
    }

    blk_iterator& operator++()
    {
        step_forward();
        return *this;
    }

    template <size_t... Is>
    std::size_t wrapper_load(std::size_t offset, std::index_sequence<Is...>)
    {
        block.first = offset;
        return load_record(block.second, parsing_context, offset, std::get<Is>(load_opt_args)...);
    }

    void step_forward(int n = 1)
    {
        while (n > 0)
        {
            n--;
            offset = next(block.second);
            if (offset != 0)
            {
                wrapper_load(offset, std::make_index_sequence<sizeof...(Args)> {});
            }
        }
    }

    const value_type* operator->() const { return &block; }
    const value_type& operator*() const { return block; }
    value_type* operator->() { return &block; }
    value_type& operator*() { return block; }
};

template <typename parsing_context_t>
auto begin_ADR(parsing_context_t& parsing_context)
{
    using adr_t = cdf_ADR_t<typename parsing_context_t::version_tag>;
    return blk_iterator<adr_t, parsing_context_t> { static_cast<std::size_t>(
                                                        parsing_context.gdr.ADRhead),
        parsing_context, [](const adr_t& adr) { return adr.ADRnext; } };
}

template <typename parsing_context_t>
auto end_ADR(parsing_context_t& parsing_context)
{
    using adr_t = cdf_ADR_t<typename parsing_context_t::version_tag>;
    return blk_iterator<adr_t, parsing_context_t> { 0, parsing_context,
        [](const auto& adr) -> decltype(adr.ADRnext) { return 0; } };
}

template <typename version_t, typename buffer_t>
auto begin_AgrEDR(const cdf_ADR_t<version_t>& adr, buffer_t& buffer)
{
    using aedr_t = cdf_AgrEDR_t<version_t>;

    return blk_iterator<aedr_t, buffer_t> { static_cast<std::size_t>(adr.AgrEDRhead), buffer,
        [](const aedr_t& aedr) { return aedr.AEDRnext; } };
}

template <typename version_t, typename buffer_t>
auto end_AgrEDR(const cdf_ADR_t<version_t>&, buffer_t& buffer)
{
    return blk_iterator<cdf_AgrEDR_t<version_t>, buffer_t> { 0, buffer,
        [](const auto& aedr) -> decltype(aedr.AEDRnext) { return 0; } };
}

template <typename version_t, typename buffer_t>
auto begin_AzEDR(const cdf_ADR_t<version_t>& adr, buffer_t& buffer)
{
    using aedr_t = cdf_AzEDR_t<version_t>;

    return blk_iterator<aedr_t, buffer_t> { static_cast<std::size_t>(adr.AzEDRhead), buffer,
        [](const aedr_t& aedr) { return aedr.AEDRnext; } };
}


template <typename version_t, typename buffer_t>
auto end_AzEDR(const cdf_ADR_t<version_t>&, buffer_t& buffer)
{
    return blk_iterator<cdf_AzEDR_t<version_t>, buffer_t> { 0, buffer,
        [](const auto& aedr) -> decltype(aedr.AEDRnext) { return 0; } };
}

template <cdf_r_z type, typename version_t, typename buffer_t>
auto begin_AEDR(const cdf_ADR_t<version_t>& adr, buffer_t& buffer)
{
    if constexpr (type == cdf_r_z::r)
        return begin_AgrEDR(adr, buffer);
    else
        return begin_AzEDR(adr, buffer);
}


template <cdf_r_z type, typename version_t, typename buffer_t>
auto end_AEDR(const cdf_ADR_t<version_t>& adr, buffer_t& buffer)
{
    if constexpr (type == cdf_r_z::r)
        return end_AgrEDR(adr, buffer);
    else
        return end_AzEDR(adr, buffer);
}


template <cdf_r_z type, typename parsing_context_t>
auto begin_VDR(parsing_context_t& parsing_context)
{
    using version_t = typename parsing_context_t::version_tag;
    if constexpr (type == cdf_r_z::r)
    {
        using vdr_t = cdf_rVDR_t<version_t>;
        return blk_iterator<vdr_t, parsing_context_t> { static_cast<std::size_t>(
                                                            parsing_context.gdr.rVDRhead),
            parsing_context, [](const vdr_t& vdr) { return vdr.VDRnext; } };
    }
    else if constexpr (type == cdf_r_z::z)
    {
        using vdr_t = cdf_zVDR_t<version_t>;
        return blk_iterator<vdr_t, parsing_context_t> { static_cast<std::size_t>(
                                                            parsing_context.gdr.zVDRhead),
            parsing_context, [](const vdr_t& vdr) { return vdr.VDRnext; } };
    }
}

template <cdf_r_z type, typename parsing_context_t>
auto end_VDR(parsing_context_t& parsing_context)
{
    using version_t = typename parsing_context_t::version_tag;
    if constexpr (type == cdf_r_z::r)
    {
        using vdr_t = cdf_rVDR_t<version_t>;
        return blk_iterator<vdr_t, parsing_context_t> { 0, parsing_context,
            [](const auto& vdr) -> decltype(vdr.VDRnext) { return 0; } };
    }
    else if constexpr (type == cdf_r_z::z)
    {
        using vdr_t = cdf_zVDR_t<version_t>;
        return blk_iterator<vdr_t, parsing_context_t> { 0, parsing_context,
            [](const auto& vdr) -> decltype(vdr.VDRnext) { return 0; } };
    }
}

template <typename vdr_t, typename version_t, typename buffer_t>
auto begin_VXR(const vdr_t& vdr, buffer_t& buffer)
{
    using vxr_t = cdf_VXR_t<version_t>;
    return blk_iterator<vxr_t, buffer_t> { vdr.VXRhead, buffer,
        [](const vxr_t& vxr) { return vxr.VXRnext; } };
}

template <typename vdr_t, typename version_t, typename buffer_t>
auto end_VXR(const vdr_t&, buffer_t& buffer)
{
    return blk_iterator<cdf_VXR_t<version_t>, buffer_t> { 0, buffer,
        []([[maybe_unused]] const auto& vxr) { return 0; } };
}

} // namespace cdf::io
