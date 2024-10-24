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
#include "cdfpp/no_init_vector.hpp"
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
    void link_aedrs(T& fac)
    {
        if (std::size(fac.aedrs))
        {
            if constexpr (std::is_same_v<T, file_attribute_ctx>)
                fac.adr.record.AgrEDRhead = fac.aedrs.front().offset;
            else
                fac.adr.record.AzEDRhead = fac.aedrs.front().offset;
            auto last_offset = 0;
            std::for_each(std::rbegin(fac.aedrs), std::rend(fac.aedrs),
                [&last_offset](auto& aedr)
                {
                    aedr.record.AEDRnext = last_offset;
                    last_offset = aedr.offset;
                });
        }
    }

    inline void link_adrs(saving_context& svg_ctx)
    {
        auto last_offset = 0;
        std::for_each(std::rbegin(svg_ctx.body.variable_attributes),
            std::rend(svg_ctx.body.variable_attributes),
            [&last_offset](auto& node)
            {
                auto& [name, vac] = node;
                vac.adr.record.ADRnext = last_offset;
                last_offset = vac.adr.offset;
                link_aedrs(vac);
            });
        if (std::size(svg_ctx.body.file_attributes) >= 2)
        {
            for (auto i = 0UL; i < std::size(svg_ctx.body.file_attributes) - 1; i++)
            {
                svg_ctx.body.file_attributes[i].adr.record.ADRnext
                    = svg_ctx.body.file_attributes[i + 1].adr.offset;
                link_aedrs(svg_ctx.body.file_attributes[i]);
            }
        }
        if (std::size(svg_ctx.body.file_attributes))
        {
            svg_ctx.body.file_attributes.back().adr.record.ADRnext = last_offset;
            link_aedrs(svg_ctx.body.file_attributes.back());
        }
    }

    inline void link_vxrs(variable_ctx& vc)
    {
        auto last_offset = 0;
        auto last_vvr = vc.values_records.rbegin();
        std::for_each(std::rbegin(vc.vxrs), std::rend(vc.vxrs),
            [&last_offset, &last_vvr](record_wrapper<cdf_VXR_t<v3x_tag>>& vxr)
            {
                vxr.record.VXRnext = last_offset;
                last_offset = vxr.offset;
                std::for_each(std::rbegin(vxr.record.Offset.values),
                    std::rend(vxr.record.Offset.values),
                    [&last_vvr](auto& offset)
                    {
                        visit(*last_vvr,
                            [&offset](const auto& value_rec) { offset = value_rec.offset; });
                        last_vvr += 1;
                    });
            });
    }

    inline void link_vdrs(saving_context& svg_ctx)
    {
        auto last_offset = 0;
        std::for_each(std::rbegin(svg_ctx.body.variables), std::rend(svg_ctx.body.variables),
            [&last_offset](variable_ctx& vc)
            {
                vc.vdr.record.VDRnext = last_offset;
                last_offset = vc.vdr.offset;
                if (std::size(vc.vxrs) >= 1)
                {
                    vc.vdr.record.VXRhead = vc.vxrs.front().offset;
                    vc.vdr.record.VXRtail = vc.vxrs.back().offset;
                    if (vc.cpr)
                    {
                        vc.vdr.record.CPRorSPRoffset = vc.cpr.value().offset;
                    }
                    link_vxrs(vc);
                }
            });
    }

    inline void link_records(saving_context& svg_ctx)
    {
        svg_ctx.body.cdr.record.GDRoffset = svg_ctx.body.gdr.offset;
        if (std::size(svg_ctx.body.file_attributes))
        {
            svg_ctx.body.gdr.record.ADRhead = svg_ctx.body.file_attributes.front().adr.offset;
        }
        else
        {
            if (std::size(svg_ctx.body.variable_attributes))
            {
                svg_ctx.body.gdr.record.ADRhead
                    = std::cbegin(svg_ctx.body.variable_attributes)->second.adr.offset;
            }
        }
        if (std::size(svg_ctx.body.variables))
        {
            svg_ctx.body.gdr.record.zVDRhead = svg_ctx.body.variables.front().vdr.offset;
        }
        link_adrs(svg_ctx);
        link_vdrs(svg_ctx);
    }


} // namespace


}
