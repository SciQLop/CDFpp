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
#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf-map.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
#include <chrono>
#include <fmt/core.h>
#include <string>
using namespace cdf;

template <class stream_t>
stream_t& operator<<(stream_t& os, const cdf::data_t& data);

template <typename collection_t>
constexpr bool is_char_like_v
    = std::is_same_v<int8_t,
          std::remove_cv_t<
              std::remove_reference_t<decltype(*std::cbegin(std::declval<collection_t>()))>>>
    or std::is_same_v<uint8_t,
        std::remove_cv_t<
            std::remove_reference_t<decltype(*std::cbegin(std::declval<collection_t>()))>>>;


namespace _repr_details
{
    // Not std::gmtime or fmt's chrono formatter: they delegate to the platform's gmtime, and the
    // Windows CRT rejects negative time_t, so pre-1970 values (routine in VALIDMIN/FILLVAL and
    // old mission data) crashed repr()/str() there. cdf::civil_from_days is pure arithmetic.
    template <class stream_t>
    inline stream_t& print_iso(stream_t& os, const cdf::utc_time& time)
    {
        using cdf::chrono::_impl::floor_div, cdf::chrono::_impl::floor_mod;
        const int64_t second_of_day = floor_mod(time.seconds, 86400);
        const auto date = cdf::civil_from_days(floor_div(time.seconds, 86400));
        os << fmt::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:09}", date.year, date.month,
            date.day, second_of_day / 3600, (second_of_day / 60) % 60, second_of_day % 60,
            time.nanoseconds);
        return os;
    }

    // NASA's library prints fill values as 9999-12-31 and pad values as 0000-01-01. NaN,
    // infinities and illegal TT2000 values print like fill values: they hold no date either.
    template <class stream_t>
    inline stream_t& print_time(stream_t& os, const cdf::utc_time& time, const char* fill,
        const char* pad)
    {
        switch (time.kind)
        {
            case cdf::time_kind::date:
                return print_iso(os, time);
            case cdf::time_kind::pad:
                os << pad;
                return os;
            default:
                os << fill;
                return os;
        }
    }
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const decltype(cdf::to_time_point(tt2000_t {}))& tp)
{
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch());
    return _repr_details::print_iso(os, cdf::chrono::_impl::utc_from_ns(0, ns.count()));
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const epoch& time)
{
    return _repr_details::print_time(
        os, cdf::to_utc_time(time), "9999-12-31T23:59:59.999", "0000-01-01T00:00:00.000");
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const epoch16& time)
{
    return _repr_details::print_time(os, cdf::to_utc_time(time),
        "9999-12-31T23:59:59.999999999", "0000-01-01T00:00:00.000000000000");
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const tt2000_t& time)
{
    return _repr_details::print_time(os, cdf::to_utc_time(time),
        "9999-12-31T23:59:59.999999999", "0000-01-01T00:00:00.000000000");
}

template <class stream_t, class input_t, class item_t>
inline auto stream_collection(
    stream_t& os, const input_t& input, const item_t& sep) -> decltype(input.back(), os)
{
    os << "[ ";
    if (std::size(input))
    {
        if (std::size(input) > 1)
        {
            if constexpr (is_char_like_v<input_t>)
            {
                std::for_each(std::cbegin(input), --std::cend(input),
                    [&sep, &os](const auto& item) { os << int { item } << sep; });
            }
            else
            {
                std::for_each(std::cbegin(input), --std::cend(input),
                    [&sep, &os](const auto& item) { os << item << sep; });
            }
        }
        if constexpr (is_char_like_v<input_t>)
        {
            os << int { input.back() };
        }
        else
        {
            os << input.back();
        }
    }
    os << " ]";
    return os;
}

template <class stream_t, class input_t>
inline stream_t& stream_string_like(stream_t& os, const input_t& input)
{
    os << "\"";
    os << ensure_utf8<std::string>(reinterpret_cast<const char*>(input.data()), std::size(input));
    os << "\"";
    return os;
}

template <class stream_t>
stream_t& operator<<(stream_t& os, const cdf::data_t& data)
{
    cdf_type_dispatch(data.type(),
        []<cdf::CDF_Types T>(stream_t& os, const cdf::data_t& data)
        {
            if constexpr (is_cdf_string_type(T))
            {
                stream_string_like(os, data.get<T>());
            }
            else
            {
                stream_collection(os, data.get<T>(), ", ");
            }
        },
        os, data);
    return os;
}


template <class stream_t>
inline stream_t& operator<<(stream_t& os, const cdf_majority& majority)
{
    os << fmt::format("majority: {}", cdf_majority_str(majority));
    return os;
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const cdf_compression_type& compression)
{
    os << fmt::format("compression: {}", cdf_compression_type_str(compression));
    return os;
}

struct indent_t
{
    int n = 0;
    char c = ' ';
    indent_t operator+(int v) const { return indent_t { n + v, c }; }
};

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const indent_t& ind)
{
    for (int i = 0; i < ind.n; i++)
        os << ind.c;
    return os;
}
