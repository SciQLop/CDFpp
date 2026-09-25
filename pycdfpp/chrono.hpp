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
#include "data_types.hpp"

#include <cdfpp/cdf-data.hpp>
#include <cdfpp/cdf.hpp>
#include <cdfpp/chrono/cdf-chrono.hpp>
#include <cpp_utils/containers/no_init_vector.hpp>
using cpp_utils::containers::no_init_vector;

using namespace cdf;

#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <datetime.h>

#include <fmt/core.h>

namespace py = pybind11;

// A datetime.datetime argument, converted with _details::to_tp.
struct py_datetime
{
    const PyObject* obj;
};

namespace pybind11::detail
{
template <>
struct type_caster<py_datetime>
{
    PYBIND11_TYPE_CASTER(py_datetime, const_name("datetime.datetime"));

    bool load(handle src, bool)
    {
        if (!PyDateTime_Check(src.ptr()))
            return false;
        value.obj = src.ptr();
        return true;
    }

    static handle cast(const py_datetime& dt, return_value_policy, handle)
    {
        return py::handle(const_cast<PyObject*>(dt.obj)).inc_ref();
    }
};
}

namespace _details
{


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

[[nodiscard]] inline std::chrono::microseconds utc_offset(const PyDateTime_DateTime* dt)
{
    auto offset = py::reinterpret_steal<py::object>(
        PyObject_CallMethod(reinterpret_cast<PyObject*>(const_cast<PyDateTime_DateTime*>(dt)),
            "utcoffset", nullptr));
    if (!offset)
        throw py::error_already_set();
    if (offset.is_none())
        return {};
    return std::chrono::days { PyDateTime_DELTA_GET_DAYS(offset.ptr()) }
    + std::chrono::seconds { PyDateTime_DELTA_GET_SECONDS(offset.ptr()) }
    + std::chrono::microseconds { PyDateTime_DELTA_GET_MICROSECONDS(offset.ptr()) };
}

// Naive datetimes are UTC, whatever the machine's timezone; aware ones are shifted to UTC.
// pybind11's own chrono caster is not used: it reads naive datetimes as local time.
[[nodiscard]] inline auto to_tp(const PyDateTime_DateTime* dt)
{
    using namespace std::chrono;
    const sys_days date = year_month_day { year { PyDateTime_GET_YEAR(dt) },
        month { static_cast<unsigned>(PyDateTime_GET_MONTH(dt)) },
        day { static_cast<unsigned>(PyDateTime_GET_DAY(dt)) } };
    const auto wall_clock = date + hours { PyDateTime_DATE_GET_HOUR(dt) }
        + minutes { PyDateTime_DATE_GET_MINUTE(dt) } + seconds { PyDateTime_DATE_GET_SECOND(dt) }
        + microseconds { PyDateTime_DATE_GET_MICROSECOND(dt) };
    const auto utc = dt->hastzinfo ? wall_clock - utc_offset(dt) : wall_clock;
    return system_clock::time_point { duration_cast<system_clock::duration>(
        utc.time_since_epoch()) };
}

[[nodiscard]] inline auto to_tp(const PyObject* dt)
{
    return to_tp(reinterpret_cast<const PyDateTime_DateTime*>(dt));
}

// A naive datetime is already UTC: copying its fields is much faster than a round trip.
[[nodiscard]] inline PyObject* to_utc_datetime(const PyObject* item)
{
    const auto* dt = reinterpret_cast<const PyDateTime_DateTime*>(item);
    if (dt->hastzinfo)
        return to_datetime(to_tp(dt));
    return PyDateTime_FromDateAndTime(PyDateTime_GET_YEAR(dt), PyDateTime_GET_MONTH(dt),
        PyDateTime_GET_DAY(dt), PyDateTime_DATE_GET_HOUR(dt), PyDateTime_DATE_GET_MINUTE(dt),
        PyDateTime_DATE_GET_SECOND(dt), PyDateTime_DATE_GET_MICROSECOND(dt));
}

template <cdf_time_t time_t>
[[nodiscard]] inline time_t from_datetime(const PyObject* dt)
{
    return cdf::to_cdf_time<time_t>(to_tp(dt));
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
    return py::array_t<int64_t>().attr("view")(dtype);
}

[[nodiscard]] inline py::object to_datetime64(const cdf_time_t auto& input)
{
    using time_t = std::decay_t<decltype(input)>;
    constexpr auto dtype = time_t_to_dtype<time_t>();
    int64_t v = cdf::to_time_point(input).time_since_epoch().count();
    return py::array(py::dtype(dtype), {}, {}, &v);
}

[[nodiscard]] inline int64_t to_datetime64(const PyObject* dt)
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(_details::to_tp(dt).time_since_epoch()).count();
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
                    throw std::invalid_argument(
                        fmt::format("to_datetime64 only supports datetime.datetime, tt2000_t, "
                                    "epoch and epoch16 types, got {}",
                            Py_TYPE(const_cast<PyObject*>(obj))->tp_name));
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
                return _details::to_utc_datetime(item);
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
    if (!is_dt64ns(input))
    {
        throw std::invalid_argument(
            fmt::format("to_datetime requires a datetime64[ns] array, got dtype '{}'",
                std::string(input.dtype().attr("str").cast<std::string>())));
    }
    return _details::ranges::transform<py::list, int64_t>(
        input, static_cast<PyObject* (*)(int64_t)>(_details::to_datetime));
}

