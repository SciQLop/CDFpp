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
#include <cdf-data.hpp>
#include <cdf.hpp>
#include <sstream>
#include <string>
using namespace cdf;

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace
{

template <class input_t, class item_t>
std::ostream& stream_collection(std::ostream& os, const input_t& input, const item_t& sep)
{
    if (std::size(input))
    {
        if (std::size(input) > 1)
        {
            std::for_each(std::cbegin(input), --std::cend(input),
                [&sep, &os](const auto& item) { os << item << sep; });
        }
        os << input.back();
    }
    return os;
}
}


std::ostream& operator<<(std::ostream& os, const cdf::Attribute& attribute)
{
#define PRINT_LAMBDA(type)                                                                         \
    [&os](const std::vector<type>& data) { stream_collection(os, data, ", "); }

    os << attribute.name << ": ";
    cdf::visit(
        attribute, PRINT_LAMBDA(double), PRINT_LAMBDA(float), PRINT_LAMBDA(uint32_t),
        PRINT_LAMBDA(uint16_t), PRINT_LAMBDA(uint8_t), PRINT_LAMBDA(int64_t), PRINT_LAMBDA(int32_t),
        PRINT_LAMBDA(int16_t), PRINT_LAMBDA(int8_t),

        PRINT_LAMBDA(char),
        [&os](const std::vector<tt2000_t>&) { os << "CDF_TT2000: (unprinted)"; },
        [&os](const std::vector<epoch>&) { os << "CDF_EPOCH: (unprinted)"; },

        [&os](const std::vector<epoch16>&) { os << "CDF_EPOCH16: (unprinted)"; },
        [&os](const std::string& data) { os << data; },
        [&os](const cdf_none&) { os << "CDF_NONE"; });
    os << std::endl;
    return os;
}


std::ostream& operator<<(std::ostream& os, const cdf::Variable& var)
{
    os << var.name() << ": (";
    stream_collection(os, var.shape(), ", ");
    os << ") [" << cdf_type_str(var.type()) << "]" << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const cdf::CDF& cdf_file)
{
    os << "CDF:" << std::endl << "\nAttributes:\n";
    std::for_each(std::cbegin(cdf_file.attributes), std::cend(cdf_file.attributes),
        [&os](const auto& item) { os << "\t" << item.second; });
    os << "\nVariables:\n";
    std::for_each(std::cbegin(cdf_file.variables), std::cend(cdf_file.variables),
        [&os](const auto& item) { os << "\t" << item.second; });
    os << std::endl;

    return os;
}


template <typename T>
std::string __repr__(T& obj)
{
    std::stringstream sstr;
    sstr << obj;
    return sstr.str();
}
