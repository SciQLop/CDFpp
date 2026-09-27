/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2025, Laboratory of Plasma Physics- CNRS
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
#include <limits>
#include "cdfpp/cdf-enums.hpp"
#include "cdfpp/cdf-helpers.hpp"
#include "cdfpp/chrono/cdf-chrono-constants.hpp"
#include "cdfpp/chrono/cdf-chrono-impl.hpp"
#include "cdfpp/chrono/cdf-leap-seconds.h"
#include <array>
#include <xsimd/xsimd.hpp>


namespace cdf::chrono::vectorized
{
using namespace cdf::chrono;

// Defined in src/arch/x86/chrono.cpp, built without SIMD flags. Calling the inline
// _impl::scalar_to_ns_from_1970 here would emit it in every per-arch object, and the linker
// could keep the AVX-512 copy for everyone: SIGILL on CPUs without AVX-512.
void scalar_to_ns_from_1970(const std::span<const tt2000_t>& input, int64_t* const output);
void scalar_to_ns_from_1970(const std::span<const epoch>& input, int64_t* const output);
void scalar_to_ns_from_1970(const std::span<const epoch16>& input, int64_t* const output);

template <class Arch, typename align_mode>
void stream_store(const auto& batch_data, auto* const output)
{
    if constexpr (std::is_same_v<align_mode, xsimd::unaligned_mode>)
    {
        return batch_data.store_unaligned(output);
    }
    if constexpr (std::is_base_of_v<xsimd::avx512f, Arch>)
    {
#if defined(__AVX512F__)
        _mm512_stream_si512(reinterpret_cast<__m512i*>(output), batch_data);
#endif
    }
    else if constexpr (std::is_base_of_v<xsimd::avx2, Arch>)
    {
#if defined(__AVX2__)
        _mm256_stream_si256(reinterpret_cast<__m256i*>(output), batch_data);
#endif
    }
    else
        xsimd::store_aligned(output, batch_data);
}

template <class Arch, typename align_mode>
void store(const auto& batch_data, auto* output)
{
    if constexpr (std::is_same_v<align_mode, xsimd::unaligned_mode>)
        batch_data.store_unaligned(output);
    else
        batch_data.store_aligned(output);
}

template <class Arch>
void sfence()
{
    if constexpr (std::is_same_v<Arch, xsimd::avx512f> || std::is_same_v<Arch, xsimd::avx2>)
    {
#if defined(__AVX__) || defined(__AVX512F__)
        _mm_sfence();
#endif
    }
}


template <class Arch, typename T>
static inline std::tuple<std::span<const T>,int64_t*>  _realign(Arch, const std::span<const T>& input,
    int64_t* const output, const auto& scalar_function)
{
    const auto count = std::size(input);
    const auto input_offset = (reinterpret_cast<uint64_t>(input.data()) / sizeof(tt2000_t))
        % (Arch::alignment() / sizeof(tt2000_t));
    const auto output_offset = (reinterpret_cast<uint64_t>(output) / sizeof(tt2000_t))
        % (Arch::alignment() / sizeof(int64_t));
    if ((input_offset == output_offset) && (input_offset < count))
    {
        const auto elements_to_align = (Arch::alignment() / sizeof(tt2000_t)) - input_offset;
        scalar_function(input.subspan(0, elements_to_align), output);
        return {input.subspan(elements_to_align), output + elements_to_align};
    }
    return {input, output};
}

template <typename Arch, std::size_t offset, std::size_t... I>
constexpr auto _make_indexes(std::index_sequence<I...>)
{
    return xsimd::batch<uint64_t, Arch>((2 * I + offset)...);
}

template <typename Arch, typename Is = std::make_index_sequence<xsimd::batch<uint64_t, Arch>::size>>
constexpr auto make_even_indexes()
{
    return _make_indexes<Arch, 0>(Is {});
}

template <typename Arch, typename Is = std::make_index_sequence<xsimd::batch<uint64_t, Arch>::size>>
constexpr auto make_odd_indexes()
{
    return _make_indexes<Arch, 1>(Is {});
}


// Integral doubles below 2^51 to int64: adding 1.5 * 2^52 lays the integer in the mantissa bits.
// x86 has a double to int64 conversion only since AVX-512DQ, and this is cheaper anyway.
template <class Arch>
inline xsimd::batch<int64_t, Arch> small_integers_to_int64(const xsimd::batch<double, Arch>& value)
{
    const auto magic = xsimd::broadcast<double, Arch>(0x1.8p52);
    return xsimd::bitwise_cast<int64_t>(value + magic) - xsimd::bitwise_cast<int64_t>(magic);
}

struct _to_ns_from_1970_epoch16_t
{
    template <class Arch, typename input_align_mode, typename output_align_mode>
    static inline void to_ns_from_1970(
        Arch, const std::span<const epoch16>& input, int64_t* const output)
    {
        using batchout_type = xsimd::batch<int64_t, Arch>;
        using batchin_type = xsimd::batch<double, Arch>;
        constexpr std::size_t simd_size = batchout_type::size;
        const auto count = std::size(input);
        const auto even_indexes = make_even_indexes<Arch>();
        const auto odd_indexes = make_odd_indexes<Arch>();

        const auto offset = xsimd::broadcast<double, Arch>(constants::epoch_offset_seconds);
        const auto min_s = xsimd::broadcast<double, Arch>(_impl::epoch16_min_s);
        const auto max_s = xsimd::broadcast<double, Arch>(_impl::epoch16_max_s);
        const auto zero = xsimd::broadcast<double, Arch>(0.0);
        const auto ps_in_s = xsimd::broadcast<double, Arch>(1e12);
        const auto ps_in_ns = xsimd::broadcast<double, Arch>(1'000);
        const auto ns_in_s = xsimd::broadcast<int64_t, Arch>(1'000'000'000);
        const auto nat = xsimd::broadcast<int64_t, Arch>(_impl::nat);
        std::size_t i = 0;
        for (; i + simd_size <= count; i += simd_size)
        {
            // The steps of _impl::epoch16_to_ns_from_1970.
            auto seconds
                = batchin_type::gather(reinterpret_cast<const double*>(&input[i]), even_indexes);
            auto picos
                = batchin_type::gather(reinterpret_cast<const double*>(&input[i]), odd_indexes);
            // In range, whole seconds are below 2^34 and sub-second ns below 1e9.
            auto whole_seconds = small_integers_to_int64<Arch>(xsimd::trunc(seconds - offset));
            auto sub_second_ns = small_integers_to_int64<Arch>(xsimd::trunc(picos / ps_in_ns));
            const auto in_range = xsimd::batch_bool_cast<int64_t>((seconds >= min_s)
                & (seconds <= max_s) & (picos >= zero) & (picos < ps_in_s));
            xsimd::select(in_range, whole_seconds * ns_in_s + sub_second_ns, nat)
                .store(output + i, output_align_mode {});
        }
        if (i < count)
        {
            vectorized::scalar_to_ns_from_1970(input.subspan(i), output + i);
        }
    }

    template <class Arch>
    void operator()(
        Arch, const std::span<const epoch16>& input, int64_t* const output);
};

template <class Arch>
void _to_ns_from_1970_epoch16_t::operator()(
    Arch, const std::span<const epoch16>& input, int64_t* const output)
{
    if constexpr (cdf::helpers::is_any_of_v<Arch, xsimd::unavailable, xsimd::sse2>)
    {
        return vectorized::scalar_to_ns_from_1970(input, output);
    }
    if (xsimd::is_aligned(input.data()) && xsimd::is_aligned(output))
    {
        to_ns_from_1970<Arch, xsimd::aligned_mode, xsimd::aligned_mode>(
            Arch {}, input, output);
    }
    else if (xsimd::is_aligned(input.data()))
    {
        to_ns_from_1970<Arch, xsimd::aligned_mode, xsimd::unaligned_mode>(
            Arch {}, input, output);
    }
    else if (xsimd::is_aligned(output))
    {
        to_ns_from_1970<Arch, xsimd::unaligned_mode, xsimd::aligned_mode>(
            Arch {}, input, output);
    }
    else
    {
        to_ns_from_1970<Arch, xsimd::unaligned_mode, xsimd::unaligned_mode>(
            Arch {}, input, output);
    }
}

struct _to_ns_from_1970_epoch_t
{

    template <class Arch, typename input_align_mode, typename output_align_mode>
    static inline void to_ns_from_1970(
        Arch, const std::span<const epoch>& input, int64_t* const output)
    {
        using batchout_type = xsimd::batch<int64_t, Arch>;
        using batchin_type = xsimd::batch<double, Arch>;
        const auto count = std::size(input);
        constexpr std::size_t simd_size = batchout_type::size;
        const auto min_ms = xsimd::broadcast<double, Arch>(_impl::epoch_min_ms);
        const auto max_ms = xsimd::broadcast<double, Arch>(_impl::epoch_max_ms);
        const auto offset = xsimd::broadcast<double, Arch>(constants::epoch_offset_miliseconds);
        const auto ns_in_ms = xsimd::broadcast<double, Arch>(1e6);
        const auto block = xsimd::broadcast<double, Arch>(0x1p13);
        const auto per_block = xsimd::broadcast<double, Arch>(0x1p-13);
        const auto nat = xsimd::broadcast<int64_t, Arch>(_impl::nat);
        std::size_t i = 0;
        for (; i + simd_size <= count; i += simd_size)
        {
            // floor((ms - offset) * 1e6) like _impl::epoch_to_ns_from_1970, with no int64
            // multiply or general double to int64 conversion (AVX2 has neither). In range:
            // - ms - offset is exact (two doubles within a factor 2) and below 2^44 ms;
            // - high * 2^13 + low splits it exactly, low in [0, 8192) on a 2^-7 grid;
            // - so high * 1e6 is an integer below 2^31 * 1e6 < 2^51 and low * 1e6 one below 2^33
            //   plus 0.5 at most: both convert exactly, and the result is
            //   (high * 1e6) << 13 + floor(low * 1e6).
            const auto ms = batchin_type::load(&input[i].mseconds, input_align_mode {});
            const auto since_1970 = ms - offset;
            const auto high = xsimd::floor(since_1970 * per_block);
            const auto low = since_1970 - high * block;
            const auto ns = (small_integers_to_int64<Arch>(high * ns_in_ms) << 13)
                + small_integers_to_int64<Arch>(xsimd::floor(low * ns_in_ms));
            const auto in_range = xsimd::batch_bool_cast<int64_t>((ms >= min_ms) & (ms <= max_ms));
            store<Arch, output_align_mode>(xsimd::select(in_range, ns, nat), output + i);
        }
        if (i < count)
        {
            vectorized::scalar_to_ns_from_1970(input.subspan(i), output + i);
        }
    }

    template <class Arch>
    void operator()(Arch, const std::span<const epoch>& input, int64_t* const output);
};

template <class Arch>
void _to_ns_from_1970_epoch_t::operator()(
    Arch, const std::span<const epoch>& input, int64_t* const output)
{
    if constexpr (cdf::helpers::is_any_of_v<Arch, xsimd::unavailable, xsimd::sse2>)
    {
        return vectorized::scalar_to_ns_from_1970(input, output);
    }
    if (xsimd::is_aligned(input.data()) && xsimd::is_aligned(output))
    {
        to_ns_from_1970<Arch, xsimd::aligned_mode, xsimd::aligned_mode>(
            Arch {}, input, output);
    }
    else if (xsimd::is_aligned(input.data()))
    {
        to_ns_from_1970<Arch, xsimd::aligned_mode, xsimd::unaligned_mode>(
            Arch {}, input, output);
    }
    else if (xsimd::is_aligned(output))
    {
        to_ns_from_1970<Arch, xsimd::unaligned_mode, xsimd::aligned_mode>(
            Arch {}, input, output);
    }
    else
    {
        to_ns_from_1970<Arch, xsimd::unaligned_mode, xsimd::unaligned_mode>(
            Arch {}, input, output);
    }
}

struct _to_ns_from_1970_tt2000_t
{

    template <class Arch, typename input_align_mode, typename output_align_mode>
    static inline void _optimistic_after_2017(
        Arch, const std::span<const tt2000_t>& input, int64_t* const output)
    {
        using batch_type = xsimd::batch<int64_t, Arch>;
        const auto count = std::size(input);
        constexpr std::size_t simd_size = batch_type::size;
        std::size_t i = 0;
        const auto offset = xsimd::broadcast<int64_t, Arch>(
            constants::tt2000_offset - leap_seconds::leap_seconds_tt2000_reverse.back().second);
        const auto last_leap_sec = xsimd::broadcast<int64_t, Arch>(
            leap_seconds::leap_seconds_tt2000_reverse.back().first);
        const auto last_representable
            = xsimd::broadcast<int64_t, Arch>(_impl::last_representable_tt2000);
        auto was_after_2017 = xsimd::batch_bool<int64_t, Arch>(true);
        for (; i + simd_size <= count; i += simd_size)
        {
            auto tt2000_batch = batch_type::load(&input[i].nseconds, input_align_mode {});
            store<Arch, output_align_mode>(tt2000_batch + offset, &output[i]);
            // After 2262 (out of int64 ns since 1970): the unsorted path writes NaT.
            was_after_2017 = was_after_2017 & (tt2000_batch >= last_leap_sec)
                & (tt2000_batch <= last_representable);
        }
        // sfence<Arch>();
        if (!xsimd::all(was_after_2017))
        {
            return _unsorted<Arch, input_align_mode, output_align_mode>(
                Arch {}, input, output);
        }
        if (i < count)
        {
            vectorized::scalar_to_ns_from_1970(input.subspan(i), &output[i]);
        }
    }

    template <class Arch, typename input_align_mode, typename output_align_mode>
    static inline void _unsorted(
        Arch, const std::span<const tt2000_t>& input, int64_t* const output)
    {
        using batch_type = xsimd::batch<int64_t, Arch>;
        constexpr std::size_t simd_size = batch_type::size;
        std::size_t i = 0;
        const auto count = std::size(input);
        constexpr auto max_leap_offset = leap_seconds::leap_seconds_tt2000_reverse.back().second;
        const auto last_leap_sec = xsimd::broadcast<int64_t, Arch>(
            leap_seconds::leap_seconds_tt2000_reverse.back().first);
        const auto one_sec = xsimd::broadcast<int64_t, Arch>(1000000000LL);


        const auto first_leap_sec = xsimd::broadcast<int64_t, Arch>(
            leap_seconds::leap_seconds_tt2000_reverse.front().first);
        const auto last_representable
            = xsimd::broadcast<int64_t, Arch>(_impl::last_representable_tt2000);
        const auto nat = xsimd::broadcast<int64_t, Arch>(_impl::nat);
        for (; i + simd_size <= count; i += simd_size)
        {
            auto tt2000_batch = batch_type::load(&input[i].nseconds, input_align_mode {});
            // Before 1972, TAI-UTC isn't a whole number of seconds: the scalar code handles it.
            if (xsimd::any(tt2000_batch < first_leap_sec))
            {
                vectorized::scalar_to_ns_from_1970(input.subspan(i, simd_size), &output[i]);
                continue;
            }
            auto offset
                = xsimd::broadcast<int64_t, Arch>(constants::tt2000_offset - max_leap_offset);
            int leap_index = std::size(leap_seconds::leap_seconds_tt2000_reverse) - 2;
            auto needs_correction = (tt2000_batch < last_leap_sec);
            // Fill and pad values are before 1972, so they went through the scalar code above.
            const auto representable = tt2000_batch <= last_representable;
            if (!xsimd::any(needs_correction))
            {
                store<Arch, output_align_mode>(
                    xsimd::select(representable, tt2000_batch + offset, nat), &output[i]);
                continue;
            }
            while (xsimd::any(needs_correction) && (leap_index != -1))
            {
                offset = xsimd::select(needs_correction, offset + one_sec, offset);
                needs_correction
                    = (tt2000_batch < xsimd::broadcast<int64_t, Arch>(
                           leap_seconds::leap_seconds_tt2000_reverse[leap_index].first));
                leap_index--;
            }
            store<Arch, output_align_mode>(
                xsimd::select(representable, tt2000_batch + offset, nat), &output[i]);
        }
        // sfence<Arch>();
        if (i < count)
        {
            vectorized::scalar_to_ns_from_1970(input.subspan(i), &output[i]);
        }
    }

    template <class Arch, typename input_align_mode, typename output_align_mode>
    static inline void to_ns_from_1970(
        Arch, const std::span<const tt2000_t>& input, int64_t* const output)
    {
        /* We asume that the input is almost always sorted and recent (after 2017)
         * so we first check if the first value is after the last leap second
         * if yes, we can use a faster algorithm (just a constant offset).
         * This optimistic still keeps track of the fact that some values may be before
         * the last leap second, in which case we fallback to the unsorted algorithm.
         */
        if (std::empty(input))
            return;
        if (input[0].nseconds >= leap_seconds::leap_seconds_tt2000_reverse.back().first)
        {
            return _optimistic_after_2017<Arch, input_align_mode, output_align_mode>(
                Arch {}, input, output);
        }
        return _unsorted<Arch, input_align_mode, output_align_mode>(Arch {}, input, output);
    }

    template <class Arch>
    void operator()(
        Arch, const std::span<const tt2000_t>& input, int64_t* const output);
};

template <class Arch>
void _to_ns_from_1970_tt2000_t::operator()(
    Arch, const std::span<const tt2000_t>& input, int64_t* const output)
{
    // fallback to scalar implementation where it's not worth vectorizing
    // in this particular case, we manipulate 64-bit integers and to avoid
    // branching in the vectorized code, we process more leap seconds than
    // the scalar code, we need at least 512 bits SIMD registers to make it
    // worth it.
    if constexpr (cdf::helpers::is_any_of_v<Arch, xsimd::unavailable, xsimd::sse2>)
    {
        return vectorized::scalar_to_ns_from_1970(input, output);
    }
    {
        if (xsimd::is_aligned(input.data()) && xsimd::is_aligned(output))
        {
            to_ns_from_1970<Arch, xsimd::aligned_mode, xsimd::aligned_mode>(
                Arch {}, input, output);
        }
        else if (xsimd::is_aligned(input.data()))
        {
            to_ns_from_1970<Arch, xsimd::aligned_mode, xsimd::unaligned_mode>(
                Arch {}, input, output);
        }
        else if (xsimd::is_aligned(output))
        {
            to_ns_from_1970<Arch, xsimd::unaligned_mode, xsimd::aligned_mode>(
                Arch {}, input, output);
        }
        else
        {
            auto [ new_in, new_out ] = _realign(Arch {}, input, output,
                static_cast<void (*)(const std::span<const tt2000_t>&, int64_t* const)>(
                    vectorized::scalar_to_ns_from_1970));
            if (new_out != output)
            {
                to_ns_from_1970<Arch, xsimd::aligned_mode, xsimd::aligned_mode>(
                    Arch {}, new_in, new_out);
            }
            else
            {
                to_ns_from_1970<Arch, xsimd::unaligned_mode, xsimd::unaligned_mode>(
                    Arch {}, new_in, new_out);
            }
        }
    }
}

#ifdef CDFPP_ENABLE_SSE2_ARCH
extern template void _to_ns_from_1970_tt2000_t::operator()<xsimd::sse2>(
    xsimd::sse2, const std::span<const tt2000_t>& input, int64_t* const output);
extern template void _to_ns_from_1970_epoch_t::operator()<xsimd::sse2>(
    xsimd::sse2, const std::span<const epoch>& input, int64_t* const output);
extern template void _to_ns_from_1970_epoch16_t::operator()<xsimd::sse2>(
    xsimd::sse2, const std::span<const epoch16>& input, int64_t* const output);
#endif
#ifdef CDFPP_ENABLE_AVX2_ARCH
extern template void _to_ns_from_1970_tt2000_t::operator()<xsimd::avx2>(
    xsimd::avx2, const std::span<const tt2000_t>& input, int64_t* const output);
extern template void _to_ns_from_1970_epoch_t::operator()<xsimd::avx2>(
    xsimd::avx2, const std::span<const epoch>& input, int64_t* const output);
extern template void _to_ns_from_1970_epoch16_t::operator()<xsimd::avx2>(
    xsimd::avx2, const std::span<const epoch16>& input, int64_t* const output);
#endif
#ifdef CDFPP_ENABLE_AVX512BW_ARCH
extern template void _to_ns_from_1970_tt2000_t::operator()<xsimd::avx512bw>(
    xsimd::avx512bw, const std::span<const tt2000_t>& input, int64_t* const output);
extern template void _to_ns_from_1970_epoch_t::operator()<xsimd::avx512bw>(
    xsimd::avx512bw, const std::span<const epoch>& input, int64_t* const output);
extern template void _to_ns_from_1970_epoch16_t::operator()<xsimd::avx512bw>(
    xsimd::avx512bw, const std::span<const epoch16>& input, int64_t* const output);
#endif
}
