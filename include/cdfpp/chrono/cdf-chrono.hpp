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
#include "cdf-chrono-constants.hpp"
#include "cdf-leap-seconds.h"
#include "cdfpp/cdf-debug.hpp"
#include "cdfpp/cdf-enums.hpp"
#include "cdfpp/no_init_vector.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace cdf
{
using namespace std::chrono;
using namespace cdf::chrono;

inline int64_t leap_second(int64_t ns_from_1970)
{
    if (ns_from_1970 > leap_seconds::leap_seconds_tt2000.front().first)
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
    return 0;
}

inline int64_t leap_second(const tt2000_t& ep)
{
    if (ep.value > leap_seconds::leap_seconds_tt2000_reverse.front().first)
    {
        if (ep.value < leap_seconds::leap_seconds_tt2000_reverse.back().first)
        {
            auto lc = std::cbegin(leap_seconds::leap_seconds_tt2000_reverse);
            while (ep.value >= (lc + 1)->first)
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
    return 0;
}

template <class Clock, class Duration = typename Clock::duration>
epoch to_epoch(const std::chrono::time_point<Clock, Duration>& tp)
{
    using namespace std::chrono;
    return epoch { duration_cast<milliseconds>(tp.time_since_epoch()).count()
        + constants::epoch_offset_miliseconds };
}

template <class Clock, class Duration = typename Clock::duration>
no_init_vector<epoch> to_epoch(const no_init_vector<std::chrono::time_point<Clock, Duration>>& tps)
{
    using namespace std::chrono;
    no_init_vector<epoch> result(std::size(tps));
    std::transform(std::cbegin(tps), std::cend(tps), std::begin(result),
        [](const std::chrono::time_point<Clock, Duration>& v) { return to_epoch(v); });
    return result;
}

template <class Clock, class Duration = typename Clock::duration>
epoch16 to_epoch16(const std::chrono::time_point<Clock, Duration>& tp)
{
    using namespace std::chrono;
    auto se = static_cast<double>(duration_cast<seconds>(tp.time_since_epoch()).count());
    auto s = se + constants::epoch_offset_seconds;
    auto ps = (static_cast<double>(duration_cast<nanoseconds>(tp.time_since_epoch()).count())
                  - (se * 1e9))
        * 1000.;
    return epoch16 { s, ps };
}

template <class Clock, class Duration = typename Clock::duration>
no_init_vector<epoch16> to_epoch16(
    const no_init_vector<std::chrono::time_point<Clock, Duration>>& tps)
{
    using namespace std::chrono;
    no_init_vector<epoch16> result(std::size(tps));
    std::transform(std::cbegin(tps), std::cend(tps), std::begin(result),
        [](const std::chrono::time_point<Clock, Duration>& v) { return to_epoch16(v); });
    return result;
}

template <class Clock, class Duration = typename Clock::duration>
tt2000_t to_tt2000(const std::chrono::time_point<Clock, Duration>& tp)
{
    using namespace std::chrono;
    auto nsec = duration_cast<nanoseconds>(tp.time_since_epoch()).count();
    return tt2000_t { nsec - constants::tt2000_offset + leap_second(nsec) };
}

template <class Clock, class Duration = typename Clock::duration>
no_init_vector<tt2000_t> to_tt2000(
    const no_init_vector<std::chrono::time_point<Clock, Duration>>& tps)
{
    using namespace std::chrono;
    no_init_vector<tt2000_t> result(std::size(tps));
    std::transform(std::cbegin(tps), std::cend(tps), std::begin(result),
        [](const std::chrono::time_point<Clock, Duration>& v) { return to_tt2000(v); });
    return result;
}

template <typename T, class Clock, class Duration = typename Clock::duration>
T to_cdf_time(const std::chrono::time_point<Clock, Duration>& tp)
{
    if constexpr (std::is_same_v<T, tt2000_t>)
        return to_tt2000(tp);
    else if constexpr (std::is_same_v<T, epoch>)
        return to_epoch(tp);
    else if constexpr (std::is_same_v<T, epoch16>)
        return to_epoch16(tp);
}

inline auto to_time_point(const epoch& ep)
{
    double ms = ep.value - constants::epoch_offset_miliseconds, ns;
    ns = std::modf(ms, &ms) * 1000000.;
    return std::chrono::time_point<std::chrono::system_clock> {} + milliseconds(int64_t(ms))
        + nanoseconds(int64_t(ns));
}

inline auto to_time_point(const epoch16& ep)
{
    double s = ep.seconds - constants::epoch_offset_seconds, ns;
    ns = ep.picoseconds / 1000.;
    return std::chrono::time_point<std::chrono::system_clock> {} + seconds(static_cast<int64_t>(s))
        + nanoseconds(static_cast<int64_t>(ns));
}


inline auto to_time_point(const tt2000_t& ep)
{
    using namespace std::chrono;
    return time_point<system_clock> {}
    + nanoseconds(ep.value - leap_second(ep) + constants::tt2000_offset);
}


}
