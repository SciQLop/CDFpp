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
#include <ctime>
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <iomanip>
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


template <class stream_t>
inline stream_t& operator<<(stream_t& os, const decltype(cdf::to_time_point(tt2000_t {}))& tp)
{
    const auto tp_time_t = std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::time_point::duration>(tp));
    auto tt = std::gmtime(&tp_time_t);
    if (tt)
    {
        const uint64_t ns
            = (std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch())
                % 1000000000)
                  .count();
        // clang-format off
        os << std::setprecision(4) << std::setw(4) << std::setfill('0')  << tt->tm_year + 1900 << '-'
           << std::setprecision(2) << std::setw(2) << std::setfill('0')  << tt->tm_mon + 1     << '-'
           << std::setprecision(2) << std::setw(2) << std::setfill('0')  << tt->tm_mday        << 'T'
           << std::setprecision(2) << std::setw(2) << std::setfill('0')  << tt->tm_hour        << ':'
           << std::setprecision(2) << std::setw(2) << std::setfill('0')  << tt->tm_min         << ':'
           << std::setprecision(2) << std::setw(2) << std::setfill('0')  << tt->tm_sec         << '.'
           << std::setprecision(9) << std::setw(9) << std::setfill('0')  << ns;
        // clang-format on
    }

    return os;
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
    os <<  fmt::format("{:%Y-%m-%dT%H:%M:%S}", cdf::to_time_point(time));
    return os;
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
    os << fmt::format("{:%Y-%m-%dT%H:%M:%S}", cdf::to_time_point(time));
    return os;
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
    os << fmt::format("{:%Y-%m-%dT%H:%M:%S}", cdf::to_time_point(time));
    return os;
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
    cdf_type_dipatch(data.type(),
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
