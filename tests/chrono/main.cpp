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


namespace
{
constexpr int64_t nat = std::numeric_limits<int64_t>::min();
constexpr double nan_value = std::numeric_limits<double>::quiet_NaN();
constexpr double inf = std::numeric_limits<double>::infinity();
// The range of int64 nanoseconds since 1970 (1677-09-21 to 2262-04-11), as CDF_EPOCH ms.
constexpr double epoch_min_ms = 52943847163145.2265625;
constexpr double epoch_max_ms = 71390591236854.765625;
constexpr int64_t last_representable_tt2000 = std::numeric_limits<int64_t>::max()
    - (cdf::constants::tt2000_offset
        - cdf::chrono::leap_seconds::leap_seconds_tt2000_reverse.back().second);

template <typename time_t>
std::vector<int64_t> converted(const std::vector<time_t>& values)
{
    std::vector<int64_t> output(std::size(values));
    cdf::to_ns_from_1970(std::span<const time_t> { values }, output.data());
    return output;
}

// Converts each value alone (scalar path), then all of them at once, 16 copies each (SIMD).
template <typename time_t>
void require_nat(const std::vector<time_t>& values)
{
    for (const auto& value : values)
    {
        REQUIRE(converted(std::vector { value }) == std::vector { nat });
        const auto many = converted(std::vector<time_t>(16, value));
        REQUIRE(std::ranges::all_of(many, [](int64_t ns) { return ns == nat; }));
    }
}

#ifdef __SIZEOF_INT128__
// floor((ms - offset) * 1e6) in exact integer arithmetic: ms = mantissa * 2^exponent.
int64_t exact_epoch_ns(double ms)
{
    int exponent = 0;
    const double fraction = std::frexp(ms, &exponent);
    const auto mantissa = static_cast<__int128>(std::ldexp(fraction, 53));
    const int shift = 53 - exponent;
    REQUIRE(shift > 0);
    const __int128 offset = static_cast<__int128>(cdf::constants::epoch_offset_miliseconds);
    const __int128 scaled = (mantissa - (offset << shift)) * 1'000'000;
    return static_cast<int64_t>(scaled >> shift); // arithmetic shift: floor
}
#endif
}

TEST_CASE("Time values without a representable date become NaT", "")
{
    require_nat(std::vector<cdf::epoch> { { -1e31 }, { 0.0 }, { nan_value }, { inf }, { -inf },
        { std::nextafter(epoch_min_ms, 0.0) }, { std::nextafter(epoch_max_ms, inf) } });
    require_nat(std::vector<cdf::epoch16> { { -1e31, -1e31 }, { 0.0, 0.0 }, { nan_value, 0.0 },
        { 63745056000.0, nan_value }, { 63745056000.0, -1.0 }, { 63745056000.0, 1e12 },
        { inf, 0.0 }, { cdf::constants::epoch_offset_seconds - 9223372037.0, 0.0 },
        { cdf::constants::epoch_offset_seconds + 9223372036.0, 0.0 } });
    // NASA's three special values (fill, pad, illegal: see SciQLop/CDFpp#19), then dates after
    // 2262-04-11, out of int64 ns since 1970.
    require_nat(std::vector<cdf::tt2000_t> { { nat }, { nat + 1 }, { nat + 3 },
        { std::numeric_limits<int64_t>::max() }, { last_representable_tt2000 + 1 } });
    // INT64_MIN + 2 is a real date for NASA's library: 1707-09-22T12:12:10.961224194.
    REQUIRE(converted(std::vector<cdf::tt2000_t> { { nat + 2 } })
        == std::vector<int64_t> { -8276644069038775806 });
    REQUIRE(converted(std::vector<cdf::tt2000_t> { { last_representable_tt2000 } })
        == std::vector { std::numeric_limits<int64_t>::max() });
}

