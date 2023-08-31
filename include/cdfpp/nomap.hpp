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
#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace details
{
template <typename T>
using base_type_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T, bool is_const>
using ref_t = std::conditional_t<is_const, const base_type_t<T>&, base_type_t<T>&>;
}

template <typename Key, typename T>
struct nomap_node : public std::pair<Key, details::base_type_t<T>>
{
    using key_type = Key;
    using mapped_type = details::base_type_t<T>;


    nomap_node() : p_empty { true } { }
    nomap_node(Key&& k, mapped_type&& v)
            : std::pair<Key, mapped_type> { std::move(k), std::move(v) }
    {
    }
    nomap_node(const Key& k, const mapped_type& v) : std::pair<Key, mapped_type> { k, v } { }
    nomap_node(const Key& k, mapped_type&& v) : std::pair<Key, mapped_type> { k, std::move(v) } { }

    nomap_node(nomap_node&&) = default;
    nomap_node(const nomap_node&) = default;
    nomap_node& operator=(const nomap_node&) = default;
    nomap_node& operator=(nomap_node&&) = default;

    [[nodiscard]] inline auto& key() const noexcept { return this->first; }
    [[nodiscard]] inline auto& mapped() const noexcept { return this->second; }
    [[nodiscard]] inline bool empty() const noexcept { return this->p_empty; };

    friend void swap(nomap_node& lhs, nomap_node& rhs)
    {
        std::swap(lhs.first, rhs.second);
        std::swap(lhs.second, rhs.second);
        std::swap(lhs.p_empty, rhs.p_empty);
    }

private:
    bool p_empty = false;
};

namespace std
{
template <typename Key, typename T>
struct tuple_size<nomap_node<Key, T>> : std::integral_constant<size_t, 2>
{
};
template <typename Key, typename T>
struct tuple_element<0, nomap_node<Key, T>>
{
    using type = Key;
};
template <typename Key, typename T>
struct tuple_element<1, nomap_node<Key, T>>
{
    using type = details::base_type_t<T>;
};

}

template <typename Key, typename T>
struct nomap
{
    using key_type = details::base_type_t<Key>;
    using mapped_type = details::base_type_t<T>;
    using value_type = nomap_node<key_type, mapped_type>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = mapped_type&;
    using const_reference = const mapped_type&;
    using iterator = decltype(std::declval<std::vector<value_type>>().begin());
    using const_iterator = decltype(std::declval<std::vector<value_type>>().cbegin());

    nomap() = default;
    nomap(nomap&&) = default;
    nomap(const nomap&) = default;
    nomap& operator=(const nomap&) = default;
    nomap& operator=(nomap&&) = default;

    inline bool operator==(const nomap& other) const
    {
        for (const auto& node : p_nodes)
        {
            if (!other.count(node.key()))
                return false;
            if (other[node.key()] != node.mapped())
                return false;
        }
        return true;
    }

    inline bool operator!=(const nomap& other) const { return !(*this == other); }

    [[nodiscard]] inline bool empty() const noexcept { return p_nodes.empty(); }
    [[nodiscard]] inline size_type size() const noexcept { return std::size(p_nodes); }

    [[nodiscard]] inline size_type max_size() const noexcept { return p_nodes.max_size(); }

    inline void clear() noexcept { p_nodes.clear(); }

    [[nodiscard]] inline T& at(const key_type& key)
    {
        for (auto i = 0UL; i < std::size(p_nodes); i++)
        {
            if (p_nodes[i].first == key)
                return p_nodes[i].second;
        }

        throw std::out_of_range { "Key not found" };
    }

    [[nodiscard]] inline const T& at(const key_type& key) const
    {
        for (auto i = 0UL; i < std::size(p_nodes); i++)
        {
            if (p_nodes[i].first == key)
                return p_nodes[i].second;
        }

        throw std::out_of_range { "Key not found" };
    }

    [[nodiscard]] inline T& operator[](const key_type& key)
    {
        for (auto i = 0UL; i < std::size(p_nodes); i++)
        {
            if (p_nodes[i].first == key)
                return p_nodes[i].second;
        }
        auto& node = p_nodes.emplace_back(key, mapped_type {});
        return node.second;
    }

