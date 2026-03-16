#include <chrono>
#include <ctime>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>


#include "cdfpp/chrono/cdf-chrono.hpp"
#include "test_values.hpp"


TEST_CASE("Leap Seconds", "")
{
    using namespace cdf::chrono::leap_seconds;
    SECTION("int64_t leap_second(int64_t ns_from_1970)")
    {
        for (const auto& item : leap_seconds_tt2000)
        {
            REQUIRE(cdf::_impl::leap_second(item.first + 1000000000) == item.second);
            REQUIRE(cdf::_impl::leap_second(item.first) == item.second);
        }
    }
    SECTION("int64_t leap_second_branchless(int64_t ns_from_1970)")
    {
        for (const auto& item : leap_seconds_tt2000)
        {
            REQUIRE(cdf::_impl::leap_second_branchless(item.first + 1000000000) == item.second);
        }
    }
    SECTION("tt2000_t leap_second at exact boundary")
    {
        // The first entry in the reverse table corresponds to Jan 1, 1972
        auto first_entry = leap_seconds_tt2000_reverse.front();
        REQUIRE(cdf::_impl::leap_second(cdf::tt2000_t { first_entry.first }) == first_entry.second);
    }
    SECTION("tt2000_t leap_second returns 0 for pre-1972")
    {
        // Any TT2000 value before Jan 1, 1972 should have 0 leap seconds
        REQUIRE(cdf::_impl::leap_second(cdf::tt2000_t { -1000000000000000000 }) == 0);
    }
}


TEST_CASE("To ns from 1970", "")
{
    using namespace cdf;
    using namespace cdf::chrono;
    using namespace cdf::chrono::leap_seconds;
    SECTION("Basic test")
    {
        auto input = std::vector<cdf::tt2000_t> { 0_tt2k, 631108869184000000_tt2k };
        auto output = std::vector<int64_t>(std::size(input));
        cdf::_impl::scalar_to_ns_from_1970(input, output.data());
        REQUIRE(output[0] / 1'000'000LL == 946727935816);
        REQUIRE(output[1] / 1'000'000LL == 1577836800000);

        // retry value by value because there are two different implementations
        // one that works for all values and one that is optimized for after the last
        // leap second (i.e., 2017-01-01T00:00:00Z), the first value triggers the
        // optimized path assuming the input is sorted.
        cdf::_impl::scalar_to_ns_from_1970({input.data(), 1}, output.data());
        REQUIRE(output[0] / 1'000'000LL == 946727935816);
        cdf::_impl::scalar_to_ns_from_1970({input.data() + 1, 1}, output.data() + 1);
        REQUIRE(output[1] / 1'000'000LL == 1577836800000);
    }
    SECTION("Scalar")
    {
        for (const auto& item : test_values)
        {
            if (item.unix_epoch >= 68688000)
            {
                int64_t output = 0;
                cdf::_impl::scalar_to_ns_from_1970({&item.tt2000_epoch, 1}, &output);
                int64_t expected = item.unix_epoch * 1'000'000'000LL;
                REQUIRE(output == expected);
            }
        }
    }
    SECTION("Scalar pre-1960 (no TAI-UTC offset)")
    {
        for (const auto& item : test_values)
        {
            // Before ~1960, TAI==UTC so tt2000 + offset == unix_epoch * 1e9 exactly.
            // NASA CDF starts applying pre-leap-second TAI-UTC corrections around 1960.
            if (item.unix_epoch < -320112000)
            {
                int64_t output = 0;
                cdf::_impl::scalar_to_ns_from_1970({&item.tt2000_epoch, 1}, &output);
                int64_t expected = item.unix_epoch * 1'000'000'000LL;
                REQUIRE(output == expected);
            }
        }
    }
    SECTION("Scalar 1960-1972 (rubber seconds era, no gross error)")
    {
        for (const auto& item : test_values)
        {
            // Between ~1960-1972, fractional TAI-UTC offsets exist that we don't model.
            // Just verify there's no 10-second gross error from the old leap_second bug.
            if (item.unix_epoch >= -320112000 && item.unix_epoch < 68688000)
            {
                int64_t output = 0;
                cdf::_impl::scalar_to_ns_from_1970({&item.tt2000_epoch, 1}, &output);
                int64_t expected = item.unix_epoch * 1'000'000'000LL;
                REQUIRE(std::abs(output - expected) < 11'000'000'000LL);
            }
        }
    }
    SECTION("Vectorized against scalar")
    {
        std::vector<cdf::tt2000_t> inputs(1024);
        std::vector<int64_t> expected_outputs(inputs.size());
        for (std::size_t i = 0; i < inputs.size(); ++i)
        {
            inputs[i] = cdf::tt2000_t(-869399957816000000 + i * 1000000000LL);
        }
        cdf::_impl::scalar_to_ns_from_1970(inputs, expected_outputs.data());
        std::vector<int64_t> outputs(inputs.size());
        cdf::to_ns_from_1970(std::span{inputs}, outputs.data());
        for (std::size_t i = 0; i < outputs.size(); ++i)
        {
            REQUIRE(outputs[i] == expected_outputs[i]);
        }
    }
    SECTION("Vectorized pre-1972 matches scalar")
    {
        for (const auto& item : test_values)
        {
            if (item.unix_epoch < 68688000)
            {
                int64_t scalar_output = 0;
                int64_t vectorized_output = 0;
                cdf::_impl::scalar_to_ns_from_1970({&item.tt2000_epoch, 1}, &scalar_output);
                cdf::to_ns_from_1970(
                    std::span<const cdf::tt2000_t> { &item.tt2000_epoch, 1 }, &vectorized_output);
                REQUIRE(scalar_output == vectorized_output);
            }
        }
    }
    SECTION("Vectorized pre-1960 exact")
    {
        for (const auto& item : test_values)
        {
            if (item.unix_epoch < -320112000)
            {
                int64_t output = 0;
                cdf::to_ns_from_1970(
                    std::span<const cdf::tt2000_t> { &item.tt2000_epoch, 1 }, &output);
                int64_t expected = item.unix_epoch * 1'000'000'000LL;
                REQUIRE(output == expected);
            }
        }
    }
}


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

TEST_CASE("timepoint to cdf epoch16 sub-second precision", "")
{
    using namespace std::chrono;

    auto tp = time_point<system_clock> {} + seconds(1'700'000'000) + nanoseconds(123'456'789);
    auto ep16 = cdf::to_epoch16(tp);
    double expected_seconds = 1'700'000'000.0 + cdf::chrono::constants::epoch_offset_seconds;
    double expected_picoseconds = 123'456'789'000.0;
    REQUIRE(ep16.seconds == expected_seconds);
    REQUIRE(ep16.picoseconds == expected_picoseconds);
}

TEST_CASE("timepoint to cdf tt2000", "")
{
    using namespace std::chrono;

    for (const auto& item : test_values)
    {
        if (item.unix_epoch >= 68688000)
        {
            auto tp = time_point<std::chrono::system_clock> {} + seconds(item.unix_epoch);
            REQUIRE(item.tt2000_epoch.nseconds == cdf::to_tt2000(tp).nseconds);
        }
    }
}
