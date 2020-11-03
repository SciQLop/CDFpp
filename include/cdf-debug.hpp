#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2019, Plasma Physics Laboratory - CNRS
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
#ifdef CDFPP_ENABLE_ASSERT
#include <cassert>
#define CDFPP_ASSERT(x) assert(x)
#else
#define CDFPP_ASSERT(x)
#endif
#ifdef CDFPP_HEDLEY
#include <hedley.h>
#define CDFPP_NON_NULL(...) HEDLEY_NON_NULL(__VA_ARGS__)
#define CDF_WARN_UNUSED_RESULT HEDLEY_WARN_UNUSED_RESULT
#else
#define CDFPP_NONNULL
#define CDF_WARN_UNUSED_RESULT
#endif
