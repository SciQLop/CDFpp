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
#include <cpp_utils/containers/no_init_vector.hpp>
using cpp_utils::containers::no_init_vector;
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
    // WebAssembly without pthreads (Pyodide, the CDFpp Explorer) can't start threads:
    // std::thread throws there, so conversions must stay on the calling thread.
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    inline constexpr bool threads_supported = false;
#else
    inline constexpr bool threads_supported = true;
#endif

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
        if (threads_supported && count >= min_chunk_size * threads_count)
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
            std::size_t sz = chunk_size;
            for (std::size_t i = 0; i < threads_count; ++i)
            {
                threads.emplace_back([input, start, sz, output, &function]()
                    { function(input.subspan(start, sz), output + start); });
                start += sz;
                sz = ((start + chunk_size) > count) ? count - start : chunk_size;
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
    auto total_ns = duration_cast<nanoseconds>(tp.time_since_epoch()).count();
    auto se = duration_cast<seconds>(tp.time_since_epoch()).count();
    auto s = static_cast<double>(se) + constants::epoch_offset_seconds;
    auto ps = static_cast<double>(total_ns - se * 1'000'000'000LL) * 1000.;
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

namespace chrono::_impl
{
    // A double cast to int64_t, or an int64_t later multiplied into a finer-grained
    // chrono::duration (e.g. milliseconds -> nanoseconds is a *1'000'000), is
    // undefined behavior once the magnitude would overflow the destination — and
    // system_clock::time_point's own nanosecond-resolution duration only spans
    // roughly 1677-09-21..2262-04-11 to begin with. A CDF epoch/epoch16/tt2000 value
    // representing a date outside that (a mis-encoded fill/pad sentinel that missed
    // cdf-repr.hpp's exact-literal check, or just corrupt data) must not crash by
    // invoking that UB, so clamp before doing any of this arithmetic - caught in
    // practice by UBSan: "signed integer overflow: -9223372036854775808 * 1000000
    // cannot be represented in type 'long int'".
    // simplify: saturates to the nearest representable boundary rather than
    // reporting an error; fine since such values are already outside CDF's own
    // meaningful calendar range. Upgrade path: have callers that need to distinguish
    // "saturated" from "a real boundary date" check the input against these same
    // bounds themselves before calling to_time_point().
    inline double clamp_to_safe_ms(double ms) noexcept
    {
        // |ms| * 1'000'000 plus up to 999'999 ns of sub-millisecond remainder added
        // on top afterwards must both stay within int64.
        constexpr double max_ms = 9'223'372'000'000.0;
        if (std::isnan(ms))
            return 0.;
        return std::clamp(ms, -max_ms, max_ms);
    }

    inline double clamp_to_safe_s(double s) noexcept
    {
        // |s| * 1'000'000'000 plus up to 999'999'999 ns of sub-second remainder
        // added on top afterwards must both stay within int64.
        constexpr double max_s = 9'223'372'000.0;
        if (std::isnan(s))
            return 0.;
        return std::clamp(s, -max_s, max_s);
    }

    inline double clamp_to_safe_sub_second_ns(double ns) noexcept
    {
        if (std::isnan(ns))
            return 0.;
        return std::clamp(ns, -999'999'999., 999'999'999.);
    }

    inline int64_t clamp_to_safe_tt2000_ns(int64_t nseconds) noexcept
    {
        // Generous headroom for tt2000_offset (~9.47e17) + any leap_second correction
        // (at most tens of seconds in ns) added on top afterwards.
        constexpr int64_t margin = 1'000'000'000'000'000'000LL;
        return std::clamp(nseconds, std::numeric_limits<int64_t>::min() + margin,
            std::numeric_limits<int64_t>::max() - margin);
    }
}

inline auto to_time_point(const epoch& ep)
{
    double ms = _impl::clamp_to_safe_ms(ep.mseconds - constants::epoch_offset_miliseconds), ns;
    ns = std::modf(ms, &ms) * 1000000.;
    return std::chrono::time_point<std::chrono::system_clock> {} + milliseconds(int64_t(ms))
        + nanoseconds(int64_t(ns));
}

inline auto to_time_point(const epoch16& ep)
{
    // Unlike epoch (single double), epoch16 stores integer seconds separately from
    // picoseconds, so this subtraction is between integer-valued doubles both well
    // within 2^53 — no catastrophic cancellation.
    double s = _impl::clamp_to_safe_s(ep.seconds - constants::epoch_offset_seconds), ns;
    ns = _impl::clamp_to_safe_sub_second_ns(ep.picoseconds / 1000.);
    return std::chrono::time_point<std::chrono::system_clock> {} + seconds(static_cast<int64_t>(s))
        + nanoseconds(static_cast<int64_t>(ns));
}

inline auto to_time_point(const tt2000_t& ep)
{
    using namespace std::chrono;
    const int64_t leap = _impl::leap_second(ep);
    const int64_t safe_ns = _impl::clamp_to_safe_tt2000_ns(ep.nseconds);
    return time_point<system_clock> {} + nanoseconds(safe_ns - leap + constants::tt2000_offset);
}

}
