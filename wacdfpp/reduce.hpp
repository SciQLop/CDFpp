#pragma once
// Reduces a record-varying array to one row per record, for the Explorer's plots of
// high-rank variables (e.g. particle distributions). Pure C++, unit-tested natively
// (tests/explorer_reduce); wacdfpp.cpp binds it. Needs xsimd: WebAssembly builds always
// have it (-msimd128).

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <xsimd/xsimd.hpp>

namespace wacdfpp
{

// One mode per non-record dimension. A mode >= 0 keeps only that index (a slice).
enum reduce_mode : int32_t
{
    keep = -1,
    collapse = -2,
};

// How the collapsed dimensions are combined.
enum class reduce_op
{
    sum,
    mean,
};

// NaN means "not set": every comparison with NaN is false, so it never rejects a value.
struct value_mask
{
    double fill;
    double valid_min;
    double valid_max;
};

namespace detail
{
    inline bool is_valid(double x, const value_mask& mask)
    {
        return std::isfinite(x) & (x != mask.fill) & !(x < mask.valid_min) & !(x > mask.valid_max);
    }

    // A float32 FILLVAL of -1e31 reads back as -9.99999985e30: compare at the values' precision.
    template <typename T>
    value_mask at_precision_of(value_mask mask)
    {
        if constexpr (std::is_floating_point_v<T>)
            mask.fill = static_cast<double>(static_cast<T>(mask.fill));
        return mask;
    }

    inline void check_modes(std::span<const std::size_t> shape, std::span<const int32_t> modes)
    {
        if (std::size(shape) == 0 || std::size(modes) != std::size(shape) - 1)
            throw std::invalid_argument { "one reduction mode per non-record dimension expected" };
        for (std::size_t d = 0; d < std::size(modes); ++d)
        {
            if (modes[d] < collapse)
                throw std::invalid_argument { "unknown reduction mode" };
            if (modes[d] >= 0 && static_cast<std::size_t>(modes[d]) >= shape[d + 1])
                throw std::out_of_range { "slice index out of range" };
        }
    }

    // Row-major strides of `shape`, where dimensions that are not kept get stride 0: the
    // output offset of an input element is then the dot product of its index with them.
    inline std::vector<std::size_t> output_strides(
        std::span<const std::size_t> shape, std::span<const int32_t> modes)
    {
        std::vector<std::size_t> strides(std::size(shape), 0);
        std::size_t stride = 1;
        for (std::size_t d = std::size(shape); d-- > 0;)
        {
            if (d == 0 || modes[d - 1] == keep)
            {
                strides[d] = stride;
                stride *= shape[d];
            }
        }
        return strides;
    }

    inline std::vector<std::size_t> input_strides(std::span<const std::size_t> shape)
    {
        std::vector<std::size_t> strides(std::size(shape), 1);
        for (std::size_t d = std::size(shape) - 1; d-- > 0;)
            strides[d] = strides[d + 1] * shape[d + 1];
        return strides;
    }

    // The compiler may not reorder one long floating-point sum into SIMD lanes, so it is
    // written with xsimd: one partial sum per lane.
    template <typename T>
    std::pair<double, std::size_t> masked_sum(
        const T* __restrict src, std::size_t n, const value_mask& mask)
    {
        using batch = xsimd::batch<double>;
        constexpr auto lanes = batch::size;
        const batch fill(mask.fill), valid_min(mask.valid_min), valid_max(mask.valid_max);
        const batch zero(0.), one(1.);
        batch sums(0.), counts(0.);
        std::size_t i = 0;
        for (; i + lanes <= n; i += lanes)
        {
            std::array<double, lanes> widened;
            for (std::size_t l = 0; l < lanes; ++l)
                widened[l] = static_cast<double>(src[i + l]);
            const auto x = batch::load_unaligned(widened.data());
            const auto ok = xsimd::isfinite(x) & (x != fill) & ~(x < valid_min) & ~(x > valid_max);
            sums += xsimd::select(ok, x, zero);
            counts += xsimd::select(ok, one, zero);
        }
        double sum = xsimd::reduce_add(sums);
        auto count = static_cast<std::size_t>(xsimd::reduce_add(counts));
        for (; i < n; ++i)
        {
            const auto x = static_cast<double>(src[i]);
            const bool ok = is_valid(x, mask);
            sum += ok ? x : 0.;
            count += ok;
        }
        return { sum, count };
    }

    struct accumulator
    {
        std::vector<double> sums;
        std::vector<std::size_t> counts;
    };

    template <typename T>
    struct walker
    {
        std::span<const T> values;
        std::span<const std::size_t> shape;
        std::span<const int32_t> modes;
        std::vector<std::size_t> in_strides;
        std::vector<std::size_t> out_strides;
        value_mask mask;
        accumulator& acc;

        // The last dimension is contiguous: a kept one adds element-wise into the output row,
        // a collapsed one is a single masked sum. Branchless so that it vectorizes.
        void run(std::size_t in, std::size_t out, std::size_t n, bool kept)
        {
            if (kept)
            {
                const T* __restrict src = values.data() + in;
                double* __restrict sums = acc.sums.data() + out;
                std::size_t* __restrict counts = acc.counts.data() + out;
                for (std::size_t i = 0; i < n; ++i)
                {
                    const auto x = static_cast<double>(src[i]);
                    const bool ok = is_valid(x, mask);
                    sums[i] += ok ? x : 0.;
                    counts[i] += ok;
                }
                return;
            }
            const auto [sum, count] = masked_sum(values.data() + in, n, mask);
            acc.sums[out] += sum;
            acc.counts[out] += count;
        }

        // Dimension 0 is the record dimension, always kept.
        void walk(std::size_t d, std::size_t in, std::size_t out)
        {
            const auto mode = d == 0 ? keep : modes[d - 1];
            const auto first = mode >= 0 ? static_cast<std::size_t>(mode) : std::size_t { 0 };
            const auto last = mode >= 0 ? first + 1 : shape[d];
            if (d + 1 == std::size(shape))
                return run(in + first, out + first * out_strides[d], last - first, out_strides[d] != 0);
            for (std::size_t i = first; i < last; ++i)
                walk(d + 1, in + i * in_strides[d], out + i * out_strides[d]);
        }
    };

    inline std::vector<double> finish(accumulator&& acc, bool averaged)
    {
        auto out = std::move(acc.sums);
        for (std::size_t i = 0; i < std::size(out); ++i)
        {
            if (acc.counts[i] == 0)
                out[i] = std::nan("");
            else if (averaged)
                out[i] /= static_cast<double>(acc.counts[i]);
        }
        return out;
    }
} // namespace detail

// `values` is row-major with `shape` = (records, d1, d2, ...). Returns a row-major
// (records, kept dims...) array of doubles. Invalid values (non-finite, FILLVAL, out of
// VALIDMIN/VALIDMAX) are left out; a cell with no valid value is NaN.
template <typename T>
std::vector<double> reduce(std::span<const T> values, std::span<const std::size_t> shape,
    std::span<const int32_t> modes, reduce_op op, value_mask mask)
{
    detail::check_modes(shape, modes);
    const auto out_strides = detail::output_strides(shape, modes);
    const auto out_size = out_strides[0] * shape[0];
    detail::accumulator acc { std::vector<double>(out_size, 0.), std::vector<std::size_t>(out_size, 0) };
    if (shape[0] != 0)
        detail::walker<T> { values, shape, modes, detail::input_strides(shape), out_strides,
            detail::at_precision_of<T>(mask), acc }
            .walk(0, 0, 0);
    return detail::finish(std::move(acc), op == reduce_op::mean);
}

} // namespace wacdfpp