TEST_CASE("CDF_EPOCH conversion is exact", "")
{
    // 2020-01-01 00:00:00.0685: exact in a double, it used to become .068499968.
    REQUIRE(converted(std::vector<cdf::epoch> { { 63745056000068.5 } })
        == std::vector<int64_t> { 1577836800068500000 });
    REQUIRE(converted(std::vector<cdf::epoch> { { epoch_min_ms } }).front() != nat);
    REQUIRE(converted(std::vector<cdf::epoch> { { epoch_max_ms } }).front() != nat);
#ifdef __SIZEOF_INT128__
    std::mt19937_64 random { 7 };
    std::uniform_real_distribution<double> representable { epoch_min_ms, epoch_max_ms };
    std::vector<cdf::epoch> values { { epoch_min_ms }, { epoch_max_ms },
        { cdf::constants::epoch_offset_miliseconds }, { 62040988800123.25 } };
    for (int i = 0; i < 100'000; ++i)
        values.push_back({ representable(random) });
    const auto output = converted(values);
    std::size_t mismatches = 0;
    for (std::size_t i = 0; i < std::size(values); ++i)
        mismatches += output[i] != exact_epoch_ns(values[i].mseconds);
    REQUIRE(mismatches == 0);
#endif
}

#ifndef CDFPP_NO_SIMD
namespace
{
std::vector<cdf::epoch> epochs_to_compare()
{
    std::vector<cdf::epoch> values;
    for (double ms : { 0.0, -1e31, 1e31, nan_value, inf, -inf, epoch_min_ms, epoch_max_ms,
             std::nextafter(epoch_min_ms, 0.0), std::nextafter(epoch_max_ms, inf),
             cdf::constants::epoch_offset_miliseconds, 63745056000068.5, 62040988800123.25 })
        values.push_back({ ms });
    std::mt19937_64 random { 42 };
    std::uniform_real_distribution<double> anywhere { 0.5 * epoch_min_ms, 1.5 * epoch_max_ms };
    for (int i = 0; i < 100'000; ++i)
        values.push_back({ anywhere(random) });
    return values;
}

std::vector<cdf::epoch16> epoch16s_to_compare()
{
    constexpr double offset = cdf::constants::epoch_offset_seconds;
    std::vector<cdf::epoch16> values { { -1e31, -1e31 }, { 0.0, 0.0 }, { nan_value, 0.0 },
        { 63745056000.0, nan_value }, { 63745056000.0, -1.0 }, { 63745056000.0, 1e12 },
        { offset - 9223372036.0, 0.0 }, { offset + 9223372035.0, 999999999999.0 },
        { offset - 9223372037.0, 999999999999.0 }, { offset + 9223372036.0, 0.0 } };
    std::mt19937_64 random { 43 };
    std::uniform_int_distribution<int64_t> seconds { -10'000'000'000, 10'000'000'000 };
    std::uniform_int_distribution<int64_t> picoseconds { 0, 999'999'999'999 };
    for (int i = 0; i < 100'000; ++i)
        values.push_back({ offset + static_cast<double>(seconds(random)),
            static_cast<double>(picoseconds(random)) });
    return values;
}

std::vector<cdf::tt2000_t> tt2000s_to_compare()
{
    // Sorted recent values take a faster SIMD path than mixed ones: test both.
    std::vector<cdf::tt2000_t> values;
    std::mt19937_64 random { 44 };
    std::uniform_int_distribution<int64_t> recent { 631108869184000000,
        std::numeric_limits<int64_t>::max() };
    for (int i = 0; i < 1000; ++i)
        values.push_back({ recent(random) });
    std::ranges::sort(values, {}, &cdf::tt2000_t::nseconds);
    for (int64_t ns : { nat, nat + 1, int64_t { 0 }, std::numeric_limits<int64_t>::max() })
        values.push_back({ ns });
    std::uniform_int_distribution<int64_t> anywhere { nat, std::numeric_limits<int64_t>::max() };
    for (int i = 0; i < 100'000; ++i)
        values.push_back({ anywhere(random) });
    return values;
}

// Every start offset (alignment) and short lengths (the scalar tail) too.
template <typename time_t>
void require_simd_matches_scalar(const std::vector<time_t>& values)
{
    std::vector<int64_t> expected(std::size(values));
    cdf::_impl::scalar_to_ns_from_1970(std::span<const time_t> { values }, expected.data());
    for (std::size_t start = 0; start < 8; ++start)
    {
        const std::span<const time_t> input { values.data() + start, std::size(values) - start };
        std::vector<int64_t> output(std::size(input));
        vectorized_to_ns_from_1970(input, output.data());
        REQUIRE(std::equal(output.begin(), output.end(), expected.begin() + start));
    }
    for (std::size_t length = 0; length < 40; ++length)
    {
        std::vector<int64_t> output(length);
        vectorized_to_ns_from_1970(std::span<const time_t> { values.data(), length }, output.data());
        REQUIRE(std::equal(output.begin(), output.end(), expected.begin()));
    }
}
}

TEST_CASE("SIMD time conversions match the scalar ones bit for bit", "")
{
    require_simd_matches_scalar(epochs_to_compare());
    require_simd_matches_scalar(epoch16s_to_compare());
    require_simd_matches_scalar(tt2000s_to_compare());
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

TEST_CASE("TT2000 values print like NASA's library", "")
{
    // NASA's encodeTT2000 output. Real dates between 1707 and 1739 used to print as 1739.
    const auto printed = [](int64_t ns)
    {
        std::ostringstream oss;
        oss << cdf::tt2000_t { ns };
        return oss.str();
    };
    REQUIRE(printed(nat) == "9999-12-31T23:59:59.999999999");
    REQUIRE(printed(nat + 1) == "0000-01-01T00:00:00.000000000");
    REQUIRE(printed(nat + 2) == "1707-09-22T12:12:10.961224194");
    REQUIRE(printed(nat + 3) == "9999-12-31T23:59:59.999999999");
    REQUIRE(printed(nat + 4) == "1707-09-22T12:12:10.961224196");
    REQUIRE(printed(631108869184000000) == "2020-01-01T00:00:00.000000000");
}

TEST_CASE("Time values print right outside the int64 ns range", "")
{
    // Expected dates from NASA's library (computeEPOCH/EPOCH16/TT2000, libcdf 3.9.0). These
    // used to go through int64 ns since 1970, which stop at 1677 and 2262.
    const auto printed = [](const auto& value)
    {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    };
    REQUIRE(printed(cdf::epoch { 47349750896789.0 }) == "1500-06-15T12:34:56.789000000");
    REQUIRE(printed(cdf::epoch { 31622400000.0 }) == "0001-01-01T00:00:00.000000000");
    REQUIRE(printed(cdf::epoch16 { 94675914123.0, 456789012345.0 })
        == "3000-02-28T01:02:03.456789012");
    REQUIRE(printed(cdf::tt2000_t { 8851933299307456789 }) == "2280-07-04T10:20:30.123456789");
    REQUIRE(printed(cdf::tt2000_t { -9146433567815999999 }) == "1710-03-01T00:00:00.000000001");
    // Not dates: printed like fill values, as datetime objects get the fill date.
    REQUIRE(printed(cdf::epoch { nan_value }) == "9999-12-31T23:59:59.999");
    REQUIRE(printed(cdf::epoch { inf }) == "9999-12-31T23:59:59.999");
    REQUIRE(printed(cdf::epoch16 { nan_value, 0.0 }) == "9999-12-31T23:59:59.999999999");
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
