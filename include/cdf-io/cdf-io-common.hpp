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
#include "cdf-io-desc-records.hpp"
#include <type_traits>

namespace cdf::io::common
{
    bool is_v3x(const magic_numbers_t& magic) { return ((magic.first >> 12) & 0xff) >= 0x30; }

    bool is_cdf(const magic_numbers_t& magic_numbers) noexcept
    {
        return (magic_numbers.first & 0xfff00000) == 0xCDF00000
            && (magic_numbers.second == 0xCCCC0001 || magic_numbers.second == 0x0000FFFF);
    }

    template <typename context_t>
    bool is_cdf(const context_t& context) noexcept
    {
        return is_cdf(context.magic);
    }

    bool is_compressed(const magic_numbers_t& magic_numbers) noexcept
    {
        return magic_numbers.second == 0xCCCC0001;
    }

}
