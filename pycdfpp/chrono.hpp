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

#include "repr.hpp"

#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename time_t, typename function_t>
[[nodiscard]] inline auto transform(time_t* input, std::size_t count, const function_t& f)
{
    auto result = py::array_t<uint64_t>(count);
    py::buffer_info res_buff = result.request(true);
    int64_t* res_ptr = static_cast<int64_t*>(res_buff.ptr);
    std::transform(input, input + count, res_ptr, f);
    return result;
}

template <typename time_t, typename T, typename function_t>
[[nodiscard]] inline auto transform(const py::array_t<T>& input, const function_t& f)
{
    py::buffer_info in_buff = input.request();
    time_t* in_ptr = static_cast<time_t*>(in_buff.ptr);
    return transform(in_ptr, in_buff.shape[0], f);
}

template <typename time_t, typename T, typename function_t>
[[nodiscard]] inline auto transform(const std::vector<T>& input, const function_t& f)
{
    auto result = py::array_t<uint64_t>(std::size(input));
    py::buffer_info res_buff = result.request(true);
    int64_t* res_ptr = static_cast<int64_t*>(res_buff.ptr);
    std::transform(std::cbegin(input), std::cend(input), res_ptr, f);
    return result;
}

template <typename time_t>
[[nodiscard]] constexpr auto time_t_to_dtype()
{
    using period = typename decltype(cdf::to_time_point(std::declval<time_t>()))::duration::period;
    if constexpr (std::is_same_v<period, std::pico>)
        return "datetime64[ps]";
    if constexpr (std::is_same_v<period, std::nano>)
        return "datetime64[ns]";
    if constexpr (std::is_same_v<period, std::micro>)
        return "datetime64[us]";
    if constexpr (std::is_same_v<period, std::milli>)
        return "datetime64[ms]";
    if constexpr (std::is_same_v<period, std::ratio<1>>)
        return "datetime64[s]";
}

template <typename time_t>
[[nodiscard]] inline py::object array_to_datetime64(const py::array_t<time_t>& input)
{
    constexpr auto dtype = time_t_to_dtype<time_t>();
    if (input.ndim() > 0)
    {
        auto result = transform<time_t>(input,
            [](const time_t& v) { return cdf::to_time_point(v).time_since_epoch().count(); });
        return py::cast(&result).attr("astype")(dtype);
    }
    return py::none();
}

template <typename time_t>
[[nodiscard]] inline py::object scalar_to_datetime64(const time_t& input)
{
    constexpr auto dtype = time_t_to_dtype<time_t>();
    auto v = new int64_t;
    *v = cdf::to_time_point(input).time_since_epoch().count();
    return py::array(py::dtype(dtype), {}, {}, v);
}

template <typename time_t>
[[nodiscard]] inline py::object vector_to_datetime64(const std::vector<time_t>& input)
{
    constexpr auto dtype = time_t_to_dtype<time_t>();

    auto result = transform<time_t>(
        input, [](const time_t& v) { return cdf::to_time_point(v).time_since_epoch().count(); });
    return py::cast(&result).attr("astype")(dtype);
}

template <typename time_t>
[[nodiscard]] inline std::vector<decltype(to_time_point(time_t {}))> vector_to_datetime(
    const std::vector<time_t>& input)
{
    std::vector<decltype(to_time_point(time_t {}))> result(std::size(input));
    std::transform(std::cbegin(input), std::cend(input), std::begin(result),
        static_cast<decltype(to_time_point(time_t {})) (*)(const time_t&)>(to_time_point));
    return result;
}

[[nodiscard]] inline py::object var_to_datetime64(const Variable& input)
{
    switch (input.type())
    {
        case cdf::CDF_Types::CDF_EPOCH:
        {
            auto result = transform(input.get<cdf::CDF_Types::CDF_EPOCH>().data(), input.shape()[0],
                [](const epoch& v) { return cdf::to_time_point(v).time_since_epoch().count(); });
            return py::cast(&result).attr("astype")("datetime64[ns]");
        }
        break;
        case cdf::CDF_Types::CDF_EPOCH16:
        {
            auto result = transform(input.get<cdf::CDF_Types::CDF_EPOCH16>().data(),
                input.shape()[0],
                [](const epoch16& v) { return cdf::to_time_point(v).time_since_epoch().count(); });
            return py::cast(&result).attr("astype")("datetime64[ns]");
        }
        break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
        {
            auto result = transform(input.get<cdf::CDF_Types::CDF_TIME_TT2000>().data(),
                input.shape()[0],
                [](const tt2000_t& v) { return cdf::to_time_point(v).time_since_epoch().count(); });
            return py::cast(&result).attr("astype")("datetime64[ns]");
        }
        break;
        default:
            throw std::out_of_range("Only supports cdf time types");
            break;
    }
    return {};
}


