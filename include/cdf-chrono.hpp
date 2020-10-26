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
#include "cdf-data.hpp"
#include "cdf-enums.hpp"
#include "cdf-leap-seconds.h"
#include "cdf-chrono-constants.hpp"
#include "date/date.h"
#include <chrono>
#include <cmath>
#include <array>

namespace cdf
{
using namespace date;
using namespace std::chrono;
using namespace cdf::chrono;

tt2000_t apply_leap_seconds(const tt2000_t& ep)
{
    const int64_t leap = [&](){
        auto lower = std::lower_bound(std::cbegin(leap_seconds::leap_seconds_tt2000),
                     std::cend(leap_seconds::leap_seconds_tt2000),
                     ep,
                     [](const auto& item,const tt2000_t& ep){return item.first.value < ep.value;});
        if(lower==std::cend(leap_seconds::leap_seconds_tt2000))
            return (lower-1)->second;
        return lower->second;
    }();
    return {ep.value+(leap*1000*1000*1000)};
}

tt2000_t remove_leap_seconds(const tt2000_t& ep)
{
    const int64_t leap = [&](){
        auto lower = std::lower_bound(std::cbegin(leap_seconds::leap_seconds_tt2000_reverse),
                     std::cend(leap_seconds::leap_seconds_tt2000_reverse),
                     ep,
                     [](const auto& item,const tt2000_t& ep){return item.first.value < ep.value;});
        if(lower==std::cend(leap_seconds::leap_seconds_tt2000_reverse))
            return (lower-1)->second;
        return lower->second;
    }();
    return {ep.value+(leap*1000*1000*1000)};
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
    return epoch16 { duration_cast<seconds>(tp.time_since_epoch()).count() + constants::seconds_0AD_to_1970,
        duration_cast<nanoseconds>(tp.time_since_epoch()).count() * 1000. };
}

template <class Clock, class Duration = typename Clock::duration>
tt2000_t to_tt2000(const std::chrono::time_point<Clock, Duration>& tp)
{
    using namespace std::chrono;
    auto [sec, nsec]
        = std::lldiv(duration_cast<nanoseconds>(tp.time_since_epoch()).count(), 1000000000);
    sec -= constants::seconds_1970_to_J2000;
    return apply_leap_seconds(tt2000_t { sec * 1000000000 + nsec });
}

auto to_time_point(const epoch& ep)
{
    double ms = ep.value - constants::mseconds_0AD_to_1970, ns;
    ns = std::modf(ms, &ms) * 1000000.;
    return constants::_1970 + milliseconds(int64_t(ms)) + nanoseconds(int64_t(ns));
}

auto to_time_point(const epoch16& ep)
{
    double ms = ep.seconds * 1000. - constants::mseconds_0AD_to_1970, ns;
    ns = std::modf(ms, &ms) * 1000000. + ep.picoseconds / 1000.;
    return constants::_1970 + milliseconds(int64_t(ms)) + nanoseconds(int64_t(ns));
}




auto to_time_point(const tt2000_t& ep)
{
    return constants::_J2000 + nanoseconds(remove_leap_seconds(ep).value);
}

}
