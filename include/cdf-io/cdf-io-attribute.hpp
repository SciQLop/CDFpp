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
#include "cdf-io-desc-records.hpp"
#include "../attribute.hpp"
#include "../cdf-file.hpp"

namespace cdf::io::attribute
{

template <typename cdf_version_tag_t, typename streamT>
Attribute::attr_data_t load_data(
    std::size_t offset, streamT& stream, std::size_t AEDRCount)
{
    cdf_AEDR_t<cdf_version_tag_t> AEDR;
    if (AEDR.load(stream, offset))
    {
        Attribute::attr_data_t values;
        while (AEDRCount--)
        {
            std::size_t element_size = cdf_type_size(CDF_Types { AEDR.DataType.value });
            auto buffer = read_buffer<std::vector<char>>(
                stream, offset + AEDR.Values.offset, AEDR.NumElements * element_size);
            values.emplace_back(load_values(
                buffer.data(), std::size(buffer), AEDR.DataType.value, cdf_encoding::IBMPC));
            offset = AEDR.AEDRnext.value;
            AEDR.load(stream, offset);
        }
        return values;
    }
    return {};
}

template <typename cdf_version_tag_t, typename streamT>
std::size_t load(std::size_t offset, streamT& stream, CDF& cdf)
{
    cdf_ADR_t<cdf_version_tag_t> ADR;
    if (ADR.load(stream, offset))
    {
        Attribute::attr_data_t data = [&]() -> Attribute::attr_data_t {
            if (ADR.AzEDRhead != 0)
                return load_data<cdf_version_tag_t>(
                    ADR.AzEDRhead, stream, ADR.NzEntries);
            else if (ADR.AgrEDRhead != 0)
                return load_data<cdf_version_tag_t>(
                    ADR.AgrEDRhead, stream, ADR.NgrEntries);
            return {};
        }();
        add_attribute(cdf, ADR.scope.value, ADR.Name.value, std::move(data), ADR.num.value);
        return ADR.ADRnext;
    }
    return 0;
}

template <typename cdf_version_tag_t, typename streamT, typename context_t>
bool load_all(streamT& stream, context_t& context, CDF& cdf)
{
    auto attr_count = context.gdr.NumAttr.value;
    auto next_attr = context.gdr.ADRhead.value;
    while ((attr_count > 0) and (next_attr != 0ul))
    {
        next_attr = load<cdf_version_tag_t>(next_attr, stream, cdf);
        --attr_count;
    }
    return true;
}
}
