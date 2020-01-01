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
#include "variable.hpp"
#include <string>
#include <unordered_map>

namespace cdf
{
struct CDF
{
    std::unordered_map<std::string, Variable> variables;
    std::unordered_map<std::string, Attribute> attributes;
    std::unordered_map<std::size_t, std::unordered_map<std::string, Attribute>> p_var_attributes;
    const Variable& operator[](const std::string& name) const { return variables.at(name); }
    Variable& operator[](const std::string& name) { return variables.at(name); }
};

void add_global_attribute(CDF& cdf_file, const std::string& name, Attribute::attr_data_t&& data)
{
    if (auto [_, success] = cdf_file.attributes.try_emplace(name, name, std::move(data)); !success)
    {
        cdf_file.attributes[name] = std::move(data);
    }
}

void add_var_attribute(CDF& cdf_file, std::size_t variable_index, const std::string& name,
    Attribute::attr_data_t&& data)
{
    if (auto [_, success]
        = cdf_file.p_var_attributes[variable_index].try_emplace(name, name, std::move(data));
        !success)
    {
        cdf_file.p_var_attributes[variable_index][name] = std::move(data);
    }
}

void add_attribute(CDF& cdf_file, cdf_attr_scope scope, const std::string& name,
    Attribute::attr_data_t&& data, std::size_t variable_index = 0)
{
    if (scope == cdf_attr_scope::global || scope == cdf_attr_scope::global_assumed)
        add_global_attribute(cdf_file, name, std::move(data));
    else if (scope == cdf_attr_scope::variable || scope == cdf_attr_scope::variable_assumed)
    {
        add_var_attribute(cdf_file, variable_index, name, std::move(data));
    }
}

void add_variable(CDF& cdf_file, const std::string& name, Variable&& var)
{
    if (auto [_, success] = cdf_file.variables.try_emplace(name, std::move(var)); !success)
    {
        cdf_file.variables[name] = std::move(var);
    }
}

void add_variable(CDF& cdf_file, const std::string& name, std::size_t number,
    Variable::var_data_t&& data, Variable::shape_t&& shape)
{
    if (auto [_, success]
        = cdf_file.variables.try_emplace(name, name, number, std::move(data), std::move(shape));
        !success)
    {
        auto& var = cdf_file.variables[name];
        var.set_data(std::move(data), std::move(shape));
    }
    cdf_file.variables[name].attributes = [&]() -> decltype(Variable::attributes) {
        auto attrs = cdf_file.p_var_attributes.extract(number);
        if (!attrs.empty())
            return attrs.mapped();
        else
            return {};
    }();
}

} // namespace cdf