[[nodiscard]] inline std::vector<decltype(to_time_point(epoch {}))> var_to_datetime(
    const Variable& input)
{
    switch (input.type())
    {
        case cdf::CDF_Types::CDF_EPOCH:
        {
            using namespace std::chrono;
            std::vector<decltype(to_time_point(epoch {}))> result(input.len());
            std::transform(std::cbegin(input.get<epoch>()), std::cend(input.get<epoch>()),
                std::begin(result),
                static_cast<decltype(to_time_point(epoch {})) (*)(const epoch&)>(to_time_point));
            return result;
        }
        break;
        case cdf::CDF_Types::CDF_EPOCH16:
        {
            std::vector<decltype(to_time_point(epoch16 {}))> result(input.len());
            std::transform(std::cbegin(input.get<epoch16>()), std::cend(input.get<epoch16>()),
                std::begin(result),
                static_cast<decltype(to_time_point(epoch16 {})) (*)(const epoch16&)>(
                    to_time_point));
            return result;
        }
        break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
        {
            std::vector<decltype(to_time_point(tt2000_t {}))> result(input.len());
            std::transform(std::cbegin(input.get<tt2000_t>()), std::cend(input.get<tt2000_t>()),
                std::begin(result),
                static_cast<decltype(to_time_point(tt2000_t {})) (*)(const tt2000_t&)>(
                    to_time_point));
            return result;
        }
        break;
        default:
            throw std::out_of_range("Only supports cdf time types");
            break;
    }
    return {};
}

template <typename T>
void def_time_types_wrapper(T& mod)
{
    py::class_<tt2000_t>(mod, "tt2000_t")
        .def(py::init<int64_t>())
        .def_readwrite("value", &tt2000_t::value)
        .def(py::self == py::self)
        .def("__repr__", __repr__<tt2000_t>);
    py::class_<epoch>(mod, "epoch")
        .def(py::init<double>())
        .def_readwrite("value", &epoch::value)
        .def(py::self == py::self)
        .def("__repr__", __repr__<epoch>);
    py::class_<epoch16>(mod, "epoch16")
        .def(py::init<double, double>())
        .def(py::self == py::self)
        .def_readwrite("seconds", &epoch16::seconds)
        .def_readwrite("picoseconds", &epoch16::picoseconds)
        .def("__repr__", __repr__<epoch16>);

    PYBIND11_NUMPY_DTYPE(tt2000_t, value);
    PYBIND11_NUMPY_DTYPE(epoch, value);
    PYBIND11_NUMPY_DTYPE(epoch16, seconds, picoseconds);
}

template <typename T>
auto def_time_conversion_functions(T& mod)
{
    // forward
    {
        mod.def("to_datetime64", array_to_datetime64<epoch>, py::arg { "values" }.noconvert());
        mod.def("to_datetime64", array_to_datetime64<epoch16>, py::arg { "values" }.noconvert());
        mod.def("to_datetime64", array_to_datetime64<tt2000_t>, py::arg { "values" }.noconvert());

        mod.def("to_datetime64", scalar_to_datetime64<epoch>, py::arg { "value" });
        mod.def("to_datetime64", scalar_to_datetime64<epoch16>, py::arg { "value" });
        mod.def("to_datetime64", scalar_to_datetime64<tt2000_t>, py::arg { "value" });

        mod.def("to_datetime64", vector_to_datetime64<epoch>, py::arg { "values" }.noconvert());
        mod.def("to_datetime64", vector_to_datetime64<epoch16>, py::arg { "values" }.noconvert());
        mod.def("to_datetime64", vector_to_datetime64<tt2000_t>, py::arg { "values" }.noconvert());

        mod.def("to_datetime64", var_to_datetime64, py::arg { "variable" });

        mod.def("to_datetime", vector_to_datetime<epoch>);
        mod.def("to_datetime", vector_to_datetime<epoch16>);
        mod.def("to_datetime", vector_to_datetime<tt2000_t>);
        mod.def("to_datetime",
            static_cast<decltype(to_time_point(epoch {})) (*)(const epoch&)>(to_time_point));
        mod.def("to_datetime",
            static_cast<decltype(to_time_point(epoch16 {})) (*)(const epoch16&)>(to_time_point));
        mod.def("to_datetime",
            static_cast<decltype(to_time_point(tt2000_t {})) (*)(const tt2000_t&)>(to_time_point));

        mod.def("to_datetime", var_to_datetime);
    }

    // backward
    {
        mod.def("to_tt2000",
            [](decltype(std::chrono::system_clock::now()) tp) { return cdf::to_tt2000(tp); });
        mod.def("to_tt2000",
            [](const no_init_vector<decltype(std::chrono::system_clock::now())>& tps)
            { return cdf::to_tt2000(tps); });


        mod.def("to_epoch",
            [](decltype(std::chrono::system_clock::now()) tp) { return cdf::to_epoch(tp); });
        mod.def("to_epoch",
            [](const no_init_vector<decltype(std::chrono::system_clock::now())>& tps)
            { return cdf::to_epoch(tps); });

        mod.def("to_epoch16",
            [](decltype(std::chrono::system_clock::now()) tp) { return cdf::to_epoch16(tp); });
        mod.def("to_epoch16",
            [](const no_init_vector<decltype(std::chrono::system_clock::now())>& tps)
            { return cdf::to_epoch16(tps); });
    }
}
