#include <random>
#include <cmath>
#include <limits>
#include <vector>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <sstream>
#include <string>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>


#include "cdfpp/cdf-repr.hpp"
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
    SECTION("tt2000_t leap_second follows the pre-1972 drift")
    {
        // 1968-04-24: TAI-UTC drifted by a fraction of a second per day before 1972. Expected
        // value from NASA's CDF library 3.9.2 (CDF_TT2000_to_UTC_parts).
        REQUIRE(cdf::_impl::leap_second(cdf::tt2000_t { -1000000000000000000 }) == 6402114000);
    }
    SECTION("tt2000_t leap_second is 0 before 1960")
    {
        // 1958-10-22: before the first entry of the table, TAI-UTC is 0.
        REQUIRE(cdf::_impl::leap_second(cdf::tt2000_t { -1300000000000000000 }) == 0);
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


#ifndef CDFPP_NO_SIMD
namespace
{
// CDF_EPOCH values the SIMD path must convert exactly like the scalar one: sub-millisecond
// fractions, pre-1970 dates, the int64 limits, fill and pad values (out of range, so NaT) and
// random values over the whole int64 range and beyond it.
std::vector<cdf::epoch> epochs_to_compare()
{
    constexpr double offset = cdf::constants::epoch_offset_miliseconds;
    constexpr double int64_limit_ms = 9223372036854.775807;
    std::vector<cdf::epoch> values;
    for (double ms : { 0.0, -1e31, 1e31, std::nan(""), std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity(), offset, offset + 1e-6, offset - 1e-6,
             offset - 0.5e-6, offset + 63745056000068.5 - 62167219200000.0, offset - 1.5,
             std::nextafter(offset + int64_limit_ms, 0.0), std::nextafter(offset - int64_limit_ms, 0.0),
             offset + int64_limit_ms, offset - int64_limit_ms, offset + int64_limit_ms * 1.0001,
             offset - int64_limit_ms * 1.0001 })
        values.push_back(cdf::epoch { ms });
    std::mt19937_64 random { 42 };
    std::uniform_real_distribution<double> anywhere { offset - 1.1 * int64_limit_ms,
        offset + 1.1 * int64_limit_ms };
    std::uniform_real_distribution<double> this_century { offset, offset + 3.2e12 };
    for (int i = 0; i < 100'000; ++i)
    {
        values.push_back(cdf::epoch { anywhere(random) });
        values.push_back(cdf::epoch { this_century(random) });
    }
    return values;
}
}

TEST_CASE("CDF_EPOCH SIMD conversion matches the scalar one bit for bit", "")
{
    const auto values = epochs_to_compare();
    std::vector<int64_t> expected(std::size(values));
    cdf::_impl::scalar_to_ns_from_1970(values, expected.data());
    for (std::size_t start = 0; start < 8; ++start)
    {
        const std::span<const cdf::epoch> input { values.data() + start, std::size(values) - start };
        std::vector<int64_t> output(std::size(input));
        vectorized_to_ns_from_1970(input, output.data());
        std::size_t mismatches = 0;
        for (std::size_t i = 0; i < std::size(output); ++i)
            mismatches += output[i] != expected[start + i];
        REQUIRE(mismatches == 0);
    }
    for (std::size_t length = 0; length < 40; ++length)
    {
        std::vector<int64_t> output(length);
        vectorized_to_ns_from_1970(std::span<const cdf::epoch> { values.data(), length },
            output.data());
        REQUIRE(std::equal(output.begin(), output.end(), expected.begin()));
    }
}
#endif

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

TEST_CASE("epoch/epoch16/tt2000_t repr for pre-1970 dates", "")
{
    // 1958-01-01T00:00:00Z: a pre-1970 date (negative time_t once converted).
    // fmt::format's chrono formatter delegates to the platform's gmtime() to build the
    // y/m/d/h/m/s breakdown - glibc's gmtime_r happily handles negative time_t, but the
    // Windows CRT's gmtime/gmtime_s rejects it outright and fmt throws
    // format_error("time_t value out of range"). VALIDMIN/FILLVAL metadata and
    // pre-space-age mission data routinely carry such pre-1970 epoch/tt2000/epoch16
    // values, so repr()/str() crashed on Windows while working fine on Linux. The fix
    // replaced that path with a pure-arithmetic civil calendar conversion that never
    // touches libc, so this string is now identical on every platform.
    using namespace std::chrono;
    const auto tp = time_point<system_clock> {} - seconds(378691200);
    const std::string expected = "1958-01-01T00:00:00.000000000";

    SECTION("epoch") { REQUIRE([&] {
        std::ostringstream oss;
        oss << cdf::to_cdf_time<cdf::epoch>(tp);
        return oss.str();
    }() == expected); }

    SECTION("epoch16") { REQUIRE([&] {
        std::ostringstream oss;
        oss << cdf::to_cdf_time<cdf::epoch16>(tp);
        return oss.str();
    }() == expected); }

    SECTION("tt2000_t") { REQUIRE([&] {
        std::ostringstream oss;
        oss << cdf::to_cdf_time<cdf::tt2000_t>(tp);
        return oss.str();
    }() == expected); }
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
