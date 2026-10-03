#include <cdfpp/vectorized/cdf-chrono-impl.hpp>

// WebAssembly SIMD: 128-bit registers, two 64-bit lanes. The kernels written for x86, built for
// xsimd's wasm target. WebAssembly has floor, trunc, 64-bit compares and selects in its SIMD
// instructions, so they map one to one.

namespace cdf::chrono::vectorized
{
void scalar_to_ns_from_1970(const std::span<const tt2000_t>& input, int64_t* const output)
{
    _impl::scalar_to_ns_from_1970(input, output);
}

void scalar_to_ns_from_1970(const std::span<const epoch>& input, int64_t* const output)
{
    _impl::scalar_to_ns_from_1970(input, output);
}

void scalar_to_ns_from_1970(const std::span<const epoch16>& input, int64_t* const output)
{
    _impl::scalar_to_ns_from_1970(input, output);
}

void scalar_from_ns_from_1970(const std::span<const int64_t>& input, tt2000_t* const output)
{
    _impl::scalar_from_ns_from_1970(input, output);
}
} // namespace cdf::chrono::vectorized

void vectorized_to_ns_from_1970(const std::span<const cdf::tt2000_t>& input, int64_t* const output)
{
    cdf::chrono::vectorized::_to_ns_from_1970_tt2000_t {}(xsimd::wasm {}, input, output);
}

void vectorized_to_ns_from_1970(const std::span<const cdf::epoch>& input, int64_t* const output)
{
    cdf::chrono::vectorized::_to_ns_from_1970_epoch_t {}(xsimd::wasm {}, input, output);
}

void vectorized_to_ns_from_1970(const std::span<const cdf::epoch16>& input, int64_t* const output)
{
    cdf::chrono::vectorized::_to_ns_from_1970_epoch16_t {}(xsimd::wasm {}, input, output);
}

void vectorized_from_ns_from_1970(const std::span<const int64_t>& input, cdf::tt2000_t* const output)
{
    cdf::chrono::vectorized::_from_ns_from_1970_tt2000_t {}(xsimd::wasm {}, input, output);
}
