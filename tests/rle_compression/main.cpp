#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

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
TEST_CASE("deflate handles runs longer than 256 zeros", "")
{
    no_init_vector<char> input(300, 0);
    input.push_back(42);
    auto deflated = cdf::io::rle::deflate(input);
    no_init_vector<char> output(301);
    cdf::io::rle::inflate(deflated, output.data(), 301);
    REQUIRE(input == output);
}

TEST_CASE("deflate handles exactly 256 zeros", "")
{
    no_init_vector<char> input(256, 0);
    auto deflated = cdf::io::rle::deflate(input);
    REQUIRE(deflated == no_init_vector<char> { 0, static_cast<char>(255) });
    no_init_vector<char> output(256);
    cdf::io::rle::inflate(deflated, output.data(), 256);
    REQUIRE(input == output);
}

TEST_CASE("deflate handles exactly 257 zeros", "")
{
    no_init_vector<char> input(257, 0);
    auto deflated = cdf::io::rle::deflate(input);
    no_init_vector<char> output(257);
    cdf::io::rle::inflate(deflated, output.data(), 257);
    REQUIRE(input == output);
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

TEST_CASE("inflate stops at output_size boundary", "")
{
    // Compressed data that decompresses to 10 bytes: {0,0,0,0,0, 1, 2, 3, 4, 5}
    // RLE encoding: {0, 4, 1, 2, 3, 4, 5} (5 zeros encoded as 0x00 0x04, then 5 literals)
    no_init_vector<char> input { 0, 4, 1, 2, 3, 4, 5 };
    constexpr std::size_t output_size = 6;
    no_init_vector<char> output(output_size, static_cast<char>(0xFF));
    auto written = cdf::io::rle::inflate(input, output.data(), output_size);
    // inflate must not write more than output_size bytes
    REQUIRE(written <= output_size);
}

TEST_CASE("inflate handles truncated input (zero byte at end)", "")
{
    // Malformed: a zero byte with no following count byte
    no_init_vector<char> input { 1, 2, 0 };
    no_init_vector<char> output(16, static_cast<char>(0xFF));
    // Must not read past end of input — should either stop or return an error,
    // not dereference past std::cend(input)
    auto written = cdf::io::rle::inflate(input, output.data(), 16);
    // At minimum the two literal bytes should have been written
    REQUIRE(written >= 2);
    REQUIRE(output[0] == 1);
    REQUIRE(output[1] == 2);
}
