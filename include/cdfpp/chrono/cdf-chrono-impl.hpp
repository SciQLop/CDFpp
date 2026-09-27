/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2026, Laboratory of Plasma Physics- CNRS
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

#include "cdf-chrono-constants.hpp"
#include "cdf-leap-seconds.h"
#include "cdfpp/cdf-enums.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace cdf::chrono::_impl
{
using namespace std::chrono;
using namespace cdf::chrono;


// TAI-UTC in ns for a UTC time before 1972, as NASA's library computes it: constant over a UTC
// day (evaluated at its noon), truncated to whole nanoseconds.
inline int64_t tai_minus_utc_before_1972(int64_t utc_ns_from_1970)
{
    constexpr int64_t day_ns = 86'400'000'000'000;
    const auto& periods = leap_seconds::drift_periods_before_1972;
    if (utc_ns_from_1970 < periods.front().start_ns_from_1970)
        return 0;
    const auto& p = *std::find_if(std::crbegin(periods), std::crend(periods),
        [utc_ns_from_1970](const auto& period) { return period.start_ns_from_1970 <= utc_ns_from_1970; });
    const auto day = utc_ns_from_1970 / day_ns - (utc_ns_from_1970 % day_ns < 0 ? 1 : 0);
    const double mjd_at_noon = static_cast<double>(day) + 40587.0 + 0.5;
    // volatile rounds the product before the sum, as NASA's library does: compilers targeting
    // FMA hardware (every aarch64 build) would otherwise fuse them, rounding once, and land 1 ns
    // off around truncation boundaries.
    const volatile double drift = (mjd_at_noon - p.mjd_ref) * p.rate;
    return static_cast<int64_t>((p.base + drift) * 1e9);
}

inline int64_t leap_second_branchless(int64_t ns_from_1970)
{
    const auto& table = leap_seconds::leap_seconds_tt2000;
    int64_t offset
        = ns_from_1970 < table.front().first ? tai_minus_utc_before_1972(ns_from_1970) : 0;
    for (size_t i = 0; i < table.size(); ++i)
    {
        offset = (ns_from_1970 >= table[i].first) ? table[i].second : offset;
    }
    return offset;
}

inline int64_t leap_second(int64_t ns_from_1970)
{
    if (ns_from_1970 >= leap_seconds::leap_seconds_tt2000.front().first)
    {
        if (ns_from_1970 < leap_seconds::leap_seconds_tt2000.back().first)
        {
            auto lc = std::cbegin(leap_seconds::leap_seconds_tt2000);
            while (ns_from_1970 >= (lc + 1)->first)
            {
                lc++;
            }
            return lc->second;
        }
        else
        {
            return leap_seconds::leap_seconds_tt2000.back().second;
        }
    }
    return tai_minus_utc_before_1972(ns_from_1970);
}

// UTC from TAI before 1972. TAI-UTC depends on the UTC day, so this refines a guess at most
// twice, like NASA's breakdownTT2000 (a guess may fall in 1972, hence leap_second).
inline int64_t utc_from_tai_before_1972(int64_t tai_ns_from_1970)
{
    int64_t utc = tai_ns_from_1970;
    for (int refinement = 0; refinement < 2; ++refinement)
    {
        const auto next = tai_ns_from_1970 - leap_second(utc);
        if (next == utc)
            break;
        utc = next;
    }
    return utc;
}

inline int64_t leap_second_before_1972(const tt2000_t& ep)
{
    const auto tai = ep.nseconds + constants::tt2000_offset;
    return tai - utc_from_tai_before_1972(tai);
}

inline int64_t leap_second(const tt2000_t& ep)
{
    if (ep.nseconds >= leap_seconds::leap_seconds_tt2000_reverse.front().first)
    {
        if (ep.nseconds < leap_seconds::leap_seconds_tt2000_reverse.back().first)
        {
            auto lc = std::cbegin(leap_seconds::leap_seconds_tt2000_reverse);
            while (ep.nseconds >= (lc + 1)->first)
            {
                lc++;
            }
            return lc->second;
        }
        else
        {
            return leap_seconds::leap_seconds_tt2000_reverse.back().second;
        }
    }
    return leap_second_before_1972(ep);
}

inline auto _leap_second(const tt2000_t& ep, std::size_t leap_index_hint)
{
    if (ep.nseconds >= leap_seconds::leap_seconds_tt2000_reverse[leap_index_hint].first)
    {
        while (leap_index_hint + 1 < leap_seconds::leap_seconds_tt2000_reverse.size()
            && ep.nseconds >= leap_seconds::leap_seconds_tt2000_reverse[leap_index_hint + 1].first)
        {
            ++leap_index_hint;
        }
        return std::tuple { static_cast<int64_t>(
                                 leap_seconds::leap_seconds_tt2000_reverse[leap_index_hint].second),
            leap_index_hint };
    }
    else
    {
        while (leap_index_hint > 0
            && ep.nseconds < leap_seconds::leap_seconds_tt2000_reverse[leap_index_hint].first)
        {
            --leap_index_hint;
        }
        if (ep.nseconds < leap_seconds::leap_seconds_tt2000_reverse[0].first)
            return std::tuple { leap_second_before_1972(ep), std::size_t { 0 } };
        return std::tuple { static_cast<int64_t>(
                                leap_seconds::leap_seconds_tt2000_reverse[leap_index_hint].second),
            leap_index_hint };
    }
}


// numpy and pandas read INT64_MIN nanoseconds as NaT ("not a time").
inline constexpr int64_t nat = std::numeric_limits<int64_t>::min();

// NASA's special TT2000 values: fill (nat itself), pad and illegal. NASA's library prints them as
// 9999-12-31, 0000-01-01 and 9999-12-31; INT64_MIN + 2 is a real date (1707-09-22).
inline constexpr int64_t tt2000_pad = nat + 1;
inline constexpr int64_t tt2000_illegal = nat + 3;
// Later TT2000 values are after 2262-04-11: their ns since 1970 don't fit int64.
inline constexpr int64_t last_representable_tt2000 = std::numeric_limits<int64_t>::max()
    - (constants::tt2000_offset - leap_seconds::leap_seconds_tt2000_reverse.back().second);

inline bool has_ns_since_1970(const tt2000_t& ep)
{
    return ep.nseconds > tt2000_pad && ep.nseconds != tt2000_illegal
        && ep.nseconds <= last_representable_tt2000;
}

// The range of int64 ns since 1970 (1677-09-21 to 2262-04-11) as CDF_EPOCH ms: the smallest
// double giving at least INT64_MIN ns, the largest giving at most INT64_MAX. Fill (-1e31), pad
// (0.0), NaN and infinities are outside.
inline constexpr double epoch_min_ms = 52943847163145.2265625;
inline constexpr double epoch_max_ms = 71390591236854.765625;

// floor((ms - offset) * 1e6), exactly: a double product would round to 256 ns. In range, ms is
// positive, so truncation is floor, and above 2^45, so its fraction is a multiple of 2^-7 and
// fraction * 1e6 is exact.
inline int64_t epoch_to_ns_from_1970(const epoch& ep)
{
    // Branchless, so that loops vectorize: out of range values are swapped for a harmless one
    // before any conversion (converting NaN or 1e31 to int64 is undefined behaviour).
    const bool in_range = ep.mseconds >= epoch_min_ms && ep.mseconds <= epoch_max_ms;
    const double ms = in_range ? ep.mseconds : constants::epoch_offset_miliseconds;
    constexpr auto offset_ms = static_cast<int64_t>(constants::epoch_offset_miliseconds);
    const auto whole_ms = static_cast<int64_t>(ms);
    const auto fraction_ns = static_cast<int64_t>((ms - static_cast<double>(whole_ms)) * 1e6);
    // Unsigned: near 1677 the whole ms alone are below INT64_MIN ns, the sum is not.
    const auto ns = static_cast<int64_t>(
        static_cast<uint64_t>(whole_ms - offset_ms) * 1'000'000U + static_cast<uint64_t>(fraction_ns));
    return in_range ? ns : nat;
}

// Whole seconds whose every picosecond fits int64 ns since 1970.
inline constexpr double epoch16_min_s = constants::epoch_offset_seconds - 9223372036.0;
inline constexpr double epoch16_max_s = constants::epoch_offset_seconds + 9223372035.0;

inline int64_t epoch16_to_ns_from_1970(const epoch16& ep)
{
    if (!(ep.seconds >= epoch16_min_s && ep.seconds <= epoch16_max_s && ep.picoseconds >= 0.0
            && ep.picoseconds < 1e12))
        return nat;
    // Whole seconds are scaled in int64: ~1e18 ns doesn't fit a double's 53-bit mantissa.
    return static_cast<int64_t>(ep.seconds - constants::epoch_offset_seconds) * 1'000'000'000
        + static_cast<int64_t>(ep.picoseconds / 1'000);
}

inline void _unsorted_to_ns_from_1970(
    const tt2000_t* const input, const std::size_t count, int64_t* const output)
{
    std::size_t last_index = 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!has_ns_since_1970(input[i]))
        {
            output[i] = nat;
            continue;
        }
        auto [ls, idx] = _leap_second(input[i], last_index);
        output[i] = input[i].nseconds - ls + constants::tt2000_offset;
        last_index = idx;
    }
}

