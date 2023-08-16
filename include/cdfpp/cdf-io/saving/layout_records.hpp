#pragma once
/*------------------------------------------------------------------------------
-- This file is a part of the CDFpp library
-- Copyright (C) 2023, Plasma Physics Laboratory - CNRS
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
#pragma once

#include "../desc-records.hpp"
#include "./records-saving.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <utility>

namespace cdf::io
{

namespace saving
{

    template <typename T>
    std::size_t layout(record_wrapper<T>& r, std::size_t offset)
    {
        r.offset = offset;
        return offset + r.size;
    }

    template <typename T, typename U>
    std::size_t layout(T&& items, std::size_t offset, U&& function)
    {
        for (auto& item : items)
        {
            offset = layout(item, offset);
            function(item, offset);
        }
        return offset;
    }


    template <typename T>
    auto layout(T&& items, std::size_t offset) -> decltype(std::cbegin(items), std::size_t())
    {
        for (auto& item : items)
        {
            offset = layout(item, offset);
        }
        return offset;
    }


    std::size_t layout(std::vector<variable_ctx::values_records_t>& items, std::size_t offset)
    {
        for (auto& item : items)
        {
            visit(item, [&offset](auto& v) { offset = layout(v, offset); });
        }
        return offset;
    }

    [[nodiscard]] inline std::size_t map_records(saving_context& svg_ctx)
    {
        if (svg_ctx.ccr)
        {
            svg_ctx.ccr->offset = 8;
        }
        auto offset = layout(svg_ctx.body.cdr, 8);
        offset = layout(svg_ctx.body.gdr, offset);
        for (auto& fac : svg_ctx.body.file_attributes)
        {
            offset = layout(fac.adr, offset);
            offset = layout(fac.aedrs, offset);
        }
        for (auto& vc : svg_ctx.body.variables)
        {
            offset = layout(vc.vdr, offset);
            offset = layout(vc.vxrs, offset);
            if (vc.cpr)
            {
                offset = layout(vc.cpr.value(), offset);
            }
            offset = layout(vc.values_records, offset);
        }
        for (auto& [name, vac] : svg_ctx.body.variable_attributes)
        {
            offset = layout(vac.adr, offset);
            offset = layout(vac.aedrs, offset);
        }
        return offset;
    }


} // namespace

}
