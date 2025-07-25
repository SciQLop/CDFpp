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
#include "attribute.hpp"
#include "cdf-data.hpp"
#include "cdf-enums.hpp"
#include "cdf-io/majority-swap.hpp"
#include "cdf-map.hpp"
#include "cdf-repr.hpp"
#include "no_init_vector.hpp"

#include <cstdint>
#include <iomanip>
#include <optional>
#include <vector>

template <class stream_t>
inline stream_t& operator<<(
    stream_t& os, const cdf_map<std::string, cdf::VariableAttribute>& attributes)
{
    std::for_each(std::cbegin(attributes), std::cend(attributes),
        [&os](const auto& item) { item.second.__repr__(os, indent_t {}); });
    return os;
}

namespace cdf
{

[[nodiscard]] std::size_t flat_size(const no_init_vector<uint32_t>& shape) noexcept
{
    if (std::size(shape) > 0)
    {
        return std::accumulate(
            std::cbegin(shape), std::cend(shape), 1UL, std::multiplies<std::size_t>());
    }
    return 0UL;
}

template <typename T>
[[nodiscard]] std::size_t flat_size(T&& cbegin, T&& cend) noexcept
{
    if (cbegin != cend)
    {
        return std::accumulate(cbegin, cend, 1UL, std::multiplies<std::size_t>());
    }
    return 0UL;
}

struct Variable
{
    using var_data_t = data_t;
    using shape_t = no_init_vector<uint32_t>;
    cdf_map<std::string, VariableAttribute> attributes;
    Variable() = default;
    Variable(Variable&&) = default;
    Variable(const Variable&) = default;
    Variable& operator=(const Variable&) = default;
    Variable& operator=(Variable&&) = default;
    Variable(const std::string& name, std::size_t number, var_data_t&& data, shape_t&& shape,
        cdf_majority majority = cdf_majority::row, bool is_nrv = false,
        cdf_compression_type compression_type = cdf_compression_type::no_compression)
            : p_name { name }
            , p_number { number }
            , p_data { std::move(data) }
            , p_shape { shape }
            , p_majority { majority }
            , p_is_nrv { is_nrv }
            , p_compression { compression_type }
    {
        if (this->majority() == cdf_majority::column)
        {
            majority::swap(_data(), p_shape);
        }
        check_shape();
    }

    Variable(const std::string& name, std::size_t number, lazy_data&& data, shape_t&& shape,
        cdf_majority majority = cdf_majority::row, bool is_nrv = false,
        cdf_compression_type compression_type = cdf_compression_type::no_compression)
            : p_name { name }
            , p_number { number }
            , p_data { std::move(data) }
            , p_shape { shape }
            , p_majority { majority }
            , p_is_nrv { is_nrv }
            , p_compression { compression_type }
    {
    }

    inline bool operator==(const Variable& other) const
    {
        return other.p_name == p_name && other.p_is_nrv == p_is_nrv
            && other.p_compression == p_compression && other.p_shape == p_shape
            && other.attributes == attributes && other._data() == _data();
    }

    inline bool operator!=(const Variable& other) const { return !(*this == other); }


    template <CDF_Types type>
    [[nodiscard]] decltype(auto) get()
    {
        return _data().get<type>();
    }

    template <CDF_Types type>
    [[nodiscard]] decltype(auto) get() const
    {
        return _data().get<type>();
    }

    template <typename type>
    [[nodiscard]] decltype(auto) get()
    {
        return _data().get<type>();
    }

    template <typename type>
    [[nodiscard]] decltype(auto) get() const
    {
        return _data().get<type>();
    }

    [[nodiscard]] const std::string& name() const noexcept { return p_name; }

    [[nodiscard]] const shape_t& shape() const noexcept { return p_shape; }
    [[nodiscard]] std::size_t len() const noexcept
    {
        if (std::size(p_shape) >= 1)
            return p_shape[0];
        return 0;
    }

    void set_data(const Variable& source)
    {
        p_data = source._data();
        p_shape = source.p_shape;
        p_is_nrv = source.p_is_nrv;
        p_majority = source.p_majority;
        p_compression = source.p_compression;
        check_shape();
    }

