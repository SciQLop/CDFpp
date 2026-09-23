/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2026, Laboratory of Plasma Physics- CNRS
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
#include "../cdf-debug.hpp"
#include <cpp_utils/containers/no_init_vector.hpp>
using cpp_utils::containers::no_init_vector;
#include <algorithm>
#if defined(_WIN32) && !defined(NOMINMAX)
// blosc2.h includes <windows.h>, whose min/max macros break every later std::min/std::max.
#define NOMINMAX
#endif
#include <blosc2.h>
#include <cstddef>
#include <memory>

// Blosc2 chunks are self-describing (codec, filters and typesize live in the chunk header),
// so decoding needs no parameter beyond the bytes; only encoding picks these settings.
// simplify: one call per block limits blocks to BLOSC2_MAX_BUFFERSIZE (~2GB); VVRs are capped
// at 1GB so variables are fine, but a whole-file (CCR) compression above 2GB fails. Use a
// blosc2 super-chunk if file-level blosc2 on huge files ever matters.
namespace cdf::io::blosc2
{
inline constexpr int compression_level = 5;

// Shuffling whole records (when they fit blosc2's 255-byte typesize) gives each record component
// its own byte streams, e.g. Bx, By, Bz of a vector. On a 39-file CDAWeb/NASA corpus this was
// ~3% smaller overall than using the element size (benchmarks/compression/sweep.py).
[[nodiscard]] inline std::size_t typesize_hint(std::size_t element_size, std::size_t record_size)
{
    return record_size <= BLOSC_MAX_TYPESIZE ? record_size : element_size;
}

namespace _internal
{
    using context_ptr = std::unique_ptr<blosc2_context, decltype(&blosc2_free_ctx)>;

    inline void ensure_initialized()
    {
        [[maybe_unused]] static const bool initialized = []()
        {
            blosc2_init();
            return true;
        }();
    }

    inline context_ptr make_cctx(std::size_t typesize)
    {
        ensure_initialized();
        blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
        cparams.compcode = BLOSC_ZSTD;
        cparams.clevel = compression_level;
        cparams.typesize = static_cast<int32_t>(typesize);
        cparams.nthreads = 1;
        return { blosc2_create_cctx(cparams), &blosc2_free_ctx };
    }

    inline context_ptr make_dctx()
    {
        ensure_initialized();
        blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
        dparams.nthreads = 1;
        return { blosc2_create_dctx(dparams), &blosc2_free_ctx };
    }

    template <typename T>
    CDF_WARN_UNUSED_RESULT std::size_t impl_inflate(
        const T& input, char* output, const std::size_t output_size)
    {
        const auto ctx = make_dctx();
        const auto ret = blosc2_decompress_ctx(ctx.get(), input.data(),
            static_cast<int32_t>(std::size(input)), output,
            static_cast<int32_t>(
                std::min(output_size, static_cast<std::size_t>(BLOSC2_MAX_BUFFERSIZE))));
        return ret > 0 ? static_cast<std::size_t>(ret) : 0;
    }

    template <typename T>
    CDF_WARN_UNUSED_RESULT no_init_vector<char> impl_deflate(
        const T& input, std::size_t element_size, std::size_t record_size)
    {
        if (std::size(input) > static_cast<std::size_t>(BLOSC2_MAX_BUFFERSIZE))
            return {};
        const auto ctx = make_cctx(typesize_hint(element_size, record_size));
        no_init_vector<char> result(std::size(input) + BLOSC2_MAX_OVERHEAD);
        const auto ret = blosc2_compress_ctx(ctx.get(), input.data(),
            static_cast<int32_t>(std::size(input)), result.data(),
            static_cast<int32_t>(std::size(result)));
        if (ret <= 0)
            return {};
        result.resize(static_cast<std::size_t>(ret));
        result.shrink_to_fit();
        return result;
    }
}

template <typename T>
std::size_t inflate(const T& input, char* output, const std::size_t output_size)
{
    using namespace _internal;
    return impl_inflate(input, output, output_size);
}

template <typename T>
no_init_vector<char> deflate(const T& input, std::size_t element_size, std::size_t record_size)
{
    using namespace _internal;
    return impl_deflate(input, element_size, record_size);
}
}
