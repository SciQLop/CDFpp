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
    inline decltype(auto) get(std::size_t index)
    {
        return data[index].get<type>();
    }

    template <CDF_Types type>
    inline decltype(auto) get(std::size_t index) const
    {
        return data[index].get<type>();
    }

    template <typename type>
    inline decltype(auto) get(std::size_t index)
    {
        return data[index].get<type>();
    }

    template <typename type>
    inline decltype(auto) get(std::size_t index) const
    {
        return data[index].get<type>();
    }

    inline void swap(attr_data_t& new_data) { std::swap(data, new_data); }

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
    inline std::size_t size() const { return std::size(data); }
    inline data_t& operator[](std::size_t index) { return data[index]; }

    template <typename... Ts>
    friend void visit(Attribute& attr, Ts... lambdas);

    template <typename... Ts>
    friend void visit(const Attribute& attr, Ts... lambdas);

    auto begin() { return data.begin(); }
    auto end() { return data.end(); }

    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

    auto cbegin() const { return data.cbegin(); }
    auto cend() const { return data.cend(); }

    auto& back() { return data.back(); }
    const auto& back() const { return data.back(); }

    auto& front() { return data.front(); }
    const auto& front() const { return data.front(); }

private:
    attr_data_t data;
};
template <typename... Ts>
void visit(Attribute& attr, Ts... lambdas)
{
    std::for_each(std::cbegin(attr.data), std::cend(attr.data),
        [lambdas...](const auto& element) { visit(element, lambdas...); });
}

template <typename... Ts>
void visit(const Attribute& attr, Ts... lambdas)
{
    std::for_each(std::cbegin(attr.data), std::cend(attr.data),
        [lambdas...](const auto& element) { visit(element, lambdas...); });
}
} // namespace cdf
