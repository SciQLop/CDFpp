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
    // Proleptic-Gregorian y/m/d from a day count relative to 1970-01-01 (z may be
    // negative) - Howard Hinnant's civil_from_days algorithm, pure integer
    // arithmetic: http://howardhinnant.github.io/date_algorithms.html
    //
    // Why not std::gmtime/fmt's chrono formatter: both delegate to the platform's
    // gmtime_r (glibc, accepts virtually any time_t) or gmtime_s (Windows CRT, only
    // 1970-01-01..3000-12-31, rejects negative time_t outright). Any pre-1970 CDF
    // epoch/epoch16/tt2000 value - routine for VALIDMIN/FILLVAL metadata and for
    // pre-space-age mission data - crashed repr()/str() on Windows
    // (fmt::format_error("time_t value out of range")) while printing fine on Linux.
    // This computation never touches libc, so the output is identical on every
    // platform for any value the surrounding nanosecond-precision time_point can hold.
    inline void civil_from_days(int64_t z, int64_t& y, unsigned& m, unsigned& d) noexcept
    {
        z += 719468;
        const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        const unsigned doe = static_cast<unsigned>(z - era * 146097); // [0, 146096]
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
        const unsigned mp = (5 * doy + 2) / 153; // [0, 11]
        d = doy - (153 * mp + 2) / 5 + 1; // [1, 31]
        m = mp < 10 ? mp + 3 : mp - 9; // [1, 12]
        y = static_cast<int64_t>(yoe) + era * 400 + (m <= 2 ? 1 : 0);
    }

    template <class stream_t, class duration_t>
    inline stream_t& print_iso(
        stream_t& os, const std::chrono::time_point<std::chrono::system_clock, duration_t>& tp)
    {
        constexpr int64_t ns_per_day = 86400LL * 1'000'000'000LL;
        const int64_t total_ns
            = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
        int64_t days = total_ns / ns_per_day;
        int64_t ns_of_day = total_ns % ns_per_day;
        if (ns_of_day < 0) // floored division: keep the time-of-day non-negative
        {
            ns_of_day += ns_per_day;
            days -= 1;
        }
        int64_t y;
        unsigned mo, d;
        civil_from_days(days, y, mo, d);
        os << fmt::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:09}", y, mo, d,
            ns_of_day / 3'600'000'000'000LL, (ns_of_day / 60'000'000'000LL) % 60,
            (ns_of_day / 1'000'000'000LL) % 60, ns_of_day % 1'000'000'000LL);
        return os;
    }
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const decltype(cdf::to_time_point(tt2000_t {}))& tp)
{
    return _repr_details::print_iso(os, tp);
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const epoch& time)
{
    if (time.mseconds == -1e31)
    {
        os << "9999-12-31T23:59:59.999";
        return os;
    }
    if (time.mseconds == 0)
    {
        os << "0000-01-01T00:00:00.000";
        return os;
    }
    return _repr_details::print_iso(os, cdf::to_time_point(time));
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const epoch16& time)
{
    if (time.seconds == -1e31 && time.picoseconds == -1e31)
    {
        os << "9999-12-31T23:59:59.999999999";
        return os;
    }
    if (time.seconds == 0 && time.picoseconds == 0)
    {
        os << "0000-01-01T00:00:00.000000000000";
        return os;
    }
    return _repr_details::print_iso(os, cdf::to_time_point(time));
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const tt2000_t& time)
{
    if (time.nseconds == static_cast<int64_t>(0x8000000000000000))
    {
        os << "9999-12-31T23:59:59.999999999";
        return os;
    }
    if (time.nseconds == static_cast<int64_t>(0x8000000000000001))
    {
        os << "0000-01-01T00:00:00.000000000";
        return os;
    }
    if (time.nseconds == static_cast<int64_t>(0x8000000000000003))
    {
        os << "9999-12-31T23:59:59.999999999";
        return os;
    }
    return _repr_details::print_iso(os, cdf::to_time_point(time));
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
