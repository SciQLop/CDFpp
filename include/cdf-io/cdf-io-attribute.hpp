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
#include "../attribute.hpp"
#include "../cdf-file.hpp"
#include "cdf-io-common.hpp"
#include "cdf-io-desc-records.hpp"

namespace cdf::io::attribute
{

template <cdf_r_z type, typename cdf_version_tag_t, typename ADR_t, typename streamT>
Attribute::attr_data_t load_data(const ADR_t& ADR, streamT& stream)
{
    Attribute::attr_data_t values;
    std::for_each(common::begin_AEDR<type>(ADR), common::end_AEDR<type>(ADR), [&](auto& AEDR) {
        std::size_t element_size = cdf_type_size(CDF_Types { AEDR.DataType.value });
        auto buffer = read_buffer<std::vector<char>>(
            stream, AEDR.offset + AEDR.Values.offset, AEDR.NumElements * element_size);
        values.emplace_back(load_values(
            buffer.data(), std::size(buffer), AEDR.DataType.value, cdf_encoding::IBMPC));
    });
    return values;
}

template <typename cdf_version_tag_t, typename streamT, typename context_t>
bool load_all(streamT& stream, context_t& context, CDF& cdf)
{
    std::for_each(common::begin_ADR(context.gdr), common::end_ADR(context.gdr), [&](auto& ADR) {
        if (ADR.is_loaded)
        {
            Attribute::attr_data_t data = [&]() -> Attribute::attr_data_t {
                if (ADR.AzEDRhead != 0)
                    return load_data<cdf_r_z::z, cdf_version_tag_t>(ADR, stream);
                else if (ADR.AgrEDRhead != 0)
                    return load_data<cdf_r_z::r, cdf_version_tag_t>(ADR, stream);
                return {};
            }();
            add_attribute(cdf, ADR.scope.value, ADR.Name.value, std::move(data), ADR.num.value);
        }
    });
    return true;
}
}
