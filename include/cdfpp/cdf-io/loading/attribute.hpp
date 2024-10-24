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
#include "../common.hpp"
#include "../desc-records.hpp"
#include "./records-loading.hpp"
#include "cdfpp/attribute.hpp"

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
            Attribute::attr_data_t data = [&, &ADR = ADR]() -> Attribute::attr_data_t
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
