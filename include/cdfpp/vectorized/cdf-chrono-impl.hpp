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
        const auto ps_in_ns = xsimd::broadcast<double, Arch>(1'000);
        const auto ns_in_s = xsimd::broadcast<double, Arch>(1'000'000'000);
        std::size_t i = 0;
        for (; i + simd_size <= count; i += simd_size)
        {
            auto seconds
                = batchin_type::gather(reinterpret_cast<const double*>(&input[i]), even_indexes);
            auto picos
                = batchin_type::gather(reinterpret_cast<const double*>(&input[i]), odd_indexes);
            xsimd::batch_cast<int64_t>(((seconds - offset) * ns_in_s + (picos / ps_in_ns)))
                .store(output + i, output_align_mode {});
        }
        if (i < count)
        {
            _impl::scalar_to_ns_from_1970(input.subspan(i), output + i);
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
    if constexpr (cdf::helpers::is_any_of_v<Arch, xsimd::unavailable, xsimd::sse2, xsimd::avx2>)
    {
        return _impl::scalar_to_ns_from_1970(input, output);
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
        const auto offset = xsimd::broadcast<double, Arch>(constants::epoch_offset_miliseconds);
        const auto ns_in_ms = xsimd::broadcast<double, Arch>(1'000'000);
        std::size_t i = 0;
        for (; i + simd_size <= count; i += simd_size)
        {
            auto epoch_batch = batchin_type::load(&input[i].mseconds, input_align_mode {});
            store<Arch, output_align_mode>(
                xsimd::batch_cast<int64_t>((epoch_batch - offset) * ns_in_ms), output + i);
        }
        if (i < count)
        {
            _impl::scalar_to_ns_from_1970(input.subspan(i), output + i);
        }
    }

    template <class Arch>
    void operator()(Arch, const std::span<const epoch>& input, int64_t* const output);
};

template <class Arch>
void _to_ns_from_1970_epoch_t::operator()(
    Arch, const std::span<const epoch>& input, int64_t* const output)
{
    if constexpr (cdf::helpers::is_any_of_v<Arch, xsimd::unavailable, xsimd::sse2, xsimd::avx2>)
    {
        return _impl::scalar_to_ns_from_1970(input, output);
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
        auto was_after_2017 = xsimd::batch_bool<int64_t, Arch>(true);
        for (; i + simd_size <= count; i += simd_size)
        {
            auto tt2000_batch = batch_type::load(&input[i].nseconds, input_align_mode {});
            store<Arch, output_align_mode>(tt2000_batch + offset, &output[i]);
            was_after_2017 = was_after_2017 & (tt2000_batch >= last_leap_sec);
        }
        // sfence<Arch>();
        if (!xsimd::all(was_after_2017))
        {
            return _unsorted<Arch, input_align_mode, output_align_mode>(
                Arch {}, input, output);
        }
        if (i < count)
        {
            _impl::scalar_to_ns_from_1970(input.subspan(i), &output[i]);
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


        for (; i + simd_size <= count; i += simd_size)
        {
            auto tt2000_batch = batch_type::load(&input[i].nseconds, input_align_mode {});
            auto offset
                = xsimd::broadcast<int64_t, Arch>(constants::tt2000_offset - max_leap_offset);
            int leap_index = std::size(leap_seconds::leap_seconds_tt2000_reverse) - 2;
            auto needs_correction = (tt2000_batch < last_leap_sec);
            if (!xsimd::any(needs_correction))
            {
                store<Arch, output_align_mode>(tt2000_batch + offset, &output[i]);
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
            store<Arch, output_align_mode>(tt2000_batch + offset, &output[i]);
        }
        // sfence<Arch>();
        if (i < count)
        {
            _impl::scalar_to_ns_from_1970(input.subspan(i), &output[i]);
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
        return _impl::scalar_to_ns_from_1970(input, output);
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
                    _impl::scalar_to_ns_from_1970));
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
