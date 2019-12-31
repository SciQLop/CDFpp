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
#include "cdf-data.hpp"
#include <cstdint>
#include <vector>

namespace cdf
{
struct Variable
{
    using var_data_t = data_t;
    std::string name;
    std::vector<uint32_t> shape;
    Variable() = default;
    Variable(Variable&&) = default;
    Variable(const Variable&) = default;
    Variable& operator=(const Variable&) = default;
    Variable& operator=(Variable&&) = default;
    Variable(const std::string& name, std::vector<uint32_t>&& shape, var_data_t&& data)
            : name { name }, shape { shape }, data { data }
    {
    }

    template <CDF_Types type>
    decltype(auto) get()
    {
        return data.get<type>();
    }

    template <CDF_Types type>
    decltype(auto) get() const
    {
        return data.get<type>();
    }

    template <typename type>
    decltype(auto) get()
    {
        return data.get<type>();
    }

    template <typename type>
    decltype(auto) get() const
    {
        return data.get<type>();
    }

private:
    var_data_t data;
};
} // namespace cdf
