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
#include "../cdf-file.hpp"
#include "cdf-io-common.hpp"
#include "cdf-io-desc-records.hpp"

namespace cdf::io::variable
{
namespace
{

    template <typename cdf_version_tag_t, typename streamT>
    std::size_t load(std::size_t offset, streamT& stream, CDF& cdf)
    {
        cdf_VDR_t<cdf_version_tag_t, streamT> VDR { stream, offset };
        if (VDR.is_loaded)
        {
            return VDR.VDRnext.value;
        }
        return 0;
    }
    template <typename cdf_version_tag_t, typename streamT, typename context_t>
    bool load_all_zVars(streamT& stream, context_t& context, CDF& cdf)
    {
        std::for_each(
            common::begin_VDR<cdf_r_z::z>(context.gdr), common::end_VDR<cdf_r_z::z>(context.gdr), [&](auto& VDR) {
                if (VDR.is_loaded)
                {
                }
            });
        return true;
    }
    template <typename cdf_version_tag_t, typename streamT, typename context_t>
    bool load_all_rVars(streamT& stream, context_t& context, CDF& cdf)
    {
        std::for_each(
            common::begin_VDR<cdf_r_z::r>(context.gdr), common::end_VDR<cdf_r_z::r>(context.gdr), [&](auto& VDR) {
                if (VDR.is_loaded)
                {
                }
            });
        return true;
    }
}

template <typename cdf_version_tag_t, typename streamT, typename context_t>
bool load_all(streamT& stream, context_t& context, CDF& cdf)
{
    return load_all_rVars<cdf_version_tag_t>(stream, context, cdf)
        & load_all_zVars<cdf_version_tag_t>(stream, context, cdf);
}

}
