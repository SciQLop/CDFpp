
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
#include "date/date.h"
#include <chrono>
#include <utility>
#include <array>
#include "cdf-chrono-constants.hpp"
namespace cdf::chrono::leap_seconds
{    
constexpr std::array leap_seconds_tt2000 = 
    {// 1972-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2272060800 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 10 - 32 },
// 1972-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2287785600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 11 - 32 },
// 1973-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2303683200 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 12 - 32 },
// 1974-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2335219200 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 13 - 32 },
// 1975-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2366755200 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 14 - 32 },
// 1976-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2398291200 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 15 - 32 },
// 1977-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2429913600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 16 - 32 },
// 1978-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2461449600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 17 - 32 },
// 1979-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2492985600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 18 - 32 },
// 1980-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2524521600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 19 - 32 },
// 1981-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2571782400 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 20 - 32 },
// 1982-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2603318400 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 21 - 32 },
// 1983-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2634854400 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 22 - 32 },
// 1985-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2698012800 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 23 - 32 },
// 1988-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2776982400 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 24 - 32 },
// 1990-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2840140800 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 25 - 32 },
// 1991-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2871676800 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 26 - 32 },
// 1992-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2918937600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 27 - 32 },
// 1993-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2950473600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 28 - 32 },
// 1994-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2982009600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 29 - 32 },
// 1996-01-01 00:00:00.000
    std::pair{ tt2000_t{ (3029443200 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 30 - 32 },
// 1997-07-01 00:00:00.000
    std::pair{ tt2000_t{ (3076704000 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 31 - 32 },
// 1999-01-01 00:00:00.000
    std::pair{ tt2000_t{ (3124137600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 32 - 32 },
// 2006-01-01 00:00:00.000
    std::pair{ tt2000_t{ (3345062400 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 33 - 32 },
// 2009-01-01 00:00:00.000
    std::pair{ tt2000_t{ (3439756800 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 34 - 32 },
// 2012-07-01 00:00:00.000
    std::pair{ tt2000_t{ (3550089600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 35 - 32 },
// 2015-07-01 00:00:00.000
    std::pair{ tt2000_t{ (3644697600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 36 - 32 },
// 2017-01-01 00:00:00.000
    std::pair{ tt2000_t{ (3692217600 - constants::seconds_1900_to_J2000)*1000*1000*1000 } , 37 - 32 },
};
constexpr std::array leap_seconds_tt2000_reverse = 
    {// 1972-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2272060800 - constants::seconds_1900_to_J2000 + 10 -32) *1000*1000*1000 }  , 10 - 32 },
// 1972-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2287785600 - constants::seconds_1900_to_J2000 + 11 -32) *1000*1000*1000 }  , 11 - 32 },
// 1973-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2303683200 - constants::seconds_1900_to_J2000 + 12 -32) *1000*1000*1000 }  , 12 - 32 },
// 1974-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2335219200 - constants::seconds_1900_to_J2000 + 13 -32) *1000*1000*1000 }  , 13 - 32 },
// 1975-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2366755200 - constants::seconds_1900_to_J2000 + 14 -32) *1000*1000*1000 }  , 14 - 32 },
// 1976-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2398291200 - constants::seconds_1900_to_J2000 + 15 -32) *1000*1000*1000 }  , 15 - 32 },
// 1977-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2429913600 - constants::seconds_1900_to_J2000 + 16 -32) *1000*1000*1000 }  , 16 - 32 },
// 1978-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2461449600 - constants::seconds_1900_to_J2000 + 17 -32) *1000*1000*1000 }  , 17 - 32 },
// 1979-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2492985600 - constants::seconds_1900_to_J2000 + 18 -32) *1000*1000*1000 }  , 18 - 32 },
// 1980-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2524521600 - constants::seconds_1900_to_J2000 + 19 -32) *1000*1000*1000 }  , 19 - 32 },
// 1981-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2571782400 - constants::seconds_1900_to_J2000 + 20 -32) *1000*1000*1000 }  , 20 - 32 },
// 1982-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2603318400 - constants::seconds_1900_to_J2000 + 21 -32) *1000*1000*1000 }  , 21 - 32 },
// 1983-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2634854400 - constants::seconds_1900_to_J2000 + 22 -32) *1000*1000*1000 }  , 22 - 32 },
// 1985-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2698012800 - constants::seconds_1900_to_J2000 + 23 -32) *1000*1000*1000 }  , 23 - 32 },
// 1988-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2776982400 - constants::seconds_1900_to_J2000 + 24 -32) *1000*1000*1000 }  , 24 - 32 },
// 1990-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2840140800 - constants::seconds_1900_to_J2000 + 25 -32) *1000*1000*1000 }  , 25 - 32 },
// 1991-01-01 00:00:00.000
    std::pair{ tt2000_t{ (2871676800 - constants::seconds_1900_to_J2000 + 26 -32) *1000*1000*1000 }  , 26 - 32 },
// 1992-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2918937600 - constants::seconds_1900_to_J2000 + 27 -32) *1000*1000*1000 }  , 27 - 32 },
// 1993-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2950473600 - constants::seconds_1900_to_J2000 + 28 -32) *1000*1000*1000 }  , 28 - 32 },
// 1994-07-01 00:00:00.000
    std::pair{ tt2000_t{ (2982009600 - constants::seconds_1900_to_J2000 + 29 -32) *1000*1000*1000 }  , 29 - 32 },
// 1996-01-01 00:00:00.000
    std::pair{ tt2000_t{ (3029443200 - constants::seconds_1900_to_J2000 + 30 -32) *1000*1000*1000 }  , 30 - 32 },
// 1997-07-01 00:00:00.000
    std::pair{ tt2000_t{ (3076704000 - constants::seconds_1900_to_J2000 + 31 -32) *1000*1000*1000 }  , 31 - 32 },
// 1999-01-01 00:00:00.000
    std::pair{ tt2000_t{ (3124137600 - constants::seconds_1900_to_J2000 + 32 -32) *1000*1000*1000 }  , 32 - 32 },
// 2006-01-01 00:00:00.000
    std::pair{ tt2000_t{ (3345062400 - constants::seconds_1900_to_J2000 + 33 -32) *1000*1000*1000 }  , 33 - 32 },
// 2009-01-01 00:00:00.000
    std::pair{ tt2000_t{ (3439756800 - constants::seconds_1900_to_J2000 + 34 -32) *1000*1000*1000 }  , 34 - 32 },
// 2012-07-01 00:00:00.000
    std::pair{ tt2000_t{ (3550089600 - constants::seconds_1900_to_J2000 + 35 -32) *1000*1000*1000 }  , 35 - 32 },
// 2015-07-01 00:00:00.000
    std::pair{ tt2000_t{ (3644697600 - constants::seconds_1900_to_J2000 + 36 -32) *1000*1000*1000 }  , 36 - 32 },
// 2017-01-01 00:00:00.000
    std::pair{ tt2000_t{ (3692217600 - constants::seconds_1900_to_J2000 + 37 -32) *1000*1000*1000 }  , 37 - 32 },
};

} //namespace cdf
    