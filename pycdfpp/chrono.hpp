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

#include "collections.hpp"
#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
#include <cdfpp/no_init_vector.hpp>

using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <datetime.h>

namespace py = pybind11;

namespace _details
{

[[nodiscard]] inline bool is_dt64ns(const py::array& arr)
{
    if (arr.dtype().kind() != 'M')
        return false;
    return std::string(arr.dtype().attr("str").cast<std::string>()).find("[ns]");
}

template <typename T>
concept time_point_type = requires(T a) { a.time_since_epoch(); };


[[nodiscard]] inline PyObject* to_datetime(const time_point_type auto& timepoint)
{
    auto dp = floor<days>(timepoint);
    year_month_day ymd { dp };
    hh_mm_ss time { floor<microseconds>(timepoint - dp) };
    return PyDateTime_FromDateAndTime(static_cast<int>(ymd.year()),
        static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()),
        static_cast<unsigned>(time.hours().count()), static_cast<unsigned>(time.minutes().count()),
        static_cast<unsigned>(time.seconds().count()),
        static_cast<unsigned>(time.subseconds().count())); // microseconds
}


[[nodiscard]] inline auto to_tp(int64_t ns)
{
    return std::chrono::system_clock::time_point {} + std::chrono::nanoseconds(ns);
}

[[nodiscard]] inline auto to_tp(const PyDateTime_DateTime* dt)
{
    int year = PyDateTime_GET_YEAR(dt);
    int month = PyDateTime_GET_MONTH(dt);
    int day = PyDateTime_GET_DAY(dt);
    int hour = PyDateTime_DATE_GET_HOUR(dt);
    int minute = PyDateTime_DATE_GET_MINUTE(dt);
    int second = PyDateTime_DATE_GET_SECOND(dt);
    int micro = PyDateTime_DATE_GET_MICROSECOND(dt);

    std::tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    t.tm_isdst = -1; // Unknown DST

    // Warning: mktime uses local time. If your Python datetime is UTC,
    // use timegm (Linux) or _mkgmtime (Windows).
#ifdef _WIN32
    std::time_t tt = _mkgmtime(&t);
#else
    std::time_t tt = timegm(&t);
#endif

    auto duration = std::chrono::seconds(tt) + std::chrono::microseconds(micro);
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(duration));
}

template <cdf_time_t time_t>
[[nodiscard]] inline time_t from_datetime(const PyObject* dt)
{
    return cdf::to_cdf_time<time_t>(to_tp(reinterpret_cast<const PyDateTime_DateTime* const>(dt)));
}

[[nodiscard]] inline PyObject* to_datetime(int64_t ns)
{
    return to_datetime(to_tp(ns));
}

[[nodiscard]] inline PyObject* to_datetime(const cdf_time_t auto time)
{
    return to_datetime(cdf::to_time_point(time));
}

template <typename T>
[[nodiscard]] inline py::list to_datetime(const std::span<T> input)
{
    using namespace _details::ranges;
    py::list result;
    auto out = py_stealing_raw_sink { _details::underlying_pyobject(result) };
    for (auto v : input)
    {
        *out = to_datetime(v);
    }
    return result;
}

} // namespace _details

