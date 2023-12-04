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
#include "cdf-data.hpp"
#include "cdf-repr.hpp"
#include <iomanip>
#include <string>
#include <variant>

namespace cdf
{
struct Attribute
{
    using attr_data_t = std::vector<data_t>;
    using iterator = attr_data_t::iterator;
    using value_type = attr_data_t::value_type;
    using const_iterator = attr_data_t::const_iterator;
    using reference = attr_data_t::reference;
    using const_reference = attr_data_t::const_reference;
    using size_type = attr_data_t::size_type;
    using difference_type = attr_data_t::difference_type;
    using pointer = attr_data_t::pointer;
    using const_pointer = attr_data_t::const_pointer;
    using reverse_iterator = attr_data_t::reverse_iterator;
    using const_reverse_iterator = attr_data_t::const_reverse_iterator;


    std::string name;
    Attribute() = default;
    Attribute(const Attribute&) = default;
    Attribute(Attribute&&) = default;
    Attribute& operator=(Attribute&&) = default;
    Attribute& operator=(const Attribute&) = default;
    Attribute(const std::string& name, attr_data_t&& data) : name { name }
    {
        this->data = std::move(data);
    }

    inline bool operator==(const Attribute& other) const
    {
        return other.name == name && other.data == data;
    }

    inline bool operator!=(const Attribute& other) const { return !(*this == other); }

    template <CDF_Types type>
    [[nodiscard]] inline decltype(auto) get(std::size_t index)
    {
        return data[index].get<type>();
    }

    template <CDF_Types type>
    [[nodiscard]] inline decltype(auto) get(std::size_t index) const
    {
        return data[index].get<type>();
    }

    template <typename type>
    [[nodiscard]] inline decltype(auto) get(std::size_t index)
    {
        return data[index].get<type>();
    }

    template <typename type>
    [[nodiscard]] inline decltype(auto) get(std::size_t index) const
    {
        return data[index].get<type>();
    }

    inline void swap(attr_data_t& new_data) { std::swap(data, new_data); }

    inline Attribute& operator=(attr_data_t& new_data)
    {
        data = new_data;
        return *this;
    }

    inline Attribute& operator=(attr_data_t&& new_data)
    {
        data = new_data;
        return *this;
    }
    [[nodiscard]] inline std::size_t size() const noexcept { return std::size(data); }
    [[nodiscard]] inline data_t& operator[](std::size_t index) { return data[index]; }
    [[nodiscard]] inline const data_t& operator[](std::size_t index) const { return data[index]; }

    inline void push_back(const data_t& value) { data.push_back(value); }

    inline void push_back(data_t&& value) { data.push_back(std::move(value)); }

    template <class... Args>
    auto emplace_back(Args&&... args)
    {
        return data.emplace_back(std::forward<Args>(args)...);
    }

    template <typename... Ts>
    friend void visit(Attribute& attr, Ts... lambdas);

    template <typename... Ts>
    friend void visit(const Attribute& attr, Ts... lambdas);

    [[nodiscard]] inline auto begin() { return data.begin(); }
    [[nodiscard]] inline auto end() { return data.end(); }

    [[nodiscard]] inline auto begin() const { return data.begin(); }
    [[nodiscard]] inline auto end() const { return data.end(); }

    [[nodiscard]] inline auto cbegin() const { return data.cbegin(); }
    [[nodiscard]] inline auto cend() const { return data.cend(); }

    [[nodiscard]] inline data_t& back() { return data.back(); }
    [[nodiscard]] inline const data_t& back() const { return data.back(); }

    [[nodiscard]] inline data_t& front() { return data.front(); }
    [[nodiscard]] inline const data_t& front() const { return data.front(); }

    template <class stream_t>
    inline stream_t& __repr__(stream_t& os, indent_t indent = {}) const
    {
        if (std::size(*this) == 1
            and (front().type() == cdf::CDF_Types::CDF_CHAR
                or front().type() == cdf::CDF_Types::CDF_UCHAR))
        {
            os << indent << name << ": " << front() << std::endl;
        }
        else
        {
            os << indent << name << ": [ ";
            stream_collection(os, *this, ", ");
            os << " ]" << std::endl;
        }
        return os;
    }

private:
    attr_data_t data;
};

struct VariableAttribute
{
    using attr_data_t = data_t;
    std::string name;
    VariableAttribute() = default;
    VariableAttribute(const VariableAttribute&) = default;
    VariableAttribute(VariableAttribute&&) = default;
    VariableAttribute& operator=(VariableAttribute&&) = default;
    VariableAttribute& operator=(const VariableAttribute&) = default;
    VariableAttribute(const std::string& name, attr_data_t&& data) : name { name }
    {
        this->data = std::move(data);
    }

    inline bool operator==(const VariableAttribute& other) const
    {
        return other.name == name && other.data == data;
    }

    inline bool operator!=(const VariableAttribute& other) const { return !(*this == other); }

    inline CDF_Types type() const noexcept { return data.type(); }

    template <CDF_Types type>
    [[nodiscard]] inline decltype(auto) get()
    {
        return data.get<type>();
    }

    template <CDF_Types type>
    [[nodiscard]] inline decltype(auto) get() const
    {
        return data.get<type>();
    }

    template <typename type>
    [[nodiscard]] inline decltype(auto) get()
    {
        return data.get<type>();
    }

    template <typename type>
    [[nodiscard]] inline decltype(auto) get() const
    {
        return data.get<type>();
    }

    inline void swap(data_t& new_data) { std::swap(data, new_data); }

    inline VariableAttribute& operator=(attr_data_t& new_data)
    {
        data = new_data;
        return *this;
    }

    inline VariableAttribute& operator=(attr_data_t&& new_data)
    {
        data = new_data;
        return *this;
    }

    inline data_t& operator*() { return data; }
    inline const data_t& operator*()const { return data; }

    [[nodiscard]] inline data_t& value() { return data; }
    [[nodiscard]] inline const data_t& value() const { return data; }

    template <typename... Ts>
    friend void visit(Attribute& attr, Ts... lambdas);

    template <typename... Ts>
    friend void visit(const Attribute& attr, Ts... lambdas);

    template <class stream_t>
    inline stream_t& __repr__(stream_t& os, indent_t indent = {}) const
    {
        os << indent << name << ": " << data << std::endl;
        return os;
    }

private:
    data_t data;
};

template <typename... Ts>
void visit(Attribute& attr, Ts... lambdas)
{
    std::for_each(std::cbegin(attr.data), std::cend(attr.data),
        [lambdas...](const auto& element) { visit(element, lambdas...); });
}

template <typename... Ts>
void visit(const Attribute& attr, Ts... lambdas)
{
    std::for_each(std::cbegin(attr.data), std::cend(attr.data),
        [lambdas...](const auto& element) { visit(element, lambdas...); });
}
} // namespace cdf


template <class stream_t>
inline stream_t& operator<<(stream_t& os, const cdf::Attribute& attribute)
{
    return attribute.template __repr__<stream_t>(os);
}

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const cdf::VariableAttribute& attribute)
{
    return attribute.template __repr__<stream_t>(os);
}
