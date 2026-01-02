#include <cdfpp/vectorized/cdf-chrono-impl.hpp>

namespace cdf::chrono::vectorized
{

auto _disp_to_ns_from_1970_tt2000
    = xsimd::dispatch<CDFPP_XSIMD_ARCH_LIST>(_to_ns_from_1970_tt2000_t {});
auto _disp_to_ns_from_1970_epoch = xsimd::dispatch<CDFPP_XSIMD_ARCH_LIST>(_to_ns_from_1970_epoch_t {});
auto _disp_to_ns_from_1970_epoch16 = xsimd::dispatch<CDFPP_XSIMD_ARCH_LIST>(_to_ns_from_1970_epoch16_t {});

} // namespace cdf::chrono::vectorized

void vectorized_to_ns_from_1970(
    const std::span<const cdf::tt2000_t>& input, int64_t* const output)
{
    cdf::chrono::vectorized::_disp_to_ns_from_1970_tt2000(input, output);
}

void vectorized_to_ns_from_1970(
    const std::span<const cdf::epoch>& input, int64_t* const output)
{
    cdf::chrono::vectorized::_disp_to_ns_from_1970_epoch(input, output);
}

void vectorized_to_ns_from_1970(
    const std::span<const cdf::epoch16>& input, int64_t* const output)
{
    cdf::chrono::vectorized::_disp_to_ns_from_1970_epoch16(input, output);
}
