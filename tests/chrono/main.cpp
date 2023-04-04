#include <chrono>
#include <ctime>

#if __has_include(<catch2/catch_all.hpp>)
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#else
#include <catch.hpp>
#endif

#include "cdfpp/chrono/cdf-chrono.hpp"
#include "test_values.hpp"


TEST_CASE("cdf epoch to timepoint", "")
{
    using namespace std::chrono;

    for (const auto& item : test_values)
    {
        auto tp = time_point<std::chrono::system_clock> {} + seconds(item.unix_epoch);
        REQUIRE(tp == cdf::to_time_point(item.epoch_epoch));
    }
}

TEST_CASE("cdf epoch16 to timepoint", "")
{
    using namespace std::chrono;

    for (const auto& item : test_values)
    {
        auto tp = time_point<std::chrono::system_clock> {} + seconds(item.unix_epoch);
        REQUIRE(tp == cdf::to_time_point(item.epoch16_epoch));
    }
}


TEST_CASE("cdf tt2000 to timepoint", "")
{
    using namespace std::chrono;

    for (const auto& item : test_values)
    {
        if (item.unix_epoch >= 68688000)
        {
            auto tp = time_point<std::chrono::system_clock> {} + seconds(item.unix_epoch);
            REQUIRE(tp == cdf::to_time_point(item.tt2000_epoch));
        }
    }
}


TEST_CASE("timepoint to cdf epoch", "")
{
    using namespace std::chrono;

    for (const auto& item : test_values)
    {
        auto tp = time_point<std::chrono::system_clock> {} + seconds(item.unix_epoch);
        REQUIRE(item.epoch_epoch == cdf::to_epoch(tp));
    }
}

TEST_CASE("timepoint to cdf epoch16", "")
{
    using namespace std::chrono;

    for (const auto& item : test_values)
    {
        auto tp = time_point<std::chrono::system_clock> {} + seconds(item.unix_epoch);
        REQUIRE(item.epoch16_epoch == cdf::to_epoch16(tp));
    }
}

TEST_CASE("timepoint to cdf tt2000", "")
{
    using namespace std::chrono;

    for (const auto& item : test_values)
    {
        if (item.unix_epoch >= 68688000)
        {
            auto tp = time_point<std::chrono::system_clock> {} + seconds(item.unix_epoch);
            REQUIRE(item.tt2000_epoch.value == cdf::to_tt2000(tp).value);
        }
    }
}
