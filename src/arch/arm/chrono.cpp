#include <arm_neon.h>
#include <cdfpp/chrono/cdf-chrono-impl.hpp>
#include <cdfpp/vectorized/cdf-chrono.hpp>

// NEON time conversions. Every aarch64 CPU has NEON: one build, no run-time dispatch, and the
// scalar fallback shares this file's flags. NEON registers hold two 64-bit lanes and their
// operations take 2 cycles or more on Apple's cores: each loop step works on several
// independent registers, so that no step waits for the previous one.

namespace
{
using namespace cdf;
using namespace cdf::chrono;

// Times between two leap seconds, from first to last inclusive: they convert with one addition.
struct interval
{
    int64_t first;
    int64_t last;
    int64_t offset;
};

// TT2000 to ns since 1970, from 1972 on. Before, TAI-UTC isn't a whole number of seconds.
constexpr auto to_ns_intervals = []()
{
    const auto& table = leap_seconds::leap_seconds_tt2000_reverse;
    std::array<interval, std::size(table)> result { };
    for (std::size_t i = 0; i < std::size(table); ++i)
        result[i] = { table[i].first,
            i + 1 < std::size(table) ? table[i + 1].first - 1 : _impl::last_representable_tt2000,
            constants::tt2000_offset - table[i].second };
    return result;
}();

// ns since 1970 to TT2000, from 1972 on.
constexpr auto from_ns_intervals = []()
{
    const auto& table = leap_seconds::leap_seconds_tt2000;
    std::array<interval, std::size(table)> result { };
    for (std::size_t i = 0; i < std::size(table); ++i)
        result[i] = { table[i].first,
            i + 1 < std::size(table) ? table[i + 1].first - 1 : std::numeric_limits<int64_t>::max(),
            table[i].second - constants::tt2000_offset };
    return result;
}();

// The interval holding value, or the closest one: the caller checks.
template <std::size_t N>
const interval& find_interval(const std::array<interval, N>& intervals, int64_t value)
{
    const auto after = std::upper_bound(std::cbegin(intervals), std::cend(intervals), value,
        [](int64_t v, const interval& i) { return v < i.first; });
    return after == std::cbegin(intervals) ? intervals.front() : *(after - 1);
}

// first <= value <= last, as one unsigned comparison of value - first. In the last interval of
// TT2000 to ns, value >= first alone: later values overflow and wrap below first, as for x86.
// Not from ns to TT2000: its offset is negative, and NaT would wrap above first.
template <bool last_interval>
inline uint64x2_t in_interval(int64x2_t value, int64x2_t first, uint64x2_t span)
{
    if constexpr (last_interval)
        return vcgeq_s64(value, first);
    else
        return vcleq_u64(vreinterpretq_u64_s64(vsubq_s64(value, first)), span);
}

// Converts 32 values with one addition each, tested on the result. True if they were all in
// the interval. Independent registers, and one reduction for the 32 values.
template <bool last_interval>
inline bool convert_block(const int64_t* const input, int64_t* const output, int64x2_t offset,
    int64x2_t first, uint64x2_t span)
{
    auto all_in = vdupq_n_u64(~0ULL);
    for (std::size_t i = 0; i < 32; i += 8)
    {
        // Plain loads, that the compiler pairs (ldp): faster than a 4-register ld1 on Apple
        // cores. But one 4-register store: it writes a whole 64-byte cache line at once, and
        // big arrays convert 1.5 times faster than with pairs of stores.
        const int64x2x4_t converted { vaddq_s64(vld1q_s64(input + i), offset),
            vaddq_s64(vld1q_s64(input + i + 2), offset),
            vaddq_s64(vld1q_s64(input + i + 4), offset),
            vaddq_s64(vld1q_s64(input + i + 6), offset) };
        vst1q_s64_x4(output + i, converted);
        all_in = vandq_u64(all_in,
            vandq_u64(vandq_u64(in_interval<last_interval>(converted.val[0], first, span),
                          in_interval<last_interval>(converted.val[1], first, span)),
                vandq_u64(in_interval<last_interval>(converted.val[2], first, span),
                    in_interval<last_interval>(converted.val[3], first, span))));
    }
    return vminvq_u32(vreinterpretq_u32_u64(all_in)) == 0xFFFFFFFFU;
}

// Sorted data stays in one interval for long: 32 values at a time, one addition each, while they
// do. A block that crosses a leap second, or holds special values, goes to the scalar code, and
// the next blocks use the interval of its last value. Shuffled data across leap seconds is
// mostly scalar this way, and real time axes are rarely shuffled.
template <std::size_t N>
void convert_by_interval(const int64_t* const input, const std::size_t count, int64_t* const output,
    const std::array<interval, N>& intervals, const auto& scalar)
{
    constexpr std::size_t block = 32;
    std::size_t i = 0;
    while (i + block <= count)
    {
        const auto& current = find_interval(intervals, input[i]);
        const bool last_interval = &current == &intervals.back() && current.offset > 0;
        // Bounds of the converted values: the test runs on the result.
        const auto offset = vdupq_n_s64(current.offset);
        const auto first = vdupq_n_s64(current.first + current.offset);
        const auto span = vdupq_n_u64(static_cast<uint64_t>(current.last - current.first));
        for (; i + block <= count; i += block)
        {
            const bool all_in = last_interval
                ? convert_block<true>(input + i, output + i, offset, first, span)
                : convert_block<false>(input + i, output + i, offset, first, span);
            if (!all_in) [[unlikely]]
            {
                scalar(i, block);
                i += block;
                break;
            }
        }
    }
    if (i < count)
        scalar(i, count - i);
}

} // namespace

