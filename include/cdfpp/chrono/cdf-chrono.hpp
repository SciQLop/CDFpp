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

#include "cdf-chrono-constants.hpp"
#include "cdf-chrono-impl.hpp"
#include "cdfpp/cdf-debug.hpp"
#include "cdfpp/cdf-enums.hpp"
#include "cdfpp/no_init_vector.hpp"
#include <cdfpp/vectorized/cdf-chrono.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <span>
#include <thread>

#ifndef CDFPP_NO_SIMD
#include "cdfpp/vectorized/cdf-chrono.hpp"
#endif


using namespace std::chrono;

namespace cdf
{

using namespace cdf::chrono;

namespace chrono::_impl
{
    static inline std::size_t ideal_threads_count()
    {
        static const auto threads_count = std::thread::hardware_concurrency() >= 32 ? 8U : 2U;
        return threads_count;
    }

    /* the takeaway is that threading is worth it on platforms with many cores where we infer
     * big memory bandwidht because of many memry channels.
     * On a typical 4 core laptop with 8 threads, threading is not worth it because we already
     * top reach ~70% of memory bandwidth with a single thread.
     */
    template <std::size_t min_chunk_size = 1 * 1024 * 1024>
    static inline void _thread_if_needed(
        const auto& input, auto* const output, const auto& function)
    {
        static const auto threads_count = ideal_threads_count();
        const auto count = std::size(input);
        if ((count >= (min_chunk_size * threads_count)))
        {
            const auto chunk_size = [count]()
            {
                auto cs = (count / threads_count);
                while ((cs * threads_count) < count)
                {
                    cs += min_chunk_size;
                }
                return cs;
            }();
            std::vector<std::thread> threads;
            threads.reserve(threads_count);
            std::size_t start = 0;
            std::size_t end = chunk_size;
            for (std::size_t i = 0; i < threads_count; ++i)
            {
                threads.emplace_back([input, start, end, output, &function]()
                    { function(input.subspan(start, end), output + start); });
                start = end;
                end = ((end + chunk_size) > count) ? count : end + chunk_size;
            }
            for (auto& t : threads)
            {
                t.join();
            }
        }
        else
        {
            function(input, output);
        }
    }

}

static inline void to_ns_from_1970(const cdf_time_t_span_t auto& input, int64_t* output)
{
    chrono::_impl::_thread_if_needed<1 * 1024 * 1024>(input, output,
        [](const cdf_time_t_span_t auto& input, int64_t* output)
        {
#ifndef CDFPP_NO_SIMD
            if (input.size() >= 8)
            {
                vectorized_to_ns_from_1970(input, output);
            }
            else
            {
                _impl::scalar_to_ns_from_1970(input, output);
            }
#else
            _impl::scalar_to_ns_from_1970(input, output);
#endif
        });
}

epoch to_epoch(const time_point_t auto& tp)
{
    using namespace std::chrono;
    return epoch { duration_cast<milliseconds>(tp.time_since_epoch()).count()
        + constants::epoch_offset_miliseconds };
}

no_init_vector<epoch> to_epoch(const auto& tps)
{
    no_init_vector<epoch> result(std::size(tps));
    std::transform(std::cbegin(tps), std::cend(tps), std::begin(result),
        static_cast<epoch (*)(const decltype(tps[0]))>(to_epoch));
    return result;
}

epoch16 to_epoch16(const time_point_t auto& tp)
{
    auto se = static_cast<double>(duration_cast<seconds>(tp.time_since_epoch()).count());
    auto s = se + constants::epoch_offset_seconds;
    auto ps = (static_cast<double>(duration_cast<nanoseconds>(tp.time_since_epoch()).count())
                  - (se * 1e9))
        * 1000.;
    return epoch16 { s, ps };
}


no_init_vector<epoch16> to_epoch16(const time_point_collection_t auto& tps)
{
    no_init_vector<epoch16> result(std::size(tps));
    std::transform(std::cbegin(tps), std::cend(tps), std::begin(result),
        static_cast<epoch16 (*)(const decltype(tps[0]))>(to_epoch16));
    return result;
}

tt2000_t to_tt2000(const time_point_t auto& tp)
{
    using namespace std::chrono;
    auto nsec = duration_cast<nanoseconds>(tp.time_since_epoch()).count();
    return tt2000_t { nsec - constants::tt2000_offset + _impl::leap_second(nsec) };
}

no_init_vector<tt2000_t> to_tt2000(const time_point_collection_t auto& tps)
{
    using namespace std::chrono;
    no_init_vector<tt2000_t> result(std::size(tps));
    std::transform(std::cbegin(tps), std::cend(tps), std::begin(result),
        static_cast<tt2000_t (*)(const decltype(tps[0]))>(to_tt2000));
    return result;
}

template <typename T>
T to_cdf_time(const time_point_t auto& tp)
{
    if constexpr (std::is_same_v<T, tt2000_t>)
        return to_tt2000(tp);
    else if constexpr (std::is_same_v<T, epoch>)
        return to_epoch(tp);
    else if constexpr (std::is_same_v<T, epoch16>)
        return to_epoch16(tp);
    else
        throw std::runtime_error("Unsupported cdf time type");
}

template <typename T>
T to_cdf_time(const cdf_time_t auto& in)
{
    using input_t = std::decay_t<decltype(in)>;
    if constexpr (std::is_same_v<T, input_t>)
        return in;
    else
        return to_cdf_time<T>(to_time_point(in));
}

static inline void from_ns_from_1970(const std::span<const int64_t>& input, cdf_time_t auto* output)
{
    std::transform(std::cbegin(input), std::cend(input), output,
        [](const int64_t ns)
        {
            return to_cdf_time<std::decay_t<decltype(output[0])>>(
                std::chrono::system_clock::time_point {} + std::chrono::nanoseconds(ns));
        });
}

inline auto to_time_point(const epoch& ep)
{
    double ms = ep.mseconds - constants::epoch_offset_miliseconds, ns;
    ns = std::modf(ms, &ms) * 1000000.;
    return std::chrono::time_point<std::chrono::system_clock> {} + milliseconds(int64_t(ms))
        + nanoseconds(int64_t(ns));
}

inline auto to_time_point(const epoch16& ep)
{
    double s = ep.seconds - constants::epoch_offset_seconds, ns;
    ns = ep.picoseconds / 1000.;
    return std::chrono::time_point<std::chrono::system_clock> {} + seconds(static_cast<int64_t>(s))
        + nanoseconds(static_cast<int64_t>(ns));
}

inline auto to_time_point(const tt2000_t& ep)
{
    using namespace std::chrono;
    return time_point<system_clock> {}
    + nanoseconds(ep.nseconds - _impl::leap_second(ep) + constants::tt2000_offset);
}

}
