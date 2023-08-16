#if __has_include(<catch2/catch_all.hpp>)
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#else
#include <catch.hpp>
#endif
#include <cdfpp_config.h>
#ifndef CDFpp_USE_LIBDEFLATE
#include "cdfpp/cdf-io/zlib.hpp"
#endif
#include <cstdint>


#ifndef CDFpp_USE_LIBDEFLATE
no_init_vector<char> build_ref()
{
    no_init_vector<char> ref(16000);
    std::iota(std::begin(ref), std::end(ref), 1);
    return ref;
}
TEST_CASE("IDEMPOTENCY check", "")
{
    const no_init_vector<char> ref = build_ref();
    no_init_vector<char> w(std::size(ref));
    auto w2 = cdf::io::zlib::gzdeflate(ref);
    cdf::io::zlib::gzinflate(w2, w.data(), std::size(ref));
    w2 = cdf::io::zlib::gzdeflate(w);
    cdf::io::zlib::gzinflate(w2, w.data(), std::size(ref));
    REQUIRE(ref == w);
}
#else
TEST_CASE("Skip check", "")
{}
#endif