void vectorized_to_ns_from_1970(const std::span<const cdf::tt2000_t>& input, int64_t* const output)
{
    convert_by_interval(reinterpret_cast<const int64_t*>(input.data()), std::size(input), output,
        to_ns_intervals,
        [&](std::size_t start, std::size_t size)
        {
            cdf::chrono::_impl::scalar_to_ns_from_1970(input.subspan(start, size), output + start);
        });
}

void vectorized_from_ns_from_1970(
    const std::span<const int64_t>& input, cdf::tt2000_t* const output)
{
    convert_by_interval(input.data(), std::size(input), reinterpret_cast<int64_t*>(output),
        from_ns_intervals,
        [&](std::size_t start, std::size_t size)
        {
            cdf::chrono::_impl::scalar_from_ns_from_1970(
                input.subspan(start, size), output + start);
        });
}

// floor(x * 1e6) exactly, without the 2^13 split the x86 code needs. p = x * 1e6 rounded, and
// e = p - x * 1e6 computed with one rounding (FMA) is exact. Below 2^52, the product (a multiple
// of 0.5) is exact, e = 0 and the result is floor(p). Above, p is a whole number and the result
// is p - ceil(e). AArch64 converts with either rounding in one instruction: floor(p) - ceil(e)
// covers both.
static inline int64x2_t exact_floor_scaled(float64x2_t x, float64x2_t scale)
{
    const auto p = vmulq_f64(x, scale);
    const auto e = vfmsq_f64(p, x, scale);
    return vsubq_s64(vcvtmq_s64_f64(p), vcvtpq_s64_f64(e));
}

void vectorized_to_ns_from_1970(const std::span<const cdf::epoch>& input, int64_t* const output)
{
    using namespace cdf::chrono;
    const auto* const ms = reinterpret_cast<const double*>(input.data());
    const auto count = std::size(input);
    const auto offset = vdupq_n_f64(constants::epoch_offset_miliseconds);
    const auto ns_in_ms = vdupq_n_f64(1e6);
    const auto min_ms = vdupq_n_f64(_impl::epoch_min_ms);
    const auto max_ms = vdupq_n_f64(_impl::epoch_max_ms);
    const auto nat = vdupq_n_s64(_impl::nat);
    std::size_t i = 0;
    for (; i + 8 <= count; i += 8)
    {
        int64x2x4_t result;
        for (std::size_t r = 0; r < 4; ++r)
        {
            const auto values = vld1q_f64(ms + i + 2 * r);
            // ms - offset is exact: both are within a factor 2 in range. NaN, fill and pad fail
            // the range test, and their lanes, converted with saturation, are replaced.
            const auto ns = exact_floor_scaled(vsubq_f64(values, offset), ns_in_ms);
            const auto in_range = vandq_u64(vcgeq_f64(values, min_ms), vcleq_f64(values, max_ms));
            result.val[r] = vbslq_s64(in_range, ns, nat);
        }
        // A whole 64-byte cache line at once, as in convert_block.
        vst1q_s64_x4(output + i, result);
    }
    if (i < count)
        _impl::scalar_to_ns_from_1970(input.subspan(i), output + i);
}

void vectorized_to_ns_from_1970(const std::span<const cdf::epoch16>& input, int64_t* const output)
{
    using namespace cdf::chrono;
    const auto* const values = reinterpret_cast<const double*>(input.data());
    const auto count = std::size(input);
    const auto offset = vdupq_n_f64(constants::epoch_offset_seconds);
    const auto ns_in_s = vdupq_n_f64(1e9);
    const auto ps_in_ns = vdupq_n_f64(1e3);
    const auto min_s = vdupq_n_f64(_impl::epoch16_min_s);
    const auto max_s = vdupq_n_f64(_impl::epoch16_max_s);
    const auto zero = vdupq_n_f64(0.);
    const auto ps_in_s = vdupq_n_f64(1e12);
    const auto nat = vdupq_n_s64(_impl::nat);
    std::size_t i = 0;
    for (; i + 8 <= count; i += 8)
    {
        int64x2x4_t result;
        for (std::size_t r = 0; r < 4; ++r)
        {
            // Seconds in val[0], picoseconds in val[1]: ld2 splits the pairs as it loads them.
            const auto pair = vld2q_f64(values + 2 * (i + 2 * r));
            const auto& seconds = pair.val[0];
            const auto& picos = pair.val[1];
            // As _impl::epoch16_to_ns_from_1970: whole seconds and ns truncated toward zero.
            // Whole seconds times 1e9 is a whole number: floor is exact.
            const auto whole_ns
                = exact_floor_scaled(vrndq_f64(vsubq_f64(seconds, offset)), ns_in_s);
            const auto sub_second_ns = vcvtq_s64_f64(vdivq_f64(picos, ps_in_ns));
            const auto in_range
                = vandq_u64(vandq_u64(vcgeq_f64(seconds, min_s), vcleq_f64(seconds, max_s)),
                    vandq_u64(vcgeq_f64(picos, zero), vcltq_f64(picos, ps_in_s)));
            result.val[r] = vbslq_s64(in_range, vaddq_s64(whole_ns, sub_second_ns), nat);
        }
        vst1q_s64_x4(output + i, result);
    }
    if (i < count)
        _impl::scalar_to_ns_from_1970(input.subspan(i), output + i);
}
