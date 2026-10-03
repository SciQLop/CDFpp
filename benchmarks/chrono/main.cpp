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

// Most real time axes: sorted, after the last leap second (2017). TT2000 2019 to 2020.
BENCHMARK_CAPTURE(BM_to_ns_from_1970, tt2000_recent_scalar,
    static_cast<void (*)(const std::span<const cdf::tt2000_t>&, int64_t* const)>(
        cdf::chrono::_impl::scalar_to_ns_from_1970),
    cdf::tt2000_t { 599'572'869'184'000'000 }, cdf::tt2000_t { 631'195'269'184'000'000 })
    ->RangeMultiplier(32)
    ->Range(1024, mega(64))
    ->UseRealTime();

BENCHMARK_CAPTURE(BM_to_ns_from_1970, tt2000_recent_vectorized,
    static_cast<void (*)(const std::span<const cdf::tt2000_t>&, int64_t* const)>(
        vectorized_to_ns_from_1970),
    cdf::tt2000_t { 599'572'869'184'000'000 }, cdf::tt2000_t { 631'195'269'184'000'000 })
    ->RangeMultiplier(32)
    ->Range(1024, mega(64))
    ->UseRealTime();

template <typename func_t>
static void BM_epoch16_to_ns_from_1970(benchmark::State& state, func_t func)
{
    no_init_vector<cdf::epoch16> time_vect(state.range(0));
    double seconds = 63'745'052'400.0;
    double picoseconds = 0.;
    for (auto& v : time_vect)
    {
        v = cdf::epoch16 { seconds, picoseconds };
        picoseconds += 62'500'000'000.; // 16 Hz
        if (picoseconds >= 1e12)
        {
            picoseconds -= 1e12;
            seconds += 1.;
        }
    }
    no_init_vector<int64_t> output(time_vect.size());
    for (auto _ : state)
    {
        benchmark::ClobberMemory();
        func(time_vect, output.data());
        benchmark::DoNotOptimize(output);
    }
    state.counters["epochs_per_second"]
        = benchmark::Counter(std::size(time_vect), benchmark::Counter::kIsIterationInvariantRate);
}

BENCHMARK_CAPTURE(BM_epoch16_to_ns_from_1970, scalar,
    static_cast<void (*)(const std::span<const cdf::epoch16>&, int64_t* const)>(
        cdf::chrono::_impl::scalar_to_ns_from_1970))
    ->RangeMultiplier(32)
    ->Range(1024, mega(64))
    ->UseRealTime();

BENCHMARK_CAPTURE(BM_epoch16_to_ns_from_1970, vectorized,
    static_cast<void (*)(const std::span<const cdf::epoch16>&, int64_t* const)>(
        vectorized_to_ns_from_1970))
    ->RangeMultiplier(32)
    ->Range(1024, mega(64))
    ->UseRealTime();

// datetime64 to TT2000, when saving: sorted 2019 to 2020, and 1972 to 2036.
template <typename func_t>
static void BM_ns_from_1970_to_tt2000(
    benchmark::State& state, func_t func, int64_t start, int64_t end)
{
    const auto time_vect = generate_sorted_time_vectors<int64_t>(start, end, state.range(0));
    no_init_vector<cdf::tt2000_t> output(time_vect.size());
    for (auto _ : state)
    {
        benchmark::ClobberMemory();
        func(time_vect, output.data());
        benchmark::DoNotOptimize(output);
    }
    state.counters["epochs_per_second"]
        = benchmark::Counter(std::size(time_vect), benchmark::Counter::kIsIterationInvariantRate);
}

BENCHMARK_CAPTURE(BM_ns_from_1970_to_tt2000, recent_scalar,
    static_cast<void (*)(const std::span<const int64_t>&, cdf::tt2000_t* const)>(
        cdf::chrono::_impl::scalar_from_ns_from_1970),
    1'546'300'800'000'000'000, 1'577'836'800'000'000'000)
    ->RangeMultiplier(32)
    ->Range(1024, mega(64))
    ->UseRealTime();

BENCHMARK_CAPTURE(BM_ns_from_1970_to_tt2000, recent_vectorized,
    static_cast<void (*)(const std::span<const int64_t>&, cdf::tt2000_t* const)>(
        vectorized_from_ns_from_1970),
    1'546'300'800'000'000'000, 1'577'836'800'000'000'000)
    ->RangeMultiplier(32)
    ->Range(1024, mega(64))
    ->UseRealTime();

BENCHMARK_CAPTURE(BM_ns_from_1970_to_tt2000, 1972_2036_scalar,
    static_cast<void (*)(const std::span<const int64_t>&, cdf::tt2000_t* const)>(
        cdf::chrono::_impl::scalar_from_ns_from_1970),
    63'072'000'000'000'000, 2'082'758'400'000'000'000)
    ->RangeMultiplier(32)
    ->Range(1024, mega(64))
    ->UseRealTime();

BENCHMARK_CAPTURE(BM_ns_from_1970_to_tt2000, 1972_2036_vectorized,
    static_cast<void (*)(const std::span<const int64_t>&, cdf::tt2000_t* const)>(
        vectorized_from_ns_from_1970),
    63'072'000'000'000'000, 2'082'758'400'000'000'000)
    ->RangeMultiplier(32)
    ->Range(1024, mega(64))
    ->UseRealTime();

BENCHMARK_MAIN();
