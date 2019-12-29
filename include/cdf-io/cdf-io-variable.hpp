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
#include "../cdf-endianness.hpp"
#include "../cdf-file.hpp"
#include "cdf-io-common.hpp"
#include "cdf-io-desc-records.hpp"

namespace cdf::io::variable
{
namespace
{

    template <typename cdf_version_tag_t, typename stream_t>
    std::size_t load(std::size_t offset, stream_t& stream, CDF& cdf)
    {
        cdf_VDR_t<cdf_version_tag_t, stream_t> VDR { stream, offset };
        if (VDR.is_loaded)
        {
            return VDR.VDRnext.value;
        }
        return 0;
    }

    template <cdf_r_z type, typename cdf_version_tag_t, typename stream_t, typename context_t>
    bool load_all_Vars(stream_t& stream, context_t& context, CDF& cdf)
    {

        std::for_each(common::begin_VDR<type>(context.gdr), common::end_VDR<type>(context.gdr),
            [&](const cdf_VDR_t<cdf_version_tag_t, stream_t>& vdr) {
                auto dim_sizes = [&]() -> std::vector<uint32_t> {
                    if constexpr (type == cdf_r_z::z)
                    {
                        std::vector<uint32_t> sizes(vdr.zNumDims.value);
                        if (vdr.zNumDims.value)
                        {
                            std::size_t offset = vdr.offset + AFTER(vdr.zNumDims);
                            common::load_values<endianness::big_endian_t>(
                                stream, offset, sizes);
                        }
                        return sizes;
                    }
                    else
                    {
                        return {};
                    }
                }();

                if (vdr.is_loaded)
                {
                    std::for_each(common::begin_VXR(vdr), common::end_VXR(vdr),
                        [&](const cdf_VXR_t<cdf_version_tag_t, stream_t>& vxr) {});
                }
            });
        return true;
    }
}

template <typename cdf_version_tag_t, typename stream_t, typename context_t>
bool load_all(stream_t& stream, context_t& context, CDF& cdf)
{
    return load_all_Vars<cdf_r_z::r, cdf_version_tag_t>(stream, context, cdf)
        & load_all_Vars<cdf_r_z::z, cdf_version_tag_t>(stream, context, cdf);
}

}
