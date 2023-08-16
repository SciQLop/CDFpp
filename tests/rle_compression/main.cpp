#if __has_include(<catch2/catch_all.hpp>)
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#else
#include <catch.hpp>
#endif
#include "cdfpp/cdf-io/rle.hpp"
#include <cstdint>


TEST_CASE("simple deflate", "")
{
    REQUIRE(no_init_vector<char> {} == cdf::io::rle::deflate(no_init_vector<char> {}));
    REQUIRE(no_init_vector<char> { 1, 2, 3, 4, 5 }
        == cdf::io::rle::deflate(no_init_vector<char> { 1, 2, 3, 4, 5 }));
    REQUIRE(no_init_vector<char> { 0, 0, 1, 2, 3, 4, 5 }
        == cdf::io::rle::deflate(no_init_vector<char> { 0, 1, 2, 3, 4, 5 }));
    REQUIRE(no_init_vector<char> { 0, 3, 1, 2, 3, 4, 5 }
        == cdf::io::rle::deflate(no_init_vector<char> { 0, 0, 0, 0, 1, 2, 3, 4, 5 }));
    REQUIRE(no_init_vector<char> { 1, 2, 3, 4, 5, 0, 3 }
        == cdf::io::rle::deflate(no_init_vector<char> { 1, 2, 3, 4, 5, 0, 0, 0, 0 }));
    REQUIRE(no_init_vector<char> { 1, 2, 0, 3, 3, 4, 5 }
        == cdf::io::rle::deflate(no_init_vector<char> { 1, 2, 0, 0, 0, 0, 3, 4, 5 }));
}
TEST_CASE("IDEMPOTENCY check", "")
{
    const no_init_vector<char> ref { 1, 2, 0, 0, 0, 0, 3, 4, 5 };
    no_init_vector<char> w(9UL);
    auto w2 = cdf::io::rle::deflate(ref);
    cdf::io::rle::inflate(w2, w.data(), 9);
    w2 = cdf::io::rle::deflate(w);
    cdf::io::rle::inflate(w2, w.data(), 9);
    REQUIRE(ref == w);
}
