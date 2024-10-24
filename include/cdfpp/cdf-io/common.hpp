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
#include "../attribute.hpp"
#include "./endianness.hpp"
#include "./majority-swap.hpp"
#include "cdfpp/cdf-map.hpp"
#include "cdfpp/variable.hpp"
#include <assert.h>
#include <functional>
#include <tuple>
#include <type_traits>
#include <vector>

namespace cdf::io::common
{
using magic_numbers_t = std::pair<uint32_t, uint32_t>;
using version_t = std::pair<uint8_t, uint8_t>;

struct iso_8859_1_to_utf8_t
{
};
struct no_iso_8859_1_to_utf8_t
{
};

template <typename T>
inline constexpr bool with_iso_8859_1_to_utf8 = std::is_same_v<T, iso_8859_1_to_utf8_t>;

inline version_t cdf_version(const magic_numbers_t& magic)
{
    return { (magic.first >> 16) & 0xf, (magic.first >> 12) & 0xf };
}

inline bool is_v3x(const magic_numbers_t& magic)
{
    return cdf_version(magic).first >= 3;
}

inline bool is_cdf(const magic_numbers_t& magic_numbers) noexcept
{
    return (((magic_numbers.first & 0xfff00000) == 0xCDF00000)
               && (magic_numbers.second == 0xCCCC0001 || magic_numbers.second == 0x0000FFFF))
        || (magic_numbers.first == 0x0000FFFF && magic_numbers.second == 0x0000FFFF);
}

template <typename context_t>
bool is_cdf(const context_t& context) noexcept
{
    return is_cdf(context.magic);
}

template <typename T>
auto is_compressed(const T& magic_numbers) noexcept -> decltype(magic_numbers.second, true)
{
    return magic_numbers.second == 0xCCCC0001;
}

template <typename T>
auto is_compressed(const T& vdr) noexcept -> decltype(vdr.Flags, true)
{
    return (vdr.Flags & 4) == 4;
}

template <typename T>
auto is_nrv(const T& vdr) noexcept -> decltype(vdr.Flags, true)
{
    return (vdr.Flags & 1) == 0;
}

template <typename T>
auto majority(const T& cdr)
{
    if (cdr.Flags & 1)
        return cdf_majority::row;
    return cdf_majority::column;
}

template <typename src_endianess_t, typename buffer_t, typename container_t>
void load_values(buffer_t& buffer, std::size_t offset, container_t& output)
{
    buffer.read(reinterpret_cast<char*>(output.data()), offset,
        std::size(output) * sizeof(typename container_t::value_type));
    endianness::decode_v<src_endianess_t>(output.data(), std::size(output));
}


struct cdf_repr
{
    std::tuple<uint32_t, uint32_t, uint32_t> distribution_version;
    cdf_map<std::string, Variable> variables;
    cdf_map<std::string, Attribute> attributes;
    std::vector<cdf_map<std::string, VariableAttribute>> var_attributes;
    cdf_majority majority;
    cdf_compression_type compression_type;
    bool lazy;
    cdf_repr(std::size_t var_count) : var_attributes(var_count) { }
    cdf_repr(cdf_repr&&) = default;
    cdf_repr(const cdf_repr&) = delete;
    cdf_repr& operator=(const cdf_repr&) = delete;
    cdf_repr& operator=(cdf_repr&&) = default;
};

void add_global_attribute(cdf_repr& repr, const std::string& name, Attribute::attr_data_t&& data)
{
    repr.attributes[name] = Attribute { name, std::move(data) };
}

void add_var_attribute(cdf_repr& repr, const std::vector<uint32_t>& variable_indexes,
    const std::string& name, std::vector<VariableAttribute::attr_data_t>&& data)
{
    assert(std::size(data) == std::size(variable_indexes));
    cdf_map<uint32_t, cdf_map<std::string, VariableAttribute::attr_data_t>> storage;
    for (auto index = 0UL; index < std::size(data); index++)
    {
        storage[variable_indexes[index]][name] = data[index];
    }
    for (auto& [v_index, attr] : storage)
    {
        for (auto& [attr_name, attr_data] : attr)
        {
            repr.var_attributes[v_index][attr_name]
                = VariableAttribute { attr_name, std::move(attr_data) };
        }
    }
}

void add_attribute(cdf_repr& repr, cdf_attr_scope scope, const std::string& name,
    Attribute::attr_data_t&& data, const std::vector<uint32_t>& variable_indexes)
{
    if (scope == cdf_attr_scope::global || scope == cdf_attr_scope::global_assumed)
        add_global_attribute(repr, name, std::move(data));
    else if (scope == cdf_attr_scope::variable || scope == cdf_attr_scope::variable_assumed)
    {
        add_var_attribute(repr, variable_indexes, name, std::move(data));
    }
}

void add_variable(cdf_repr& repr, const std::string& name, std::size_t number,
    Variable::var_data_t&& data, Variable::shape_t&& shape, bool is_nrv,
    cdf_compression_type compression_type)
{
    repr.variables[name] = Variable { name, number, std::move(data), std::move(shape),
        repr.majority, is_nrv, compression_type };
    repr.variables[name].attributes = [&]() -> decltype(Variable::attributes)
    { return std::move(repr.var_attributes[number]); }();
}

void add_lazy_variable(cdf_repr& repr, const std::string& name, std::size_t number,
    lazy_data&& data, Variable::shape_t&& shape, bool is_nrv, cdf_compression_type compression_type)
{
    repr.variables[name] = Variable { name, number, std::move(data), std::move(shape),
        repr.majority, is_nrv, compression_type };
    repr.variables[name].attributes = [&]() -> decltype(Variable::attributes)
    { return std::move(repr.var_attributes[number]); }();
}

}
