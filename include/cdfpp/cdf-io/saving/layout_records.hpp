/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2024, Laboratory of Plasma Physics- CNRS
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the “Software”), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is furnished to do
-- so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
-- INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
-- PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
-- HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
-- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
-- SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