    [[nodiscard]] inline T& operator[](key_type&& key)
    {
        for (auto i = 0UL; i < std::size(p_nodes); i++)
        {
            if (p_nodes[i].first == key)
                return p_nodes[i].second;
        }
        auto& node = p_nodes.emplace_back(std::move(key), mapped_type {});
        return node.second;
    }

    [[nodiscard]] inline const T& operator[](const key_type& key) const { return this->at(key); }

    [[nodiscard]] inline iterator begin() { return std::begin(p_nodes); }

    [[nodiscard]] inline iterator end() { return std::end(p_nodes); }

    [[nodiscard]] inline auto rbegin() { return std::rbegin(p_nodes); }

    [[nodiscard]] inline auto rend() { return std::rend(p_nodes); }

    [[nodiscard]] inline const_iterator begin() const { return std::cbegin(p_nodes); }

    [[nodiscard]] inline const_iterator end() const { return std::cend(p_nodes); }

    [[nodiscard]] inline auto rbegin() const { return std::crbegin(p_nodes); }

    [[nodiscard]] inline auto rend() const { return std::crend(p_nodes); }

    [[nodiscard]] inline const_iterator cbegin() const { return this->p_nodes.cbegin(); }

    [[nodiscard]] inline const_iterator cend() const { return this->p_nodes.cend(); }

    [[nodiscard]] inline auto crbegin() const { return this->p_nodes.crbegin(); }

    [[nodiscard]] inline auto crend() const { return this->p_nodes.crend(); }

    [[nodiscard]] inline value_type extract(const_iterator position)
    {
        if (position != cend())
            return _extract(position - p_nodes.cbegin());
        return {};
    }

    inline iterator erase(iterator position)
    {
        if (position != end())
        {
            auto next_idx = (position - begin());
            std::swap(*position, p_nodes.back());
            p_nodes.pop_back();
            return begin() + next_idx;
        }
        return end();
    }

    inline const_iterator erase(const_iterator position)
    {
        if (position != cend())
        {
            auto next_idx = (position - cbegin());
            std::swap(*position, p_nodes.back());
            p_nodes.pop_back();
            return cbegin() + next_idx;
        }
        return cend();
    }

    inline iterator erase(const_iterator first, const_iterator last)
    {
        if (first != cend() and last != cend() and first <= last)
        {
            auto next_idx = (first - cbegin());
            auto count = last - first;
            while (count--)
            {
                std::swap(p_nodes[next_idx], p_nodes.back());
                p_nodes.pop_back();
            }
            return cbegin() + next_idx;
        }
        return end();
    }

    [[nodiscard]] inline value_type extract(const key_type& key)
    {
        for (auto i = 0UL; i < std::size(p_nodes); i++)
        {
            if (p_nodes[i].first == key)
                return _extract(i);
        }
        return {};
    }

    friend void swap(nomap& lhs, nomap& rhs) { std::swap(lhs.p_nodes, rhs.p_nodes); }

    [[nodiscard]] auto find(const key_type& key)
    {
        return std::find_if(std::begin(p_nodes), std::end(p_nodes),
            [&key](const auto& node) { return node.first == key; });
    }

    [[nodiscard]] auto find(const key_type& key) const
    {
        return std::find_if(std::cbegin(p_nodes), std::cend(p_nodes),
            [&key](const auto& node) { return node.first == key; });
    }

    [[nodiscard]] size_type count(const Key& key) const
    {
        if (this->find(key) == this->cend())
            return 0;
        return 1;
    }

    void reserve(size_type count) { p_nodes.reserve(count); }

    template <typename Kt, class... Args>
    std::pair<iterator, bool> emplace(Kt&& key, Args&&... args)
    {
        auto it = find(key);
        if (it == end())
        {
            p_nodes.emplace_back(
                std::forward<Kt>(key), mapped_type { std::forward<Args>(args)... });
            return { p_nodes.end() - 1, true };
        }
        return { it, false };
    }


private:
    inline value_type _extract(std::size_t index)
    {
        std::swap(p_nodes[size() - 1], p_nodes[index]);
        value_type v = std::move(p_nodes[size() - 1]);
        p_nodes.pop_back();
        return v;
    }
    std::vector<value_type> p_nodes;
};
