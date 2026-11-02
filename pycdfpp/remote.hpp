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

#include <cdfpp/cdf-io/loading/loading.hpp>
#include <cpp_utils/io/range_reader.hpp>
#include <cpp_utils/io/remote_buffer.hpp>

#include <cstring>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>

#include <pybind11/pybind11.h>
namespace py = pybind11;

namespace _details
{
// Set by an atexit callback, which runs while Python is still whole: from then on, readers
// and their references are left alone, since finalisation may start at any moment.
inline std::atomic<bool> python_exiting = false;

inline bool python_usable()
{
#if PY_VERSION_HEX >= 0x030D0000
    return !python_exiting && Py_IsInitialized() && !Py_IsFinalizing();
#else
    return !python_exiting && Py_IsInitialized() && !_Py_IsFinalizing();
#endif
}

inline void stop_using_python_at_exit()
{
    py::module_::import("atexit").attr("register")(
        py::cpp_function([] { python_exiting = true; }));
}

// Owns one reference to the Python reader. The last copy may die on any thread, or after the
// interpreter is gone: then the reference is leaked on purpose, touching Python would crash.
struct python_reference
{
    PyObject* object;
    explicit python_reference(py::object o) : object { o.release().ptr() } { }
    python_reference(const python_reference&) = delete;
    python_reference& operator=(const python_reference&) = delete;
    ~python_reference()
    {
        if (!python_usable())
            return;
        const auto gil = PyGILState_Ensure();
        Py_DECREF(object);
        PyGILState_Release(gil);
    }
};

// Runs wait() without the GIL when this thread holds it: whoever wait() waits for may need it.
template <typename F>
void without_gil(F&& wait)
{
    if (python_usable() && PyGILState_Check())
    {
        py::gil_scoped_release release;
        wait();
    }
    else
        wait();
}

inline void wait_for_variable_lock_without_gil(std::unique_lock<std::mutex>& lock)
{
    without_gil([&] { lock.lock(); });
}

inline py::list to_python(std::span<const cpp_utils::io::byte_range> ranges)
{
    py::list result;
    for (const auto& range : ranges)
        result.append(py::make_tuple(range.offset, range.size));
    return result;
}

// PyBUF_SIMPLE refuses strided buffers, which a flat copy would scramble.
inline void copy_exact(const py::handle& chunk, char* destination, std::size_t size)
{
    Py_buffer view;
    if (PyObject_GetBuffer(chunk.ptr(), &view, PyBUF_SIMPLE) != 0)
        throw py::error_already_set();
    const auto length = static_cast<std::size_t>(view.len);
    if (length == size)
        std::memcpy(destination, view.buf, size);
    PyBuffer_Release(&view);
    if (length != size)
        throw std::runtime_error { "reader returned " + std::to_string(length)
            + " bytes for a range of " + std::to_string(size) };
}
}

// A cpp_utils range_reader backed by a Python object with size() and
// read_ranges(ranges) -> list of bytes-like.
class python_range_reader
{
    std::size_t p_size;
    std::shared_ptr<_details::python_reference> p_reader;

    void read_copy(const py::handle& reader, std::span<const cpp_utils::io::byte_range> ranges,
        std::span<char* const> destinations) const
    {
        const py::list chunks = reader.attr("read_ranges")(_details::to_python(ranges));
        if (std::size(chunks) != std::size(ranges))
            throw std::runtime_error { "reader returned " + std::to_string(std::size(chunks))
                + " chunks for " + std::to_string(std::size(ranges)) + " ranges" };
        for (std::size_t i = 0; i < std::size(ranges); ++i)
            _details::copy_exact(chunks[i], destinations[i], ranges[i].size);
    }

public:
    // Called with the GIL held.
    explicit python_range_reader(py::object reader)
            : p_size { reader.attr("size")().cast<std::size_t>() }
            , p_reader { std::make_shared<_details::python_reference>(std::move(reader)) }
    {
    }

    [[nodiscard]] std::size_t size() const { return p_size; }

    template <typename F>
    void blocking_wait(F&& wait) const
    {
        _details::without_gil(std::forward<F>(wait));
    }

    void read_ranges(std::span<const cpp_utils::io::byte_range> ranges,
        std::span<char* const> destinations) const
    {
        if (!_details::python_usable())
            throw std::runtime_error { "Python reader used after interpreter shutdown" };
        py::gil_scoped_acquire gil;
        read_copy(py::handle { p_reader->object }, ranges, destinations);
    }
};
static_assert(cpp_utils::io::range_reader<python_range_reader>);

namespace _details
{
inline cpp_utils::io::region_backing parse_backing(const std::string& value)
{
    if (value == "memory")
        return cpp_utils::io::region_backing::memory;
    if (value == "file")
        return cpp_utils::io::region_backing::file;
    throw py::value_error { "backing must be 'memory' or 'file', got '" + value + "'" };
}

struct remote_knobs
{
    std::optional<std::size_t> read_ahead;
    std::optional<std::size_t> tail_prefetch;
    std::optional<std::string> backing;
    std::optional<std::string> temp_dir;
    std::optional<std::size_t> download_whole_below;
};

// Keywords win over the CDFPP_REMOTE_* environment variables, which win over the defaults.
inline cpp_utils::io::remote_buffer_options remote_options(const remote_knobs& knobs)
{
    auto options = cdf::io::default_remote_options();
    options.read_ahead = knobs.read_ahead.value_or(options.read_ahead);
    options.tail_prefetch = knobs.tail_prefetch.value_or(options.tail_prefetch);
    options.download_whole_below = knobs.download_whole_below.value_or(options.download_whole_below);
    if (knobs.backing)
        options.backing = parse_backing(*knobs.backing);
    if (knobs.temp_dir)
        options.temp_dir = *knobs.temp_dir;
    return options;
}
}
