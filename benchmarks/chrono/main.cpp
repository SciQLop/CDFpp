#include <benchmark/benchmark.h>
#include <cdfpp/chrono/cdf-chrono.hpp>
#include <cstring>

inline constexpr std::size_t mega(std::size_t n)
{
    return n * 1024 * 1024;
}

template <typename T>
no_init_vector<T> generate_sorted_time_vectors(T start, T end, std::size_t count)
{
    T step = (end - start) / count;
    no_init_vector<T> result(count);
    for (auto& v : result)
    {
        v = T(start);
        start += step;
    }
    return result;
}

template <cdf::cdf_time_t T>
inline auto& value_ref(T& t)
{
    if constexpr (std::is_same_v<T, cdf::tt2000_t>)
    {
        return t.nseconds;
    }
    else if constexpr (std::is_same_v<T, cdf::epoch>)
    {
        return t.mseconds;
    }
    else if constexpr (std::is_same_v<T, cdf::epoch16>)
    {
        return t.seconds;
    }
}


template <cdf::cdf_time_t T>
no_init_vector<T> generate_sorted_time_vectors(T start, T end, std::size_t count)
{
    T step((value_ref(end) - value_ref(start)) / count);
    no_init_vector<T> result(count);
    for (auto& v : result)
    {
        v = start;
        value_ref(start) += value_ref(step);
    }
    return result;
}


template <typename leap_func_t>
static void BM_leap_second(benchmark::State& state, leap_func_t leap_func)
{
    constexpr std::size_t start = 63'072'000'000'000'000;
    constexpr std::size_t end = 2'081'948'754'000'000'000;

    const auto time_vect = generate_sorted_time_vectors<int64_t>(start, end, state.range(0));
    for (auto _ : state)
    {
        int64_t leap = 0;
        for (const auto& time : time_vect)
        {
            leap ^= leap_func(time);
        }
        benchmark::DoNotOptimize(leap);
    }
    state.counters["Epochs"] = std::size(time_vect);
    state.counters["epochs_per_second"]
        = benchmark::Counter(std::size(time_vect), benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK_CAPTURE(BM_leap_second, branchless, cdf::chrono::_impl::leap_second_branchless)
    ->RangeMultiplier(4)
    ->Range(100, mega(64))
    ->Complexity();

BENCHMARK_CAPTURE(
    BM_leap_second, baseline, static_cast<int64_t (*)(int64_t)>(cdf::chrono::_impl::leap_second))
    ->RangeMultiplier(4)
    ->Range(100, mega(64))
    ->Complexity();

template <typename time_t, typename func_t>
static void BM_to_ns_from_1970(benchmark::State& state, func_t func, time_t start, time_t end)
{
    const auto time_vect = generate_sorted_time_vectors<time_t>(start, end, state.range(0));
    no_init_vector<int64_t> output(time_vect.size());
    for (auto _ : state)
    {
        benchmark::ClobberMemory();
        func(time_vect, output.data());
        benchmark::DoNotOptimize(output);
    }
    state.counters["Epochs"] = std::size(time_vect);
    state.counters["epochs_per_second"]
        = benchmark::Counter(std::size(time_vect), benchmark::Counter::kIsIterationInvariantRate);
}

BENCHMARK_CAPTURE(BM_to_ns_from_1970, tt2000_scalar,
    static_cast<void (*)(const std::span<const cdf::tt2000_t>&, int64_t* const)>(
        cdf::chrono::_impl::scalar_to_ns_from_1970),
    cdf::tt2000_t{63'072'000'000'000'000},
    cdf::tt2000_t{2'081'948'754'000'000'000}
    )
    ->RangeMultiplier(4)
    ->Range(10, mega(1024))
    ->Complexity()
    ->UseRealTime();

BENCHMARK_CAPTURE(BM_to_ns_from_1970, tt2000_vectorized,
    static_cast<void (*)(const std::span<const cdf::tt2000_t>&, int64_t* const)>(
        vectorized_to_ns_from_1970),
    cdf::tt2000_t{63'072'000'000'000'000},
    cdf::tt2000_t{2'081'948'754'000'000'000}
    )
    ->RangeMultiplier(4)
    ->Range(10, mega(1024))
    ->Complexity()
    ->UseRealTime();

BENCHMARK_CAPTURE(BM_to_ns_from_1970, tt2000_entry_point,
    static_cast<void (*)(const std::span<const cdf::tt2000_t>& , int64_t* const)>(
        cdf::to_ns_from_1970),
    cdf::tt2000_t{63'072'000'000'000'000},
    cdf::tt2000_t{2'081'948'754'000'000'000}
    )
    ->RangeMultiplier(4)
    ->Range(10, mega(1024))
    ->Complexity()
    ->UseRealTime();

BENCHMARK_CAPTURE(BM_to_ns_from_1970, epoch_scalar,
    static_cast<void (*)(const std::span<const cdf::epoch>&, int64_t* const)>(
        cdf::chrono::_impl::scalar_to_ns_from_1970),
    cdf::epoch{62167215600000.0},
    cdf::epoch{63745052400000.0}
    )
    ->RangeMultiplier(4)
    ->Range(10, mega(1024))
    ->Complexity()
    ->UseRealTime();

BENCHMARK_CAPTURE(BM_to_ns_from_1970, epoch_vectorized,
    static_cast<void (*)(const std::span<const cdf::epoch>&, int64_t* const)>(
        vectorized_to_ns_from_1970),
    cdf::epoch{62167215600000.0},
    cdf::epoch{63745052400000.0}
    )
    ->RangeMultiplier(4)
    ->Range(10, mega(1024))
    ->Complexity()
    ->UseRealTime();

BENCHMARK_CAPTURE(BM_to_ns_from_1970, epoch_entry_point,
    static_cast<void (*)(const std::span<const cdf::epoch>&, int64_t* const)>(
        cdf::to_ns_from_1970),
    cdf::epoch{62167215600000.0},
    cdf::epoch{63745052400000.0}
    )
    ->RangeMultiplier(4)
    ->Range(10, mega(1024))
    ->Complexity()
    ->UseRealTime();

BENCHMARK_MAIN();
