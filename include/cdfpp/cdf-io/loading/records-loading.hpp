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
#include "./buffers.hpp"
#include "cdfpp/cdf-helpers.hpp"
#include <cpp_utils/serde/serde.hpp>
#include <functional>
#include <string>
#include <utility>
#include <variant>
namespace cdf::io
{

// Readers don't need the level to decompress, so an out of range one is replaced, not an error.
template <typename cpr_t>
[[nodiscard]] int32_t gzip_level(const cpr_t& cpr)
{
    if (cpr.cType == cdf_compression_type::gzip_compression && std::size(cpr.cParms) >= 1
        && cpr.cParms[0] >= 1 && cpr.cParms[0] <= 9)
        return cpr.cParms[0];
    return default_gzip_level;
}

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
    int32_t compression_level = default_gzip_level;

    parsing_context_t(buffer_t&& buff, cdf_compression_type compression_type)
            : buffer { std::move(buff) }, cdr {}, gdr {}, compression_type { compression_type }
    {
    }
    inline cdf_encoding encoding() { return cdr.Encoding; }
    inline std::tuple<int32_t, int32_t, int32_t> distribution_version()
    {
        return { cdr.Version, cdr.Release, cdr.Increment };
    }
};

// Called with a raw buffer/stream directly — records with no external-context need
// (CDR/GDR/CCR/CPR at file scope), or the unit tests exercising a buffer container
// directly. buffer_t&& (forwarding reference) rather than buffer_t&: it's simply the
// more general choice, binding both lvalue containers (as every call site does today)
// and any rvalue that might be passed, without needing two overloads.
//
// The explicit cpp_utils::serde::no_context{} is load-bearing, not decorative:
// cpp_utils::serde::deserialize's mutating (SPLIT_FIELDS-generated) overload has no
// default for its context parameter — unlike the value-returning convenience
// overload — so omitting it doesn't fall back to no_context, it silently shifts
// every field binding by one position (the first decomposed field gets bound to the
// context slot instead of being read at all). Confirmed by a standalone repro
// against the real cpp_utils headers before writing this comment.
template <typename record_t, typename buffer_t>
inline std::size_t load_record(record_t& r, buffer_t&& buffer, std::size_t offset)
{
    return cpp_utils::serde::deserialize(
        r, std::forward<buffer_t>(buffer), offset, cpp_utils::serde::no_context {});
}

// Called with the full parsing_context_t (as begin_ADR/begin_VDR/etc. do) for a
// record type that needs no external context — unwrap to the byte buffer.
template <typename record_t, typename buffer_t, typename version_t>
inline std::size_t load_record(
    record_t& r, parsing_context_t<buffer_t, version_t>& parsing_context, std::size_t offset)
{
    return cpp_utils::serde::deserialize(
        r, parsing_context.buffer, offset, cpp_utils::serde::no_context {});
}

// cdf_rVDR_t specialization: DimVarys needs the GDR's rNumDims, external to the
// record itself — thread it through as context.
template <typename version_t, typename buffer_t>
inline std::size_t load_record(cdf_rVDR_t<version_t>& r,
    parsing_context_t<buffer_t, version_t>& parsing_context, std::size_t offset)
{
    return cpp_utils::serde::deserialize(r, parsing_context.buffer, offset,
        vdr_context_t<version_t> { parsing_context.gdr.rNumDims });
}

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
        return std::visit(helpers::Visitor { lambdas... }, actual_record);
    }
};

template <typename T, typename U>
std::size_t load_mut_record(
    cdf_mutable_variable_record_t<T>& s, const U& parsing_context, std::size_t offset)
{
    using mutable_record = cdf_mutable_variable_record_t<T>;
    using enum cdf_record_type;
    load_record(s.header, parsing_context, offset);
    switch (s.header.record_type)
    {
        case CVVR:
            s.actual_record.template emplace<typename mutable_record::cvvr_t>();
            return load_record(std::get<typename mutable_record::cvvr_t>(s.actual_record),
                parsing_context, offset);
        case VVR:
            s.actual_record.template emplace<typename mutable_record::vvr_t>();
            return load_record(
                std::get<typename mutable_record::vvr_t>(s.actual_record), parsing_context, offset);
        case VXR:
            s.actual_record.template emplace<typename mutable_record::vxr_t>();
            return load_record(
                std::get<typename mutable_record::vxr_t>(s.actual_record), parsing_context, offset);
        default:
            return 0;
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

    bool operator==(const blk_iterator& other) const { return other.offset == offset; }

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

    blk_iterator& operator++(int)
    {
        step_forward();
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