inline void _optimistic_to_ns_from_1970_after_2017(
    const tt2000_t* const input, const std::size_t count, int64_t* const output)
{
    const auto last_leap_sec = leap_seconds::leap_seconds_tt2000_reverse.back().first;
    const auto offset
        = constants::tt2000_offset - leap_seconds::leap_seconds_tt2000_reverse.back().second;
    bool all_after_2017 = true;
    for (std::size_t i = 0; i < count; ++i)
    {
        output[i] = static_cast<int64_t>(
            static_cast<uint64_t>(input[i].nseconds) + static_cast<uint64_t>(offset));
        all_after_2017 &= (input[i].nseconds >= last_leap_sec)
            && (input[i].nseconds <= last_representable_tt2000);
    }
    if (!all_after_2017)
    {
        _impl::_unsorted_to_ns_from_1970(input, count, output);
    }
}

inline void scalar_to_ns_from_1970(
    const std::span<const tt2000_t>& input, int64_t* const output)
{
    if (input.size() == 0)
    {
        return;
    }
    if (input[0].nseconds >= leap_seconds::leap_seconds_tt2000_reverse.back().first)
    {
        return _impl::_optimistic_to_ns_from_1970_after_2017(input.data(), input.size(), output);
    }
    return _impl::_unsorted_to_ns_from_1970(input.data(), input.size(), output);
}

inline void scalar_to_ns_from_1970(
    const std::span<const epoch>& input, int64_t* const output)
{
    std::ranges::transform(input, output, epoch_to_ns_from_1970);
}

inline void scalar_to_ns_from_1970(
    const std::span<const epoch16>& input, int64_t* const output)
{
    std::ranges::transform(input, output, epoch16_to_ns_from_1970);
}

}