template <typename time_t>
inline void to_datetime64(const std::span<time_t>& input, int64_t* const output)
{
    py::gil_scoped_release release;
    cdf::to_ns_from_1970(input, output);
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
[[nodiscard]] inline py::object to_datetime64(const py::array_t<time_t>& input)
{
    constexpr auto dtype = time_t_to_dtype<time_t>();
    if (input.ndim() > 0)
    {
        auto result = _details::ranges::transform<py::array, time_t, int64_t>(input,
            static_cast<void (*)(const std::span<time_t>&, int64_t* const)>(to_datetime64<time_t>));
        return result.attr("view")(dtype);
    }
    return py::none();
}

[[nodiscard]] inline py::object to_datetime64(const cdf_time_t auto& input)
{
    using time_t = std::decay_t<decltype(input)>;
    constexpr auto dtype = time_t_to_dtype<time_t>();
    int64_t v = cdf::to_time_point(input).time_since_epoch().count();
    return py::array(py::dtype(dtype), {}, {}, &v);
}

[[nodiscard]] inline int64_t to_datetime64(const PyDateTime_DateTime* dt)
{
    using namespace std::chrono;

    int y = PyDateTime_GET_YEAR(dt);
    int m = PyDateTime_GET_MONTH(dt);
    int d = PyDateTime_GET_DAY(dt);
    int hh = PyDateTime_DATE_GET_HOUR(dt);
    int mm = PyDateTime_DATE_GET_MINUTE(dt);
    int ss = PyDateTime_DATE_GET_SECOND(dt);
    int us = PyDateTime_DATE_GET_MICROSECOND(dt);

    auto date = year_month_day { year { y }, month { static_cast<unsigned>(m) },
        day { static_cast<unsigned>(d) } };
    sys_days tp_days = date;

    auto tp = tp_days + hours { hh } + minutes { mm } + seconds { ss } + microseconds { us };

    return duration_cast<nanoseconds>(tp.time_since_epoch()).count();
}

[[nodiscard]] inline int64_t to_datetime64(const PyObject* o)
{
    return to_datetime64(reinterpret_cast<const PyDateTime_DateTime*>(o));
}

[[nodiscard]] inline py::object to_datetime64(const py_list_or_py_tuple auto& input)
{
    /*
     * The code here is directly using Python C-API to avoid the overhead of pybind11 in order
     * to reduce as much as possible the overhead of iterating a list/tuple of random PyObject*.
     * We avoid using py::cast or py::object for the items since they would add reference counting.
     */
    static auto* tt2000_type_ptr = reinterpret_cast<PyTypeObject*>(py::type::of<tt2000_t>().ptr());
    static auto* epoch_type_ptr = reinterpret_cast<PyTypeObject*>(py::type::of<epoch>().ptr());
    static auto* epoch16_type_ptr = reinterpret_cast<PyTypeObject*>(py::type::of<epoch16>().ptr());

    return _details::ranges::transform<py::array_t<int64_t>>(input,
        [](const PyObject* obj) -> int64_t
        {
            if (PyDateTime_Check(obj))
            {
                return to_datetime64(obj);
            }
            else
            {
                auto item_type = Py_TYPE(obj);
                if (item_type == tt2000_type_ptr)
                {
                    return cdf::to_time_point(*_details::cast<tt2000_t>(obj))
                        .time_since_epoch()
                        .count();
                }
                else if (item_type == epoch_type_ptr)
                {
                    return cdf::to_time_point(*_details::cast<epoch>(obj))
                        .time_since_epoch()
                        .count();
                }
                else if (item_type == epoch16_type_ptr)
                {
                    return cdf::to_time_point(*_details::cast<epoch16>(obj))
                        .time_since_epoch()
                        .count();
                }
                else
                {
                    throw std::out_of_range(
                        "Only supports datetime.datetime, tt2000_t, epoch and epoch16 types");
                }
            }
        })
        .attr("view")("datetime64[ns]");
}

[[nodiscard]] inline py::object to_datetime(const cdf_time_t auto& input)
{
    return py::reinterpret_steal<py::object>(_details::to_datetime(cdf::to_time_point(input)));
}


[[nodiscard]] inline py::list to_datetime(const py_list_or_py_tuple auto& input)
{
    static auto* tt2000_type_ptr = reinterpret_cast<PyTypeObject*>(py::type::of<tt2000_t>().ptr());
    static auto* epoch_type_ptr = reinterpret_cast<PyTypeObject*>(py::type::of<epoch>().ptr());
    static auto* epoch16_type_ptr = reinterpret_cast<PyTypeObject*>(py::type::of<epoch16>().ptr());

    return _details::ranges::transform<py::list>(input,
        [](const PyObject* const item) -> PyObject*
        {
            if (PyDateTime_Check(item))
            {
                return PyDateTime_FromDateAndTime(PyDateTime_GET_YEAR(item),
                    PyDateTime_GET_MONTH(item), PyDateTime_GET_DAY(item),
                    PyDateTime_DATE_GET_HOUR(item), PyDateTime_DATE_GET_MINUTE(item),
                    PyDateTime_DATE_GET_SECOND(item), PyDateTime_DATE_GET_MICROSECOND(item));
            }
            else
            {
                auto item_type = Py_TYPE(item);
                if (item_type == tt2000_type_ptr)
                {
                    return _details::to_datetime(to_time_point(*_details::cast<tt2000_t>(item)));
                }
                else if (item_type == epoch_type_ptr)
                {
                    return _details::to_datetime(to_time_point(*_details::cast<epoch>(item)));
                }
                else if (item_type == epoch16_type_ptr)
                {
                    return _details::to_datetime(to_time_point(*_details::cast<epoch16>(item)));
                }
            }
            return nullptr;
        });
}

template <typename time_t>
[[nodiscard]] inline py::list to_datetime(const py::array_t<time_t>& input)
{
    return _details::ranges::transform<py::list, time_t>(
        input, static_cast<PyObject* (*)(const time_t)>(_details::to_datetime<time_t>));
}

[[nodiscard]] inline py::list to_datetime(const py::array& input)
{
    if (!_details::is_dt64ns(input))
    {
        throw std::out_of_range("Only supports datetime64[ns] arrays");
    }
    return _details::ranges::transform<py::list, int64_t>(
        input, static_cast<PyObject* (*)(int64_t)>(_details::to_datetime));
}

[[nodiscard]] inline py::object to_datetime64(const Variable& input)
{
    auto result = _details::fast_allocate_array<uint64_t>(input.shape());
    auto out_ptr = static_cast<int64_t*>(result.request(true).ptr);
    const auto size = static_cast<std::size_t>(result.size());
    switch (input.type())
    {
        case cdf::CDF_Types::CDF_EPOCH:
        {
            to_datetime64(
                std::span { input.get<cdf::CDF_Types::CDF_EPOCH>().data(), size }, out_ptr);
        }
        break;
        case cdf::CDF_Types::CDF_EPOCH16:
        {
            to_datetime64(
                std::span { input.get<cdf::CDF_Types::CDF_EPOCH16>().data(), size }, out_ptr);
        }
        break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
        {
            to_datetime64(
                std::span { input.get<cdf::CDF_Types::CDF_TIME_TT2000>().data(), size }, out_ptr);
        }
        break;
        default:
            throw std::out_of_range("Only supports cdf time types");
            break;
    }
    return result.attr("view")("datetime64[ns]");
}


[[nodiscard]] py::list to_datetime(const Variable& input)
{
    switch (input.type())
    {
        case cdf::CDF_Types::CDF_EPOCH:
        {
            return _details::ranges::transform<py::list, epoch>(
                input, static_cast<PyObject* (*)(const epoch)>(_details::to_datetime<epoch>));
        }
        break;
        case cdf::CDF_Types::CDF_EPOCH16:
        {
            return _details::ranges::transform<py::list, epoch16>(
                input, static_cast<PyObject* (*)(const epoch16)>(_details::to_datetime<epoch16>));
        }
        break;
        case cdf::CDF_Types::CDF_TIME_TT2000:
        {
            return _details::ranges::transform<py::list, tt2000_t>(
                input, static_cast<PyObject* (*)(const tt2000_t)>(_details::to_datetime<tt2000_t>));
        }
        break;
        default:
            throw std::out_of_range("Only supports cdf time types");
            break;
    }
    return py::list {};
}

template <typename time_t>
[[nodiscard]] inline time_t to_cdf_time_t(const PyObject* dt)
{
    return cdf::to_cdf_time<time_t>(_details::to_tp(reinterpret_cast<const PyDateTime_DateTime*>(dt)));
}

[[nodiscard]] inline py::list to_tt2000(const py_list_or_py_tuple auto& input)
{
    static auto* tt2000_type_ptr = reinterpret_cast<PyTypeObject*>(py::type::of<tt2000_t>().ptr());
    static auto* epoch_type_ptr = reinterpret_cast<PyTypeObject*>(py::type::of<epoch>().ptr());
    static auto* epoch16_type_ptr = reinterpret_cast<PyTypeObject*>(py::type::of<epoch16>().ptr());

    return _details::ranges::transform<py::list>(input,
        [](const PyObject* const item) -> PyObject*
        {
            if (PyDateTime_Check(item))
            {
                return py::cast(_details::from_datetime<tt2000_t>(item)).release().ptr();
            }
            else
            {
                auto item_type = Py_TYPE(item);
                if (item_type == tt2000_type_ptr)
                {
                    return py::cast(*_details::cast<tt2000_t>(item)).release().ptr();
                }
                else if (item_type == epoch_type_ptr)
                {
                    return py::cast(
                        to_cdf_time<tt2000_t>(to_time_point(*_details::cast<epoch>(item))))
                        .release()
                        .ptr();
                }
                else if (item_type == epoch16_type_ptr)
                {
                    return py::cast(
                        to_cdf_time<tt2000_t>(to_time_point(*_details::cast<epoch16>(item))))
                        .release()
                        .ptr();
                }
            }
            return nullptr;
        });
}


[[nodiscard]] inline auto to_epoch(const time_point_collection_t auto& tps)
{
    auto result = cdf::to_epoch(tps);
    return py::array_t<epoch>(std::size(result), result.data());
}

[[nodiscard]] inline auto to_epoch16(
    const no_init_vector<decltype(std::chrono::system_clock::now())>& tps)
{
    auto result = cdf::to_epoch16(tps);
    return py::array_t<epoch16>(std::size(result), result.data());
}


[[nodiscard]] inline bool _to_cdf_time_t(
    const cdf_time_t_span_t auto& input, cdf_time_t auto* output)
{
    for (const auto in: input)
    {
        *output= cdf::to_cdf_time<std::decay_t<decltype(output[0])>>(in);
        ++output;
    }
    return true;
}

template <typename time_t>
[[nodiscard]] inline bool to_cdf_time_t(const py::array& input, time_t* const output)
{
    const static auto tt2000_dtype = py::dtype::of<tt2000_t>();
    const static auto epoch_dtype = py::dtype::of<epoch>();
    const static auto epoch16_dtype = py::dtype::of<epoch16>();
    if (_details::is_dt64ns(input) || input.dtype().is(py::dtype::of<int64_t>()) || input.dtype().is(py::dtype::of<uint64_t>()))
    {
        auto view = _details::ranges::make_span<const int64_t>(py::array(input.attr("view")("int64")));
        py::gil_scoped_release release;
        cdf::from_ns_from_1970(view, output);
        return true;
    }
    if (input.dtype().is(tt2000_dtype))
    {
        auto view =  _details::ranges::make_span<const tt2000_t>(input);
        py::gil_scoped_release release;
        return _to_cdf_time_t(view, output);
    }
    if (input.dtype().is(epoch_dtype))
    {
        auto view =  _details::ranges::make_span<const epoch>(input);
        py::gil_scoped_release release;
        return _to_cdf_time_t(view, output);
    }
    if (input.dtype().is(epoch16_dtype))
    {
        auto view =  _details::ranges::make_span<const epoch16>(input);
        py::gil_scoped_release release;
        return _to_cdf_time_t(view, output);
    }
    return false;
}

template <typename time_t>
inline py::object to_cdf_time_t(const py::array& input)
{

    auto res = _details::fast_allocate_array<time_t>(input);
    time_t* out = reinterpret_cast<time_t*>(res.request().ptr);
    if (to_cdf_time_t<time_t>(input, out))
    {
        return res;
    }
    return py::none();
}


void def_time_types_wrapper(auto& mod)
{
    py::class_<tt2000_t>(mod, "tt2000_t")
        .def(py::init<int64_t>())
        .def_readwrite("nseconds", &tt2000_t::nseconds)
        .def(py::self == py::self)
        .def("__repr__", __repr__<tt2000_t>);
    py::class_<epoch>(mod, "epoch")
        .def(py::init<double>())
        .def_readwrite("mseconds", &epoch::mseconds)
        .def(py::self == py::self)
        .def("__repr__", __repr__<epoch>);
    py::class_<epoch16>(mod, "epoch16")
        .def(py::init<double, double>())
        .def(py::self == py::self)
        .def_readwrite("seconds", &epoch16::seconds)
        .def_readwrite("picoseconds", &epoch16::picoseconds)
        .def("__repr__", __repr__<epoch16>);

    PYBIND11_NUMPY_DTYPE(tt2000_t, nseconds);
    PYBIND11_NUMPY_DTYPE(epoch, mseconds);
    PYBIND11_NUMPY_DTYPE(epoch16, seconds, picoseconds);
}

auto def_to_dt64_conversion_functions(auto& mod)
{
    mod.def("to_datetime64",
        static_cast<py::object (*)(const py::array_t<epoch>&)>(to_datetime64<epoch>),
        py::arg { "values" }.noconvert());
    mod.def("to_datetime64",
        static_cast<py::object (*)(const py::array_t<epoch16>&)>(to_datetime64<epoch16>),
        py::arg { "values" }.noconvert());
    mod.def("to_datetime64",
        static_cast<py::object (*)(const py::array_t<tt2000_t>&)>(to_datetime64<tt2000_t>),
        py::arg { "values" }.noconvert());

    mod.def("to_datetime64", static_cast<py::object (*)(const epoch&)>(to_datetime64<epoch>),
        py::arg { "value" });
    mod.def("to_datetime64", static_cast<py::object (*)(const epoch16&)>(to_datetime64<epoch16>),
        py::arg { "value" });
    mod.def("to_datetime64", static_cast<py::object (*)(const tt2000_t&)>(to_datetime64<tt2000_t>),
        py::arg { "value" });

    mod.def("to_datetime64", static_cast<py::object (*)(const py::list&)>(to_datetime64),
        py::arg { "values" }.noconvert());
    mod.def("to_datetime64", static_cast<py::object (*)(const py::tuple&)>(to_datetime64),
        py::arg { "values" }.noconvert());

    mod.def("to_datetime64", static_cast<py::object (*)(const Variable&)>(to_datetime64),
        py::arg { "variable" });
}

auto def_to_datetime_conversion_functions(auto& mod)
{

    mod.def("to_datetime", static_cast<py::list (*)(const py::array_t<epoch>&)>(to_datetime<epoch>),
        py::arg { "values" }.noconvert());
    mod.def("to_datetime",
        static_cast<py::list (*)(const py::array_t<epoch16>&)>(to_datetime<epoch16>),
        py::arg { "values" }.noconvert());
    mod.def("to_datetime",
        static_cast<py::list (*)(const py::array_t<tt2000_t>&)>(to_datetime<tt2000_t>),
        py::arg { "values" }.noconvert());

    mod.def("to_datetime", static_cast<py::list (*)(const py::tuple&)>(to_datetime),
        py::arg { "values" }.noconvert());
    mod.def("to_datetime", static_cast<py::list (*)(const py::list&)>(to_datetime),
        py::arg { "values" }.noconvert());


    mod.def("to_datetime", static_cast<py::list (*)(const py::array&)>(to_datetime));

    mod.def("to_datetime", static_cast<py::object (*)(const epoch&)>(to_datetime<epoch>),
        py::arg { "value" });
    mod.def("to_datetime", static_cast<py::object (*)(const epoch16&)>(to_datetime<epoch16>),
        py::arg { "value" });
    mod.def("to_datetime", static_cast<py::object (*)(const tt2000_t&)>(to_datetime<tt2000_t>),
        py::arg { "value" });

    mod.def("to_datetime", static_cast<py::list (*)(const Variable&)>(to_datetime));
}

auto def_time_conversion_functions(auto& mod)
{
    PyDateTime_IMPORT;
    // forward
    {
        def_to_dt64_conversion_functions(mod);
        def_to_datetime_conversion_functions(mod);
    }

    // backward
    {
        mod.def("to_tt2000", static_cast<py::list (*)(const py::tuple&)>(to_tt2000),
            py::arg { "values" }.noconvert());
        mod.def("to_tt2000", static_cast<py::list (*)(const py::list&)>(to_tt2000),
            py::arg { "values" }.noconvert());

        mod.def("to_tt2000",
            [](decltype(std::chrono::system_clock::now()) tp) { return cdf::to_tt2000(tp); });

        mod.def("to_tt2000",
            [](const py::array& input) -> py::object { return to_cdf_time_t<tt2000_t>(input); });

        mod.def("to_epoch",
            [](decltype(std::chrono::system_clock::now()) tp) { return cdf::to_epoch(tp); });
        mod.def("to_epoch",
            [](const no_init_vector<decltype(std::chrono::system_clock::now())>& tps)
            { return to_epoch(tps); });
        mod.def("to_epoch",
            [](const py::array& input) -> py::object { return to_cdf_time_t<epoch>(input); });

        mod.def("to_epoch16",
            [](decltype(std::chrono::system_clock::now()) tp) { return cdf::to_epoch16(tp); });
        mod.def("to_epoch16",
            [](const no_init_vector<decltype(std::chrono::system_clock::now())>& tps)
            { return to_epoch16(tps); });
        mod.def("to_epoch16",
            [](const py::array& input) -> py::object { return to_cdf_time_t<epoch16>(input); });
    }
}
