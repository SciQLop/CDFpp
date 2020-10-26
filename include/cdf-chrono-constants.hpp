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
#include <cmath>
#include <array>

namespace cdf::chrono::constants
{
using namespace date;
using namespace std::chrono;
constexpr auto _0AD = sys_days { 0_y / January / 1 } + 0h;
constexpr auto _1900 = sys_days { 1900_y / January / 1 } + 0h;
constexpr auto _1970 = sys_days { 1970_y / January / 1 } + 0h;
constexpr auto _J2000 = sys_days { 2000_y / January / 1 } + 12h;
constexpr double seconds_0AD_to_1970 = duration_cast<seconds>(_1970 - _0AD).count();
constexpr double mseconds_0AD_to_1970 = duration_cast<milliseconds>(_1970 - _0AD).count();
constexpr int64_t seconds_1970_to_J2000 = duration_cast<seconds>(_J2000 - _1970).count();
constexpr int64_t seconds_1900_to_J2000 = duration_cast<seconds>(_J2000 - _1900).count();
}
