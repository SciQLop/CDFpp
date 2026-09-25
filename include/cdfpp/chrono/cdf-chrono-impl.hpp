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
#include <array>

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
    return static_cast<int64_t>((p.base + (mjd_at_noon - p.mjd_ref) * p.rate) * 1e9);
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


inline void _unsorted_to_ns_from_1970(
    const tt2000_t* const input, const std::size_t count, int64_t* const output)
{
    if (count)
    {
        std::size_t last_index = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            auto [ls, idx] = _leap_second(input[i], last_index);
            output[i] = input[i].nseconds - ls + constants::tt2000_offset;
            last_index = idx;
        }
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
        output[i] = input[i].nseconds + offset;
        all_after_2017 &= (input[i].nseconds >= last_leap_sec);
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
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        output[i] = (input[i].mseconds - constants::epoch_offset_miliseconds) * 1'000'000;
    }
}

inline void scalar_to_ns_from_1970(
    const std::span<const epoch16>& input, int64_t* const output)
{

    // Whole seconds are scaled in int64: ~1e18 ns doesn't fit a double's 53-bit mantissa.
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        output[i]
            = static_cast<int64_t>(input[i].seconds - constants::epoch_offset_seconds)
                * 1'000'000'000
            + static_cast<int64_t>(input[i].picoseconds / 1'000);
    }
}

}
