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
#include "cdf-enums.hpp"
#include "cdf-io/cdf-io-common.hpp"
#include "variable.hpp"
#include <string>
#include <unordered_map>


namespace cdf
{
struct CDF
{
    cdf_majority majority;
    std::tuple<uint32_t,uint32_t,uint32_t> distribution_version;
    std::unordered_map<std::string, Variable> variables;
    std::unordered_map<std::string, Attribute> attributes;
    const Variable& operator[](const std::string& name) const { return variables.at(name); }
    Variable& operator[](const std::string& name) { return variables.at(name); }
};

void add_attribute(CDF& cdf_file, const std::string& name, Attribute::attr_data_t&& data)
{
    cdf_file.attributes[name] = std::move(data);
}

void add_variable(CDF& cdf_file, const std::string& name, Variable&& var)
{
    cdf_file.variables[name] = std::move(var);
}

} // namespace cdf
