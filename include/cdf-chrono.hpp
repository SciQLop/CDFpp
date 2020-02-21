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
#include "date/date.h"
#include <chrono>

namespace cdf
{
using namespace date;
using namespace std::chrono;
constexpr auto _0AD = sys_days { 0_y / January / 1 } + 0h;
constexpr auto _1970 = sys_days { 1970_y / January / 1 } + 0h;
constexpr auto _J2000 = sys_days { 2000_y / January / 1 } + 12h;
constexpr double seconds_0AD_to_1970 = duration_cast<seconds>(_1970 - _0AD).count();
constexpr double mseconds_0AD_to_1970 = duration_cast<milliseconds>(_1970 - _0AD).count();
constexpr int64_t seconds_1970_to_J2000 = duration_cast<seconds>(_J2000 - _1970).count();

constexpr std::array leap_seconds_J2000 =
{
    std::pair{ duration_cast<seconds>( (sys_days { 1972_y / 01 / 01 } + 0h ) - _J2000).count() , 10 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1972_y / 07 / 01 } + 0h ) - _J2000).count() , 11 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1973_y / 01 / 01 } + 0h ) - _J2000).count() , 12 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1974_y / 01 / 01 } + 0h ) - _J2000).count() , 13 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1975_y / 01 / 01 } + 0h ) - _J2000).count() , 14 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1976_y / 01 / 01 } + 0h ) - _J2000).count() , 15 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1977_y / 01 / 01 } + 0h ) - _J2000).count() , 16 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1978_y / 01 / 01 } + 0h ) - _J2000).count() , 17 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1979_y / 01 / 01 } + 0h ) - _J2000).count() , 18 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1980_y / 01 / 01 } + 0h ) - _J2000).count() , 19 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1981_y / 07 / 01 } + 0h ) - _J2000).count() , 20 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1982_y / 07 / 01 } + 0h ) - _J2000).count() , 21 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1983_y / 07 / 01 } + 0h ) - _J2000).count() , 22 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1985_y / 07 / 01 } + 0h ) - _J2000).count() , 23 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1988_y / 01 / 01 } + 0h ) - _J2000).count() , 24 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1990_y / 01 / 01 } + 0h ) - _J2000).count() , 25 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1991_y / 01 / 01 } + 0h ) - _J2000).count() , 26 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1992_y / 07 / 01 } + 0h ) - _J2000).count() , 27 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1993_y / 07 / 01 } + 0h ) - _J2000).count() , 28 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1994_y / 07 / 01 } + 0h ) - _J2000).count() , 29 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1996_y / 01 / 01 } + 0h ) - _J2000).count() , 30 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1997_y / 07 / 01 } + 0h ) - _J2000).count() , 31 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 1999_y / 01 / 01 } + 0h ) - _J2000).count() , 32 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 2006_y / 01 / 01 } + 0h ) - _J2000).count() , 33 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 2009_y / 01 / 01 } + 0h ) - _J2000).count() , 34 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 2012_y / 07 / 01 } + 0h ) - _J2000).count() , 35 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 2015_y / 07 / 01 } + 0h ) - _J2000).count() , 36 - 32 },
    std::pair{ duration_cast<seconds>( (sys_days { 2017_y / 01 / 01 } + 0h ) - _J2000).count() , 37 - 32 },
};


template <class Clock, class Duration = typename Clock::duration>
epoch to_epoch(std::chrono::time_point<Clock, Duration> tp)
{
    using namespace std::chrono;
    return epoch { duration_cast<milliseconds>(tp.time_since_epoch()).count()
        + mseconds_0AD_to_1970 };
}

template <class Clock, class Duration = typename Clock::duration>
epoch16 to_epoch16(std::chrono::time_point<Clock, Duration> tp)
{
    using namespace std::chrono;
    return epoch16 { duration_cast<seconds>(tp.time_since_epoch()).count() + seconds_0AD_to_1970,
        duration_cast<nanoseconds>(tp.time_since_epoch()).count() * 1000. };
}

inline int64_t J2000_leap_sec_offset(int64_t sec)
{
    if (sec < leap_seconds_J2000[0].first)
        return 0;
    auto i = 0ul;
    while (i < std::size(leap_seconds_J2000) && leap_seconds_J2000[i].first < sec)
    {
        i++;
    }
    return leap_seconds_J2000[i - 1].second;
}

template <class Clock, class Duration = typename Clock::duration>
tt2000_t to_tt2000(std::chrono::time_point<Clock, Duration> tp)
{
    using namespace std::chrono;
    auto [sec, nsec]
        = std::lldiv(duration_cast<nanoseconds>(tp.time_since_epoch()).count(), 1000000000);
    sec -= seconds_1970_to_J2000;
    sec += J2000_leap_sec_offset(sec);
    return tt2000_t { sec * 1000000000 + nsec };
}

}