[[nodiscard]] inline py::object to_datetime64(const Variable& input)
{
    using enum cdf::CDF_Types;
    auto result = _details::fast_allocate_array<uint64_t>(input.shape());
    auto out_ptr = static_cast<int64_t*>(result.request(true).ptr);
    const auto size = static_cast<std::size_t>(result.size());
    switch (input.type())
    {
        case CDF_EPOCH:
            to_datetime64(
                std::span { input.get<cdf::CDF_Types::CDF_EPOCH>().data(), size }, out_ptr);
            break;
        case CDF_EPOCH16:
            to_datetime64(
                std::span { input.get<cdf::CDF_Types::CDF_EPOCH16>().data(), size }, out_ptr);
            break;
        case CDF_TIME_TT2000:
            to_datetime64(
                std::span { input.get<cdf::CDF_Types::CDF_TIME_TT2000>().data(), size }, out_ptr);
            break;
        default:
            throw std::invalid_argument(
                fmt::format("to_datetime64 requires a CDF time variable (CDF_EPOCH, CDF_EPOCH16, "
                            "or CDF_TIME_TT2000), got {}",
                    cdf_type_str(input.type())));
    }
    return result.attr("view")("datetime64[ns]");
}


[[nodiscard]] py::list to_datetime(const Variable& input)
{
    using enum cdf::CDF_Types;
    switch (input.type())
    {
        case CDF_EPOCH:
            return _details::ranges::transform<py::list, epoch>(
                input, static_cast<PyObject* (*)(const epoch)>(_details::to_datetime<epoch>));
        case CDF_EPOCH16:
            return _details::ranges::transform<py::list, epoch16>(
                input, static_cast<PyObject* (*)(const epoch16)>(_details::to_datetime<epoch16>));
        case CDF_TIME_TT2000:
            return _details::ranges::transform<py::list, tt2000_t>(
                input, static_cast<PyObject* (*)(const tt2000_t)>(_details::to_datetime<tt2000_t>));
        default:
            throw std::invalid_argument(
                fmt::format("to_datetime requires a CDF time variable (CDF_EPOCH, CDF_EPOCH16, or "
                            "CDF_TIME_TT2000), got {}",
                    cdf_type_str(input.type())));
    }
    return py::list {};
}

