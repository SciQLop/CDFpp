/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2026, Laboratory of Plasma Physics- CNRS
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
#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

namespace cdf::parallel
{

// WebAssembly without pthreads (Pyodide, the CDFpp Explorer) can't start threads:
// std::thread throws there, so work must stay on the calling thread.
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
inline constexpr bool threads_supported = false;
#else
inline constexpr bool threads_supported = true;
#endif

// Below this much data, starting threads costs more than (de)compressing it on one.
inline constexpr std::size_t min_bytes_worth_threads = 1 << 20;

inline std::size_t hardware_threads()
{
    return std::max(1U, std::thread::hardware_concurrency());
}

// Calls fn(i) for every i in [0, count), on up to max_threads threads including the caller.
// Tasks may have very different costs, so each thread takes the next index when it is done.
// The first exception is rethrown once every thread has stopped.
template <typename fn_t>
void for_each_index(std::size_t count, std::size_t max_threads, fn_t&& fn)
{
    const std::size_t threads = threads_supported ? std::min(count, max_threads) : 1;
    if (threads <= 1)
    {
        for (std::size_t i = 0; i < count; ++i)
            fn(i);
        return;
    }
    std::atomic<std::size_t> next { 0 };
    std::exception_ptr error;
    std::mutex error_mutex;
    const auto work = [&]()
    {
        for (auto i = next++; i < count; i = next++)
        {
            try
            {
                fn(i);
            }
            catch (...)
            {
                std::scoped_lock lock { error_mutex };
                if (!error)
                    error = std::current_exception();
                next = count;
            }
        }
    };
    std::vector<std::thread> workers;
    workers.reserve(threads - 1);
    try
    {
        for (std::size_t t = 1; t < threads; ++t)
            workers.emplace_back(work);
    }
    catch (const std::system_error&)
    {
        // The system refused another thread: the ones already started, and this one, do the rest.
    }
    work();
    for (auto& worker : workers)
        worker.join();
    if (error)
        std::rethrow_exception(error);
}

} // namespace cdf::parallel
