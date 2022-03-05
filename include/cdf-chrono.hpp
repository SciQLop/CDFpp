#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2020, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#include "cdf-chrono-constants.hpp"
#include "cdf-debug.hpp"
#include "cdf-enums.hpp"
#include "cdf-leap-seconds.h"
CDFPP_DIAGNOSTIC_PUSH
CDFPP_DIAGNOSTIC_DISABLE_DEPRECATED
#include "date/date.h"
CDFPP_DIAGNOSTIC_POP
#include <array>
#include <chrono>
#include <cmath>

namespace cdf
{
using namespace date;
using namespace std::chrono;
using namespace cdf::chrono;

template <class Clock, class Duration = typename Clock::duration>
std::chrono::time_point<Clock, Duration> apply_leap_seconds(
    const std::chrono::time_point<Clock, Duration>& tp)
{
    const int64_t leap = [&]()
    {
        auto lower = std::lower_bound(std::cbegin(leap_seconds::leap_seconds_tt2000),
            std::cend(leap_seconds::leap_seconds_tt2000), tp,
            [](const auto& item, const auto& tp) { return item.first <= tp; });
        if (lower == std::cend(leap_seconds::leap_seconds_tt2000))
            return (lower - 1)->second;
        return lower->second - 1;
    }();
    return { tp + std::chrono::seconds { leap } };
}

tt2000_t remove_leap_seconds(const tt2000_t& ep)
{
    const int64_t leap = [&ep]()
    {
        auto upper = std::upper_bound(std::cbegin(leap_seconds::leap_seconds_tt2000_reverse),
            std::cend(leap_seconds::leap_seconds_tt2000_reverse), ep.value + 1000'000'000,
            [](const auto v, const auto& item) { return v <= item.first.value; });
        if (upper == std::cend(leap_seconds::leap_seconds_tt2000_reverse))
            return (upper - 1)->second;
        return upper->second - 1;
    }();
    return { ep.value - (leap * 1000 * 1000 * 1000) };
}

template <class Clock, class Duration = typename Clock::duration>
epoch to_epoch(const std::chrono::time_point<Clock, Duration>& tp)
{
    using namespace std::chrono;
    return epoch { duration_cast<milliseconds>(tp.time_since_epoch()).count()
        + constants::mseconds_0AD_to_1970 };
}

template <class Clock, class Duration = typename Clock::duration>
epoch16 to_epoch16(const std::chrono::time_point<Clock, Duration>& tp)
{
    using namespace std::chrono;
    auto se = static_cast<double>(duration_cast<seconds>(tp.time_since_epoch()).count());
    auto s = se
        - static_cast<double>(duration_cast<seconds>(constants::_0AD.time_since_epoch()).count());
    auto ps = (static_cast<double>(duration_cast<nanoseconds>(tp.time_since_epoch()).count())
                  - (se * 1e9))
        * 1000;
    return epoch16 { s, ps };
}

template <class Clock, class Duration = typename Clock::duration>
tt2000_t to_tt2000(const std::chrono::time_point<Clock, Duration>& tp)
{
    using namespace std::chrono;
    auto [sec, nsec] = std::lldiv(
        duration_cast<nanoseconds>(apply_leap_seconds(tp) - constants::_J2000).count(), 1000000000);
    // sec -= constants::seconds_1970_to_J2000;
    return tt2000_t { sec * 1000000000 + nsec };
}

auto to_time_point(const epoch& ep)
{
    double ms = ep.value - constants::mseconds_0AD_to_1970, ns;
    ns = std::modf(ms, &ms) * 1000000.;
    return constants::_1970 + milliseconds(int64_t(ms)) + nanoseconds(int64_t(ns));
}

auto to_time_point(const epoch16& ep)
{
    double s = ep.seconds
        + static_cast<double>(duration_cast<seconds>(constants::_0AD.time_since_epoch()).count()),
           ns;
    ns = ep.picoseconds / 1000.;
    return constants::_1970 + seconds(static_cast<int64_t>(s))
        + nanoseconds(static_cast<int64_t>(ns));
}


auto to_time_point(const tt2000_t& ep)
{
    return constants::_J2000 + nanoseconds(remove_leap_seconds(ep).value);
}

}
