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
#include "cdfpp/attribute.hpp"
#include "../common.hpp"
#include "../desc-records.hpp"
#include "./records-loading.hpp"

namespace cdf::io::attribute
{

template <cdf_r_z type, typename cdf_version_tag_t, bool iso_8859_1_to_utf8, typename ADR_t,
    typename context_t>
Attribute::attr_data_t load_data(
    context_t& context, const ADR_t& ADR, std::vector<uint32_t>& var_num)
{
    Attribute::attr_data_t values;
    std::for_each(begin_AEDR<type>(ADR, context), end_AEDR<type>(ADR, context),
        [&](auto& blk)
        {
            auto& [offset, AEDR] = blk;
            std::size_t element_size = cdf_type_size(CDF_Types { AEDR.DataType });
            data_t data
                = new_data_container(AEDR.NumElements * element_size, CDF_Types { AEDR.DataType });
            context.buffer.read(
                data.bytes_ptr(), offset + packed_size(AEDR), AEDR.NumElements * element_size);
            values.emplace_back(
                load_values<iso_8859_1_to_utf8>(std::move(data), context.encoding()));
            var_num.push_back(AEDR.Num);
        });
    return values;
}

template <typename cdf_version_tag_t, bool iso_8859_1_to_utf8, typename context_t>
bool load_all(context_t& context, common::cdf_repr& repr)
{
    std::for_each(begin_ADR(context), end_ADR(context),
        [&](auto& blk)
        {
            auto& [offset, ADR] = blk;
            std::vector<uint32_t> var_nums;
            Attribute::attr_data_t data = [&,&ADR=ADR]() -> Attribute::attr_data_t
            {
                if (ADR.AzEDRhead != 0)
                    return load_data<cdf_r_z::z, cdf_version_tag_t, iso_8859_1_to_utf8>(
                        context, ADR, var_nums);
                else if (ADR.AgrEDRhead != 0)
                    return load_data<cdf_r_z::r, cdf_version_tag_t, iso_8859_1_to_utf8>(
                        context, ADR, var_nums);
                return {};
            }();
            common::add_attribute(repr, ADR.scope, ADR.Name.value, std::move(data), var_nums);
        });
    return true;
}
} // namespace cdf::io::attribute
