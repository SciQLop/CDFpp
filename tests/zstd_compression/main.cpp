#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cdfpp_config.h>
#ifdef CDFPP_USE_ZSTD
#include "cdfpp/cdf-io/zstd.hpp"
#endif
#include <cstdint>
#include <numeric>

#ifdef CDFPP_USE_ZSTD
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
    auto w2 = cdf::io::zstd::deflate(ref);
    cdf::io::zstd::inflate(w2, w.data(), std::size(ref));
    w2 = cdf::io::zstd::deflate(w);
    cdf::io::zstd::inflate(w2, w.data(), std::size(ref));
    REQUIRE(ref == w);
}

TEST_CASE("inflate returns 0 on garbage input", "")
{
    no_init_vector<char> garbage { 'n', 'o', 't', ' ', 'z', 's', 't', 'd' };
    char output[256] = {};
    auto ret = cdf::io::zstd::inflate(garbage, output, sizeof(output));
    REQUIRE(ret == 0);
}

TEST_CASE("deflate then inflate with truncated data returns 0", "")
{
    const no_init_vector<char> ref = build_ref();
    auto compressed = cdf::io::zstd::deflate(ref);
    REQUIRE(std::size(compressed) > 4);
    compressed.resize(4);
    char output[16000] = {};
    auto ret = cdf::io::zstd::inflate(compressed, output, sizeof(output));
    REQUIRE(ret == 0);
}
#else
TEST_CASE("Skip check", "") { }
#endif
