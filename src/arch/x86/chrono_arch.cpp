#include <cdfpp/vectorized/cdf-chrono-impl.hpp>

namespace cdf::chrono::vectorized
{

template void _to_ns_from_1970_tt2000_t::operator()<xsimd::CDFPP_ARCH>(xsimd::CDFPP_ARCH, const std::span<const tt2000_t>& input, int64_t* const output);
template void _to_ns_from_1970_epoch_t::operator()<xsimd::CDFPP_ARCH>(xsimd::CDFPP_ARCH, const std::span<const epoch>& input, int64_t* const output);
template void _to_ns_from_1970_epoch16_t::operator()<xsimd::CDFPP_ARCH>(xsimd::CDFPP_ARCH, const std::span<const epoch16>& input, int64_t* const output);

} // namespace cdf::chrono::vectorized