    void set_data(const data_t& data, const shape_t& shape)
    {
        p_data = data;
        p_shape = shape;
        check_shape();
    }

    void set_data(data_t&& data, shape_t&& shape)
    {
        p_data = std::move(data);
        p_shape = std::move(shape);
        check_shape();
    }

    [[nodiscard]] std::size_t bytes() const noexcept
    {
        if (std::size(p_shape))
            return flat_size(p_shape) * cdf_type_size(this->type());
        else
            return 0UL;
    }

    [[nodiscard]] const char* bytes_ptr() const noexcept { return _data().bytes_ptr(); }
    [[nodiscard]] char* bytes_ptr() noexcept { return _data().bytes_ptr(); }

    [[nodiscard]] CDF_Types type() const
    {
        if (std::holds_alternative<var_data_t>(p_data))
            return std::get<var_data_t>(p_data).type();
        return std::get<lazy_data>(p_data).type();
    }

    [[nodiscard]] bool is_nrv() const noexcept { return p_is_nrv; }
    [[nodiscard]] std::size_t number() const noexcept { return p_number; }
    [[nodiscard]] cdf_majority majority() const noexcept { return p_majority; }
    [[nodiscard]] cdf_compression_type compression_type() const noexcept { return p_compression; }
    void set_compression_type(cdf_compression_type ct) noexcept { p_compression = ct; }

    [[nodiscard]] inline bool values_loaded() const noexcept
    {
        return not std::holds_alternative<lazy_data>(p_data);
    }

    inline void load_values() const
    {
        if (not values_loaded())
        {
            p_data = std::get<lazy_data>(p_data).load();
            auto& data = std::get<data_t>(p_data);
            if (this->majority() == cdf_majority::column)
            {
                majority::swap(data, p_shape);
            }
            check_shape();
        }
    }


    template <typename... Ts>
    friend auto visit(Variable& var, Ts... lambdas);

    template <class stream_t>
    inline stream_t& __repr__(stream_t& os, indent_t indent = {}, bool detailed = true) const
    {
        if (detailed)
        {
            os << indent << name() << ":\n" << indent + 2 << "shape: ";
            stream_collection(os, shape(), ", ");
            os << "\n"
               << indent + 2 << "type: " << cdf_type_str(type()) << "\n"
               << indent + 2 << "record vary: " << (is_nrv() ? "False" : "True") << "\n"
               << indent + 2 << compression_type() << "\n\n";
            os << indent + 2 << "Attributes:\n";
            std::for_each(std::cbegin(attributes), std::cend(attributes),
                [&os, indent](const auto& item) { item.second.__repr__(os, indent + 4); });
        }
        else
        {
            os << indent << name() << ": ";
            stream_collection(os, shape(), ", ");
            os << ", [" << cdf_type_str(type())
               << "], record vary:" << (is_nrv() ? "False" : "True")
               << ", compression: " << cdf_compression_type_str(compression_type()) << std::endl;
        }
        return os;
    }


private:
    [[nodiscard]] var_data_t& _data()
    {
        load_values();
        return std::get<var_data_t>(p_data);
    }

    [[nodiscard]] const var_data_t& _data() const
    {
        load_values();
        return std::get<var_data_t>(p_data);
    }

    void check_shape() const
    {

        if (flat_size(p_shape) != _data().size()
            and not(is_nrv() and _data().size() == 0UL
                and (_data().type() == CDF_Types::CDF_CHAR
                    or _data().type() == CDF_Types::CDF_UCHAR)))
            throw std::invalid_argument { "Variable: given shape and data size doens't match" };
    }

    std::string p_name;
    std::size_t p_number;
    mutable std::variant<lazy_data, var_data_t> p_data;
    shape_t p_shape;
    cdf_majority p_majority;
    bool p_is_nrv;
    cdf_compression_type p_compression;
};

template <typename... Ts>
auto visit(Variable& var, Ts... lambdas)
{
    return visit(var.p_data, lambdas...);
}

} // namespace cdf

template <class stream_t>
inline stream_t& operator<<(stream_t& os, const cdf::Variable& variable)
{
    return variable.template __repr__<stream_t>(os, indent_t {});
}
