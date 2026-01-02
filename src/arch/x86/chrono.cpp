#include <cdfpp/vectorized/cdf-chrono.hpp>
#include <cdfpp/vectorized/cdf-chrono-impl.hpp>

namespace cdf::chrono::vectorized
{

template void _to_ns_from_1970_tt2000_t::operator()<xsimd::avx>(
    xsimd::avx, const tt2000_t*const input, const std::size_t count, int64_t*const output);

template void _to_ns_from_1970_tt2000_t::operator()<xsimd::avx512bw>(
    xsimd::avx512bw, const tt2000_t*const input, const std::size_t count, int64_t*const output);

template void _to_ns_from_1970_tt2000_t::operator()<xsimd::sse2>(
    xsimd::sse2, const tt2000_t*const input, const std::size_t count, int64_t*const output);


auto _disp_to_ns_from_1970_tt2000
    = xsimd::dispatch<xsimd::arch_list<xsimd::avx512vnni<xsimd::avx512bw>, xsimd::avx512bw, xsimd::sse2>>(_to_ns_from_1970_tt2000_t {});

template void _to_ns_from_1970_epoch_t::operator()<xsimd::avx>(
    xsimd::avx, const epoch*const input, const std::size_t count, int64_t*const output);

template void _to_ns_from_1970_epoch_t::operator()<xsimd::avx512bw>(
    xsimd::avx512bw, const epoch*const input, const std::size_t count, int64_t*const output);

template void _to_ns_from_1970_epoch_t::operator()<xsimd::sse2>(
    xsimd::sse2, const epoch*const input, const std::size_t count, int64_t*const output);

auto _disp_to_ns_from_1970_ep
    = xsimd::dispatch<xsimd::arch_list<xsimd::avx512vnni<xsimd::avx512bw>, xsimd::avx512bw, xsimd::sse2>>(_to_ns_from_1970_epoch_t {});

template void _to_ns_from_1970_epoch16_t::operator()<xsimd::avx>(
    xsimd::avx, const epoch16*const input, const std::size_t count, int64_t*const output);

template void _to_ns_from_1970_epoch16_t::operator()<xsimd::avx512bw>(
    xsimd::avx512bw, const epoch16*const input, const std::size_t count, int64_t*const output);

template void _to_ns_from_1970_epoch16_t::operator()<xsimd::sse2>(
    xsimd::sse2, const epoch16*const input, const std::size_t count, int64_t*const output);

auto _disp_to_ns_from_1970_ep16
    = xsimd::dispatch<xsimd::arch_list<xsimd::avx512vnni<xsimd::avx512bw>, xsimd::avx512bw, xsimd::sse2>>(_to_ns_from_1970_epoch16_t {});


} // namespace cdf::chrono::vectorized

void vectorized_to_ns_from_1970(const cdf::tt2000_t* const input, const std::size_t count, int64_t* const output)
{
    cdf::chrono::vectorized::_disp_to_ns_from_1970_tt2000(input, count, output);
}

void vectorized_to_ns_from_1970(const cdf::epoch* const input, const std::size_t count, int64_t* const output)
{
    cdf::chrono::vectorized::_disp_to_ns_from_1970_ep(input, count, output);
}

void vectorized_to_ns_from_1970(const cdf::epoch16* const input, const std::size_t count, int64_t* const output)
{
    cdf::chrono::vectorized::_disp_to_ns_from_1970_ep16(input, count, output);
}
