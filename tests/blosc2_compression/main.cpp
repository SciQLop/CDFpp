#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cdfpp_config.h>
#ifdef CDFPP_USE_BLOSC2
#include "cdfpp/cdf-io/blosc2.hpp"
#endif
#include <cmath>
#include <cstring>
#include <numeric>

#ifdef CDFPP_USE_BLOSC2
using namespace cdf::io;

TEST_CASE("typesize hint uses the whole record when it fits blosc2's 255-byte limit", "")
{
    REQUIRE(blosc2::typesize_hint(8, 24) == 24); // 3-component float64 vector
    REQUIRE(blosc2::typesize_hint(4, 255) == 255);
    REQUIRE(blosc2::typesize_hint(4, 256) == 4); // too large, back to the element size
    REQUIRE(blosc2::typesize_hint(4, 64 * 1024) == 4); // e.g. a particle distribution record
    REQUIRE(blosc2::typesize_hint(8, 8) == 8); // scalar record
    REQUIRE(blosc2::typesize_hint(1, 1) == 1); // untyped data, e.g. a whole-file CCR payload
}

no_init_vector<char> vectors_as_bytes(std::size_t count)
{
    no_init_vector<double> values(count * 3);
    for (std::size_t i = 0; i < count; i++)
    {
        values[3 * i] = 7000. * std::cos(i * 1e-3);
        values[3 * i + 1] = -300. + std::sin(i * 1e-3);
        values[3 * i + 2] = 1e-3 * i;
    }
    no_init_vector<char> bytes(std::size(values) * sizeof(double));
    std::memcpy(bytes.data(), values.data(), std::size(bytes));
    return bytes;
}

TEST_CASE("round trip with element and record hints", "")
{
    const auto ref = vectors_as_bytes(10000);
    const auto [element_size, record_size]
        = GENERATE(std::pair<std::size_t, std::size_t> { 8, 8 },
            std::pair<std::size_t, std::size_t> { 8, 24 }, std::pair<std::size_t, std::size_t> { 1, 1 });
    const auto compressed = blosc2::deflate(ref, element_size, record_size);
    REQUIRE(std::size(compressed) > 0);
    REQUIRE(std::size(compressed) < std::size(ref));
    no_init_vector<char> restored(std::size(ref));
    REQUIRE(blosc2::inflate(compressed, restored.data(), std::size(restored)) == std::size(ref));
    REQUIRE(restored == ref);
}

TEST_CASE("inflate returns 0 on garbage input", "")
{
    no_init_vector<char> garbage(64, 'x');
    char output[256] = {};
    REQUIRE(blosc2::inflate(garbage, output, sizeof(output)) == 0);
}
#else
TEST_CASE("Skip check", "") { }
#endif
