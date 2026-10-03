// WebAssembly SIMD time conversions against the scalar ones, bit for bit, as tests/chrono does
// for x86 and ARM: every start offset (alignment), short lengths (the scalar tail), special and
// random values. Built for WebAssembly with -msimd128 only, run with node.
#include "cdfpp/chrono/cdf-chrono.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

using namespace cdf;
constexpr int64_t nat = std::numeric_limits<int64_t>::min();
int failures = 0;

template <typename in_t, typename out_t, typename S, typename V>
void compare(const char* name, const std::vector<in_t>& values, S scalar, V vectorized)
{
    std::vector<out_t> expected(values.size());
    scalar(std::span<const in_t> { values }, expected.data());
    for (std::size_t start = 0; start < 8; ++start)
    {
        std::span<const in_t> input { values.data() + start, values.size() - start };
        std::vector<out_t> out(input.size());
        vectorized(input, out.data());
        if (std::memcmp(out.data(), expected.data() + start, out.size() * sizeof(out_t)) != 0)
            ++failures, std::printf("FAIL %s start %zu\n", name, start);
    }
    for (std::size_t length = 1; length < 40; ++length)
    {
        std::vector<out_t> out(length);
        vectorized(std::span<const in_t> { values.data(), length }, out.data());
        if (std::memcmp(out.data(), expected.data(), length * sizeof(out_t)) != 0)
            ++failures, std::printf("FAIL %s length %zu\n", name, length);
    }
    std::printf("checked %s: %zu values\n", name, values.size());
}

int main()
{
    std::mt19937_64 random { 42 };
    const double emin = 52943847163145.2265625, emax = 71390591236854.765625;
    std::vector<epoch> epochs;
    for (double ms : { 0.0, -1e31, 1e31, std::nan(""), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), emin, emax,
             std::nextafter(emin, 0.0), std::nextafter(emax, std::numeric_limits<double>::infinity()),
             constants::epoch_offset_miliseconds, 63745056000068.5, 62040988800123.25 })
        epochs.push_back({ ms });
    std::uniform_real_distribution<double> anywhere_ms { 0.5 * emin, 1.5 * emax };
    for (int i = 0; i < 100'000; ++i)
        epochs.push_back({ anywhere_ms(random) });

    const double off = constants::epoch_offset_seconds;
    std::vector<epoch16> epoch16s { { -1e31, -1e31 }, { 0.0, 0.0 }, { std::nan(""), 0.0 },
        { 63745056000.0, std::nan("") }, { 63745056000.0, -1.0 }, { 63745056000.0, 1e12 },
        { off - 9223372036.0, 0.0 }, { off + 9223372035.0, 999999999999.0 },
        { off - 9223372037.0, 999999999999.0 }, { off + 9223372036.0, 0.0 } };
    std::uniform_int_distribution<int64_t> secs { -10'000'000'000, 10'000'000'000 };
    std::uniform_int_distribution<int64_t> ps { 0, 999'999'999'999 };
    for (int i = 0; i < 100'000; ++i)
        epoch16s.push_back({ off + double(secs(random)), double(ps(random)) });

    std::vector<tt2000_t> tt2000s;
    std::uniform_int_distribution<int64_t> recent { 631108869184000000,
        std::numeric_limits<int64_t>::max() };
    for (int i = 0; i < 1000; ++i)
        tt2000s.push_back({ recent(random) });
    std::sort(tt2000s.begin(), tt2000s.end(), [](auto a, auto b) { return a.nseconds < b.nseconds; });
    for (int64_t ns : { nat, nat + 1, nat + 2, nat + 3, int64_t { 0 },
             std::numeric_limits<int64_t>::max() })
        tt2000s.push_back({ ns });
    std::uniform_int_distribution<int64_t> any64 { nat, std::numeric_limits<int64_t>::max() };
    for (int i = 0; i < 100'000; ++i)
        tt2000s.push_back({ any64(random) });

    std::vector<int64_t> datetimes;
    const int64_t last_leap = chrono::leap_seconds::leap_seconds_tt2000.back().first;
    std::uniform_int_distribution<int64_t> after { last_leap, std::numeric_limits<int64_t>::max() };
    for (int i = 0; i < 1000; ++i)
        datetimes.push_back(after(random));
    std::sort(datetimes.begin(), datetimes.end());
    for (const auto& [threshold, _] : chrono::leap_seconds::leap_seconds_tt2000)
        for (int64_t d : { int64_t { -1'000'000'000 }, int64_t { -1 }, int64_t { 0 }, int64_t { 1 } })
            datetimes.push_back(threshold + d);
    for (int64_t ns : { nat, nat + 1, nat + constants::tt2000_offset, int64_t { -1 }, int64_t { 0 } })
        datetimes.push_back(ns);
    for (int i = 0; i < 100'000; ++i)
        datetimes.push_back(any64(random));

    compare<epoch, int64_t>("CDF_EPOCH", epochs,
        [](auto in, auto* out) { cdf::_impl::scalar_to_ns_from_1970(in, out); },
        [](auto in, auto* out) { vectorized_to_ns_from_1970(in, out); });
    compare<epoch16, int64_t>("CDF_EPOCH16", epoch16s,
        [](auto in, auto* out) { cdf::_impl::scalar_to_ns_from_1970(in, out); },
        [](auto in, auto* out) { vectorized_to_ns_from_1970(in, out); });
    compare<tt2000_t, int64_t>("TT2000", tt2000s,
        [](auto in, auto* out) { cdf::_impl::scalar_to_ns_from_1970(in, out); },
        [](auto in, auto* out) { vectorized_to_ns_from_1970(in, out); });
    compare<int64_t, tt2000_t>("datetime64 to TT2000", datetimes,
        [](auto in, auto* out) { cdf::_impl::scalar_from_ns_from_1970(in, out); },
        [](auto in, auto* out) { vectorized_from_ns_from_1970(in, out); });
    std::printf(failures ? "FAILED: %d\n" : "all match\n", failures);
    return failures != 0;
}
