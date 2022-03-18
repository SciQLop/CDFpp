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
#include "attribute.hpp"
#include "cdf-data.hpp"
#include "cdf-enums.hpp"
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cdf
{
struct Variable
{
    using var_data_t = data_t;
    using shape_t = std::vector<uint32_t>;
    std::unordered_map<std::string, Attribute> attributes;
    Variable() = default;
    Variable(Variable&&) = default;
    Variable(const Variable&) = default;
    Variable& operator=(const Variable&) = default;
    Variable& operator=(Variable&&) = default;
    Variable(const std::string& name, std::size_t number, var_data_t&& data, shape_t&& shape,
        cdf_majority majority)
            : p_name { name }
            , p_number { number }
            , p_data { std::move(data) }
            , p_shape { shape }
            , p_majority { majority }
    {
    }

    Variable(const std::string& name, var_data_t&& data, shape_t&& shape)
            : p_name { name }, p_data { std::move(data) }, p_shape { shape }
    {
    }

    template <CDF_Types type>
    decltype(auto) get()
    {
        return p_data.get<type>();
    }

    template <CDF_Types type>
    decltype(auto) get() const
    {
        return p_data.get<type>();
    }

    template <typename type>
    decltype(auto) get()
    {
        return p_data.get<type>();
    }

    template <typename type>
    decltype(auto) get() const
    {
        return p_data.get<type>();
    }

    const std::string& name() const { return p_name; }

    const shape_t& shape() const { return p_shape; }
    std::size_t len() const { return p_shape[0]; }

    void set_data(const data_t& data, const shape_t& shape)
    {
        p_data = data;
        p_shape = shape;
    }

    void set_data(data_t&& data, shape_t&& shape)
    {
        p_data = std::move(data);
        p_shape = std::move(shape);
    }

    std::optional<std::size_t> number() { return p_number; }

    CDF_Types type() const { return p_data.type(); }

    cdf_majority majority() { return p_majority; }

    template <typename... Ts>
    friend auto visit(Variable& var, Ts... lambdas);

private:
    std::string p_name;
    std::optional<std::size_t> p_number;
    var_data_t p_data;
    shape_t p_shape;
    cdf_majority p_majority;
};

template <typename... Ts>
auto visit(Variable& var, Ts... lambdas)
{
    return visit(var.p_data, lambdas...);
}
} // namespace cdf