template <typename time_t>
[[nodiscard]] inline time_t to_cdf_time_t(const PyObject* dt)
{
    return cdf::to_cdf_time<time_t>(
        _details::to_tp(reinterpret_cast<const PyDateTime_DateTime*>(dt)));
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


template <cdf_time_t time_t>
[[nodiscard]] inline py::array_t<time_t> from_datetimes(const py_list_or_py_tuple auto& input)
{
    auto result = py::array_t<time_t>(static_cast<py::ssize_t>(py::len(input)));
    auto* out = result.mutable_data();
    for (const PyObject* item : _details::ranges::py_list_or_tuple_view { input })
    {
        if (!PyDateTime_Check(item))
            throw py::type_error(fmt::format("expected datetime.datetime items, got {}",
                Py_TYPE(const_cast<PyObject*>(item))->tp_name));
        *out++ = _details::from_datetime<time_t>(item);
    }
    return result;
}


[[nodiscard]] inline bool _to_cdf_time_t(
    const cdf_time_t_span_t auto& input, cdf_time_t auto* output)
{
    for (const auto in : input)
    {
        *output = cdf::to_cdf_time<std::decay_t<decltype(output[0])>>(in);
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
    if (is_dt64(input))
    {
        if (is_dt64ns(input) || input.dtype().is(py::dtype::of<int64_t>())
            || input.dtype().is(py::dtype::of<uint64_t>()))
        {
            auto view = _details::ranges::make_span<const int64_t>(
                py::array(input.attr("view")("int64")));
            py::gil_scoped_release release;
            cdf::from_ns_from_1970(view, output);
            return true;
        }
        if (is_dt64ms(input))
        {
            return to_cdf_time_t<time_t>(py::array(input.attr("astype")("datetime64[ns]")), output);
        }
    }
    if (input.dtype().is(tt2000_dtype))
    {
        auto view = _details::ranges::make_span<const tt2000_t>(input);
        py::gil_scoped_release release;
        return _to_cdf_time_t(view, output);
    }
    if (input.dtype().is(epoch_dtype))
    {
        auto view = _details::ranges::make_span<const epoch>(input);
        py::gil_scoped_release release;
        return _to_cdf_time_t(view, output);
    }
    if (input.dtype().is(epoch16_dtype))
    {
        auto view = _details::ranges::make_span<const epoch16>(input);
        py::gil_scoped_release release;
        return _to_cdf_time_t(view, output);
    }
    return false;
}

template <typename time_t>
inline py::object to_cdf_time_t(const py::array& input)
{
    const auto contiguous = _details::c_contiguous(input);
    auto res = _details::fast_allocate_array<time_t>(contiguous);
    time_t* out = reinterpret_cast<time_t*>(res.request().ptr);
    if (to_cdf_time_t<time_t>(contiguous, out))
    {
        return res;
    }
    throw std::invalid_argument(fmt::format("Cannot convert array with dtype '{}' to CDF time type",
        std::string(input.dtype().attr("str").cast<std::string>())));
}


namespace time_fmt
{
// clang-format off
    alignas(64) constexpr char DIGIT_PAIRS[] =
        "00010203040506070809"
        "10111213141516171819"
        "20212223242526272829"
        "30313233343536373839"
        "40414243444546474849"
        "50515253545556575859"
        "60616263646566676869"
        "70717273747576777879"
        "80818283848586878889"
        "90919293949596979899";
// clang-format on

inline void write_2d(char* p, unsigned v)
{
    std::memcpy(p, DIGIT_PAIRS + v * 2, 2);
}

inline void write_3d(char* p, unsigned v)
{
    p[0] = '0' + static_cast<char>(v / 100);
    write_2d(p + 1, v % 100);
}

inline void write_4d(char* p, unsigned v)
{
    write_2d(p, v / 100);
    write_2d(p + 2, v % 100);
}

inline void write_nd(char* p, uint64_t v, int n)
{
    for (int i = n - 1; i >= 0; --i)
    {
        p[i] = '0' + static_cast<char>(v % 10);
        v /= 10;
    }
}

enum class field_kind : uint8_t
{
    YEAR,
    MONTH,
    DAY,
    DOY,
    HOUR,
    MINUTE,
    SECOND
};

struct field
{
    uint16_t offset;
    field_kind kind;
};

struct plan
{
    std::string tmpl;
    std::vector<field> fields;
    uint16_t subsec_offset = 0;
    uint8_t subsec_digits = 0;
    bool needs_doy = false;
};

template <typename time_t>
constexpr int default_subsec_digits()
{
    using tp_t = decltype(cdf::to_time_point(std::declval<time_t>()));
    using period = typename tp_t::duration::period;
    if constexpr (std::is_same_v<period, std::pico>)
        return 12;
    else if constexpr (std::is_same_v<period, std::nano>)
        return 9;
    else if constexpr (std::is_same_v<period, std::micro>)
        return 6;
    else if constexpr (std::is_same_v<period, std::milli>)
        return 3;
    else
        return 0;
}

template <typename time_t>
plan compile(const std::string& fmt)
{
    plan p;
    const int subsec_n = default_subsec_digits<time_t>();
    for (std::size_t i = 0; i < fmt.size(); ++i)
    {
        if (fmt[i] == '%' && i + 1 < fmt.size())
        {
            ++i;
            switch (fmt[i])
            {
                case 'Y':
                    p.fields.push_back({ static_cast<uint16_t>(p.tmpl.size()), field_kind::YEAR });
                    p.tmpl.append("0000");
                    break;
                case 'm':
                    p.fields.push_back({ static_cast<uint16_t>(p.tmpl.size()), field_kind::MONTH });
                    p.tmpl.append("00");
                    break;
                case 'd':
                    p.fields.push_back({ static_cast<uint16_t>(p.tmpl.size()), field_kind::DAY });
                    p.tmpl.append("00");
                    break;
                case 'j':
                    p.fields.push_back({ static_cast<uint16_t>(p.tmpl.size()), field_kind::DOY });
                    p.needs_doy = true;
                    p.tmpl.append("000");
                    break;
                case 'H':
                    p.fields.push_back({ static_cast<uint16_t>(p.tmpl.size()), field_kind::HOUR });
                    p.tmpl.append("00");
                    break;
                case 'M':
                    p.fields.push_back(
                        { static_cast<uint16_t>(p.tmpl.size()), field_kind::MINUTE });
                    p.tmpl.append("00");
                    break;
                case 'S':
                    p.fields.push_back(
                        { static_cast<uint16_t>(p.tmpl.size()), field_kind::SECOND });
                    p.tmpl.append("00");
                    if (subsec_n > 0)
                    {
                        p.tmpl += '.';
                        p.subsec_offset = static_cast<uint16_t>(p.tmpl.size());
                        p.subsec_digits = static_cast<uint8_t>(subsec_n);
                        p.tmpl.append(static_cast<std::size_t>(subsec_n), '0');
                    }
                    break;
                case '%':
                    p.tmpl += '%';
                    break;
                default:
                    throw std::invalid_argument(
                        fmt::format("to_time_string: unsupported format specifier '%{}'", fmt[i]));
            }
        }
        else
        {
            p.tmpl += fmt[i];
        }
    }
    return p;
}

template <typename time_t>
void format_chunk(const time_t* input, std::size_t count, char* output, const plan& p)
{
    using namespace std::chrono;
    const auto str_len = p.tmpl.size();
    const auto* tmpl_data = p.tmpl.data();
    const auto* fields_data = p.fields.data();
    const auto nfields = p.fields.size();

    for (std::size_t i = 0; i < count; ++i)
    {
        auto* out = output + i * str_len;
        std::memcpy(out, tmpl_data, str_len);

        auto tp = cdf::to_time_point(input[i]);
        auto dp = floor<days>(tp);
        year_month_day ymd { dp };
        hh_mm_ss time { tp - dp };

        const auto y = static_cast<unsigned>(static_cast<int>(ymd.year()));
        const auto mo = static_cast<unsigned>(ymd.month());
        const auto d = static_cast<unsigned>(ymd.day());
        const auto h = static_cast<unsigned>(time.hours().count());
        const auto mi = static_cast<unsigned>(time.minutes().count());
        const auto s = static_cast<unsigned>(time.seconds().count());

        for (std::size_t f = 0; f < nfields; ++f)
        {
            auto* dest = out + fields_data[f].offset;
            switch (fields_data[f].kind)
            {
                case field_kind::YEAR:
                    write_4d(dest, y);
                    break;
                case field_kind::MONTH:
                    write_2d(dest, mo);
                    break;
                case field_kind::DAY:
                    write_2d(dest, d);
                    break;
                case field_kind::HOUR:
                    write_2d(dest, h);
                    break;
                case field_kind::MINUTE:
                    write_2d(dest, mi);
                    break;
                case field_kind::SECOND:
                    write_2d(dest, s);
                    break;
                case field_kind::DOY:
                    write_3d(dest,
                        static_cast<unsigned>(
                            (dp - sys_days { ymd.year() / January / day { 1 } }).count() + 1));
                    break;
            }
        }
        if (p.subsec_digits > 0)
        {
            write_nd(out + p.subsec_offset, static_cast<uint64_t>(time.subseconds().count()),
                p.subsec_digits);
        }
    }
}
} // namespace time_fmt

template <typename time_t>
void format_time_to_bytes(
    const time_t* input, std::size_t count, char* output, const time_fmt::plan& plan)
{
    static constexpr std::size_t min_chunk = 1024;
    static const auto threads_count
        = static_cast<std::size_t>(std::max(1U, std::thread::hardware_concurrency()));
    if (count >= min_chunk * threads_count)
    {
        const auto chunk_size = (count + threads_count - 1) / threads_count;
        std::vector<std::thread> threads;
        threads.reserve(threads_count);
        std::size_t start = 0;
        for (std::size_t i = 0; i < threads_count && start < count; ++i)
        {
            const auto sz = std::min(chunk_size, count - start);
            threads.emplace_back(time_fmt::format_chunk<time_t>, input + start, sz,
                output + start * plan.tmpl.size(), std::cref(plan));
            start += sz;
        }
        for (auto& t : threads)
        {
            t.join();
        }
    }
    else
    {
        time_fmt::format_chunk(input, count, output, plan);
    }
}

template <typename time_t>
[[nodiscard]] py::array to_time_string_span(const std::span<const time_t> input,
    const std::vector<ssize_t>& shape, const std::string& format)
{
    if (input.empty())
    {
        return py::array(py::dtype("S1"), std::vector<ssize_t> { 0 });
    }
    auto plan = time_fmt::compile<time_t>(format);
    auto result = py::array(py::dtype(fmt::format("S{}", plan.tmpl.size())), shape);
    auto* buf = static_cast<char*>(result.mutable_data());
    {
        py::gil_scoped_release release;
        format_time_to_bytes(input.data(), input.size(), buf, plan);
    }
    return result;
}

template <typename time_t>
[[nodiscard]] py::array to_time_string(const py::array_t<time_t>& input, const std::string& format)
{
    const auto contiguous = _details::c_contiguous(input);
    auto info = contiguous.request();
    std::vector<ssize_t> shape(input.shape(), input.shape() + input.ndim());
    return to_time_string_span(
        std::span { static_cast<const time_t*>(info.ptr), static_cast<std::size_t>(info.size) },
        shape, format);
}

[[nodiscard]] py::array to_time_string(const Variable& input, const std::string& format)
{
    using enum cdf::CDF_Types;
    auto shape = _details::shape_ssize_t(input);
    const auto size = static_cast<std::size_t>(
        std::accumulate(shape.begin(), shape.end(), ssize_t { 1 }, std::multiplies<>()));
    switch (input.type())
    {
        case CDF_EPOCH:
            return to_time_string_span(
                std::span { input.get<CDF_EPOCH>().data(), size }, shape, format);
        case CDF_EPOCH16:
            return to_time_string_span(
                std::span { input.get<CDF_EPOCH16>().data(), size }, shape, format);
        case CDF_TIME_TT2000:
            return to_time_string_span(
                std::span { input.get<CDF_TIME_TT2000>().data(), size }, shape, format);
        default:
            throw std::invalid_argument(fmt::format(
                "to_time_string requires a CDF time variable (CDF_EPOCH, CDF_EPOCH16, or "
                "CDF_TIME_TT2000), got {}",
                cdf_type_str(input.type())));
    }
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

auto def_to_time_string_functions(auto& mod)
{
    mod.def("to_time_string",
        static_cast<py::array (*)(const py::array_t<epoch>&, const std::string&)>(
            to_time_string<epoch>),
        py::arg { "values" }.noconvert(), py::arg { "format" });
    mod.def("to_time_string",
        static_cast<py::array (*)(const py::array_t<epoch16>&, const std::string&)>(
            to_time_string<epoch16>),
        py::arg { "values" }.noconvert(), py::arg { "format" });
    mod.def("to_time_string",
        static_cast<py::array (*)(const py::array_t<tt2000_t>&, const std::string&)>(
            to_time_string<tt2000_t>),
        py::arg { "values" }.noconvert(), py::arg { "format" });
    mod.def("to_time_string",
        static_cast<py::array (*)(const Variable&, const std::string&)>(to_time_string),
        py::arg { "variable" }, py::arg { "format" });
}

auto def_time_conversion_functions(auto& mod)
{
    PyDateTime_IMPORT;
    // forward
    {
        def_to_dt64_conversion_functions(mod);
        def_to_datetime_conversion_functions(mod);
        def_to_time_string_functions(mod);
    }

    // backward
    {
        mod.def("to_tt2000", static_cast<py::list (*)(const py::tuple&)>(to_tt2000),
            py::arg { "values" }.noconvert());
        mod.def("to_tt2000", static_cast<py::list (*)(const py::list&)>(to_tt2000),
            py::arg { "values" }.noconvert());

        mod.def("to_tt2000",
            [](py_datetime dt) { return _details::from_datetime<tt2000_t>(dt.obj); });

        mod.def("to_tt2000",
            [](const py::array& input) -> py::object { return to_cdf_time_t<tt2000_t>(input); });

        mod.def("to_epoch", [](py_datetime dt) { return _details::from_datetime<epoch>(dt.obj); });
        mod.def("to_epoch", from_datetimes<epoch, py::list>, py::arg { "values" }.noconvert());
        mod.def("to_epoch", from_datetimes<epoch, py::tuple>, py::arg { "values" }.noconvert());
        mod.def("to_epoch",
            [](const py::array& input) -> py::object { return to_cdf_time_t<epoch>(input); });

        mod.def(
            "to_epoch16", [](py_datetime dt) { return _details::from_datetime<epoch16>(dt.obj); });
        mod.def("to_epoch16", from_datetimes<epoch16, py::list>, py::arg { "values" }.noconvert());
        mod.def(
            "to_epoch16", from_datetimes<epoch16, py::tuple>, py::arg { "values" }.noconvert());
        mod.def("to_epoch16",
            [](const py::array& input) -> py::object { return to_cdf_time_t<epoch16>(input); });
    }
}
