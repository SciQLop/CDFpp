#include <benchmark/benchmark.h>
#include <cdfpp/cdf-io/rle.hpp>
#include <cdfpp/no_init_vector.hpp>
#include <cstring>
#include <random>

no_init_vector<char> make_rle_friendly_data(std::size_t size, double zero_fraction)
{
    no_init_vector<char> data(size);
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::uniform_int_distribution<int> byte_dist(1, 127);
    for (auto& b : data)
        b = (dist(rng) < zero_fraction) ? 0 : static_cast<char>(byte_dist(rng));
    return data;
}

static void BM_rle_deflate(benchmark::State& state)
{
    auto data = make_rle_friendly_data(state.range(0), 0.5);
    for (auto _ : state)
    {
        auto result = cdf::io::rle::deflate(data);
        benchmark::DoNotOptimize(result);
    }
    state.counters["bytes_per_second"]
        = benchmark::Counter(state.range(0), benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_rle_deflate)->RangeMultiplier(4)->Range(1024, 1024 * 1024)->Complexity();

static void BM_rle_inflate(benchmark::State& state)
{
    auto data = make_rle_friendly_data(state.range(0), 0.5);
    auto compressed = cdf::io::rle::deflate(data);
    no_init_vector<char> output(data.size());
    for (auto _ : state)
    {
        cdf::io::rle::inflate(compressed, output.data(), output.size());
        benchmark::DoNotOptimize(output);
    }
    state.counters["bytes_per_second"]
        = benchmark::Counter(state.range(0), benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_rle_inflate)->RangeMultiplier(4)->Range(1024, 1024 * 1024)->Complexity();

static void BM_rle_roundtrip(benchmark::State& state)
{
    auto data = make_rle_friendly_data(state.range(0), 0.5);
    no_init_vector<char> output(data.size());
    for (auto _ : state)
    {
        auto compressed = cdf::io::rle::deflate(data);
        cdf::io::rle::inflate(compressed, output.data(), output.size());
        benchmark::DoNotOptimize(output);
    }
    state.counters["bytes_per_second"]
        = benchmark::Counter(state.range(0), benchmark::Counter::kIsIterationInvariantRate);
}
BENCHMARK(BM_rle_roundtrip)->RangeMultiplier(4)->Range(1024, 1024 * 1024)->Complexity();

BENCHMARK_MAIN();
