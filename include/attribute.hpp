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
#include <string>
#include <variant>

namespace cdf
{
struct Attribute
{
    using attr_data_t = std::vector<data_t>;
    std::string name;
    Attribute() = default;
    Attribute(const Attribute&) = default;
    Attribute(Attribute&&) = default;
    Attribute& operator=(Attribute&&) = default;
    Attribute& operator=(const Attribute&) = default;
    Attribute(const std::string& name, attr_data_t&& data) : name { name }
    {
        this->data = std::move(data);
    }

    template <CDF_Types type>
    decltype(auto) get(std::size_t index)
    {
        return data[index].get<type>();
    }

    template <CDF_Types type>
    decltype(auto) get(std::size_t index) const
    {
        return data[index].get<type>();
    }

    template <typename type>
    decltype(auto) get(std::size_t index)
    {
        return data[index].get<type>();
    }

    template <typename type>
    decltype(auto) get(std::size_t index) const
    {
        return data[index].get<type>();
    }

    void swap(attr_data_t& new_data) { std::swap(data, new_data); }

    Attribute& operator=(attr_data_t& new_data)
    {
        data = new_data;
        return *this;
    }

    Attribute& operator=(attr_data_t&& new_data)
    {
        data = new_data;
        return *this;
    }
    std::size_t len() const { return std::size(data); }

private:
    attr_data_t data;
};
} // namespace cdf
