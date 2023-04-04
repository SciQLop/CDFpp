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
#include "../cdf-endianness.hpp"
#include "../cdf-majority-swap.hpp"
#include "../variable.hpp"
#include <assert.h>
#include <functional>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace cdf::io::common
{
using magic_numbers_t = std::pair<uint32_t, uint32_t>;
using version_t = std::pair<uint8_t, uint8_t>;

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
auto is_compressed(const T& vdr) noexcept -> decltype(vdr.Flags.value, true)
{
    return (vdr.Flags.value & 4) == 4;
}

template <typename T>
auto is_nrv(const T& vdr) noexcept -> decltype(vdr.Flags.value, true)
{
    return (vdr.Flags.value & 1) == 0;
}

template <typename T>
auto majority(const T& cdr)
{
    if (cdr.Flags.value & 1)
        return cdf_majority::row;
    return cdf_majority::column;
}

template <typename src_endianess_t, typename buffer_t, typename container_t>
void load_values(buffer_t& buffer, std::size_t offset, container_t& output)
{
    auto data = buffer.read(offset, std::size(output) * sizeof(typename container_t::value_type));
    endianness::decode_v<src_endianess_t>(data.data(), std::size(data), output.data());
}

template <typename value_t, typename stream_t, typename... Args>
struct blk_iterator
{
    using iterator_category = std::forward_iterator_tag;
    using value_type = value_t;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = value_type&;


    std::size_t offset;
    value_t block;
    stream_t& stream;
    std::function<std::size_t(value_t&)> next;
    std::tuple<Args...> load_opt_args;

    blk_iterator(std::size_t offset, stream_t& stream, std::function<std::size_t(value_t&)>&& next,
        Args... args)
            : offset { offset }
            , block { stream }
            , stream { stream }
            , next { std::move(next) }
            , load_opt_args { args... }
    {
        if (offset != 0)
            block.load(offset, args...);
    }

    auto operator==(const blk_iterator& other) { return other.offset == offset; }

    bool operator!=(const blk_iterator& other) { return not(*this == other); }

    blk_iterator& operator+(int n)
    {
        step_forward(n);
        return *this;
    }

    blk_iterator& operator+=(int n)
    {
        step_forward(n);
        return *this;
    }

    blk_iterator& operator++(int n)
    {
        step_forward(n);
        return *this;
    }

    blk_iterator& operator++()
    {
        step_forward();
        return *this;
    }

    template <size_t... Is>
    int wrapper_load(std::size_t offset, std::index_sequence<Is...>)
    {
        return block.load(offset, std::get<Is>(load_opt_args)...);
    }

    void step_forward(int n = 1)
    {
        while (n > 0)
        {
            n--;
            offset = next(block);
            if (offset != 0)
            {
                wrapper_load(offset, std::make_index_sequence<sizeof...(Args)> {});
            }
        }
    }

    const value_type* operator->() const { return &block; }
    const value_type& operator*() const { return block; }
    value_type* operator->() { return &block; }
    value_type& operator*() { return block; }
};

struct cdf_repr
{
    cdf_majority majority;
    std::tuple<uint32_t, uint32_t, uint32_t> distribution_version;
    std::unordered_map<std::string, Variable> variables;
    std::unordered_map<std::string, Attribute> attributes;
    std::unordered_map<std::size_t, std::unordered_map<std::string, Attribute>> var_attributes;
};

void add_global_attribute(cdf_repr& repr, const std::string& name, Attribute::attr_data_t&& data)
{
    repr.attributes[name] = Attribute { name, std::move(data) };
}

void add_var_attribute(cdf_repr& repr, const std::vector<uint32_t>& variable_indexes,
    const std::string& name, Attribute::attr_data_t&& data)
{
    assert(std::size(data) == std::size(variable_indexes));
    std::unordered_map<uint32_t, std::unordered_map<std::string, Attribute::attr_data_t>> storage;
    for (auto index = 0UL; index < std::size(data); index++)
    {
        storage[variable_indexes[index]][name].push_back(data[index]);
    }
    for (auto& [v_index, attr] : storage)
    {
        for (auto& [attr_name, attr_data] : attr)
        {
            repr.var_attributes[v_index][attr_name] = Attribute { attr_name, std::move(attr_data) };
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
    Variable::var_data_t&& data, Variable::shape_t&& shape)
{
    repr.variables[name]
        = Variable { name, number, std::move(data), std::move(shape), repr.majority };
    if (repr.majority == cdf_majority::column)
    {
        majority::swap(repr.variables[name]);
    }
    repr.variables[name].attributes = [&]() -> decltype(Variable::attributes)
    {
        auto attrs = repr.var_attributes.extract(number);
        if (!attrs.empty())
            return attrs.mapped();
        else
            return {};
    }();
}

}
