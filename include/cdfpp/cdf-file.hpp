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
#include "cdf-map.hpp"
#include "cdf-repr.hpp"
#include "chrono/cdf-leap-seconds.h"
#include "variable.hpp"

#include <string>

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const cdf_map<std::string, cdf::Variable>& variables)
{
    return os;
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const cdf_map<std::string, cdf::Attribute>& attributes)
{
    std::for_each(std::cbegin(attributes), std::cend(attributes),
        [&os](const auto& item) { item.second.__repr__(os, indent_t {}); });
    return os;
}


namespace cdf
{
struct CDF
{
    cdf_majority majority = cdf_majority::row;
    cdf_compression_type compression = cdf_compression_type::no_compression;
    std::tuple<uint32_t, uint32_t, uint32_t> distribution_version = { 3, 9, 0 };
    cdf_map<std::string, Variable> variables;
    cdf_map<std::string, Attribute> attributes;
    uint32_t leap_second_last_updated = chrono::leap_seconds::last_updated;
    bool lazy_loaded = false;

    CDF() = default;
    CDF(const CDF&) = default;
    CDF(CDF&&) = default;
    CDF& operator=(CDF&&) = default;
    CDF& operator=(const CDF&) = default;

    inline bool operator==(const CDF& other) const
    {
        return (other.leap_second_last_updated == leap_second_last_updated)
            && (other.attributes == attributes) && (other.variables == variables);
    }

    inline bool operator!=(const CDF& other) const { return !(*this == other); }

    [[nodiscard]] const Variable& operator[](const std::string& name) const
    {
        return variables.at(name);
    }
    [[nodiscard]] Variable& operator[](const std::string& name) { return variables.at(name); }

    template <class stream_t>
    inline stream_t& __repr__(stream_t& os, indent_t indent = {}) const
    {
        os << indent << "CDF:\n"
           << indent + 2
           << fmt::format("version: {}.{}.{}\n", std::get<0>(distribution_version),
                  std::get<1>(distribution_version), std::get<2>(distribution_version))
           << indent + 2 << majority << '\n'
           << indent + 2 << compression << "\n\nAttributes:\n";
        std::for_each(std::cbegin(attributes), std::cend(attributes),
            [&os, indent](const auto& item) { item.second.__repr__(os, indent + 2); });
        os << indent << "\nVariables:\n";
        std::for_each(std::cbegin(variables), std::cend(variables),
            [&os, indent](const auto& item) { item.second.__repr__(os, indent + 2, false); });
        os << std::endl;
        return os;
    }
};

void add_attribute(CDF& cdf_file, const std::string& name, Attribute::attr_data_t&& data)
{
    cdf_file.attributes.emplace(name, name, std::move(data));
}

void add_variable(CDF& cdf_file, const std::string& name, Variable&& var)
{
    cdf_file.variables.emplace(name, std::move(var));
}

} // namespace cdf

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const cdf::CDF& cdf_file)
{
    return cdf_file.__repr__(os);
}
