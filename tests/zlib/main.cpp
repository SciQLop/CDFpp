#if __has_include(<catch2/catch_all.hpp>)
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#else
#include <catch.hpp>
#endif
#include "cdfpp/cdf-io/cdf-io-zlib.hpp"
#include <algorithm>
#include <cstdint>
#include <random>

TEST_CASE("", "")
{
    using namespace cdf::io::zlib;
    std::vector<char> data { 'h', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', ' ', 's', 'o',
        'm', 'e', '\0' };
    auto comp = cdf::io::zlib::deflate(data);
    auto decomp = cdf::io::zlib::inflate<char>(comp);
    REQUIRE(decomp == data);
}

TEST_CASE("", "")
{
    using namespace cdf::io::zlib;
    std::vector<double> data(2048);
    std::generate(std::begin(data), std::end(data), []() mutable { return 1.; });
    auto comp = cdf::io::zlib::deflate(data);
    REQUIRE(std::size(comp) < std::size(data));
    auto decomp = cdf::io::zlib::inflate<double>(comp);
    REQUIRE(decomp == data);
}

TEST_CASE("", "")
{
    using namespace cdf::io::zlib;
    std::vector<double> data(2048);
    std::generate(std::begin(data), std::end(data), []() mutable { return 1.; });
    auto comp = cdf::io::zlib::deflate(data);
    REQUIRE(std::size(comp) < std::size(data));
    REQUIRE(cdf::io::zlib::inflate(comp, data));
    REQUIRE(std::size(data) == 4096);
    REQUIRE(std::accumulate(
        std::cbegin(data), std::cend(data), true, [](double a, bool b) { return b && (a == 1.); }));
}

TEST_CASE("", "")
{
    std::random_device rnd_device;
    std::mt19937 mersenne_engine { rnd_device() };
    std::uniform_int_distribution<int64_t> dist { -99999999999999, 99999999999999 };

    auto gen = [&dist, &mersenne_engine]() {
        return static_cast<double>(dist(mersenne_engine)) / 10000.;
    };

    std::vector<double> data(1024 * 1024 * 4);
    std::generate(begin(data), end(data), gen);

    auto comp = cdf::io::zlib::deflate(data);
    REQUIRE(std::size(comp) < (std::size(data) * sizeof(double)));
    auto decomp = cdf::io::zlib::inflate<double>(comp);
    REQUIRE(decomp == data);
}
