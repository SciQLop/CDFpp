#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "cdfpp/attribute.hpp"
#include "cdfpp/cdf-file.hpp"
#include "cdfpp/cdf-io/cdf-io.hpp"
#include "cdfpp/cdf-repr.hpp"
#include "tests_config.hpp"

using namespace cdf;

namespace
{
template <typename T>
std::string repr(const T& value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

struct expected_case
{
    std::string label;
    std::optional<std::string> iso; // nullopt: no known-correct string, just must not throw
};

template <typename T>
void check_cases(const VariableAttribute& attr, const std::vector<expected_case>& cases)
{
    const auto& values = attr.get<T>();
    REQUIRE(std::size(values) == std::size(cases));
    for (std::size_t i = 0; i < std::size(cases); ++i)
    {
        INFO("case: " << cases[i].label);
        std::string out;
        REQUIRE_NOTHROW(out = repr(values[i]));
        if (cases[i].iso)
        {
            REQUIRE(out == *cases[i].iso);
        }
    }
}

void check_attr_no_throw_and_known(
    const VariableAttribute& attr, const std::optional<std::string>& expected)
{
    std::string out;
    REQUIRE_NOTHROW(out = repr(attr.get<epoch>()[0]));
    if (expected)
        REQUIRE(out == *expected);
}
}

// Real-world trigger: a JASON3 CDF's Epoch variable had VALIDMIN=1950-01-01 (a genuine
// pre-1970 date) as a plain attribute. str() on it threw
// "RuntimeError: time_t value out of range" on Windows only - the pre-fix repr code
// formatted via fmt::format(), which delegates to the platform's gmtime(); glibc accepts
// negative time_t, the Windows CRT rejects it outright. This fixture
// (tests/resources/make_pathological_time_cdf.py) bundles every known-or-imagined value
// that could hit that class of bug (and a few that hit an adjacent, still-open
// int64-overflow bug in cdf::to_time_point()) so every platform's CI catches a
// regression here, not just Windows.
SCENARIO("repr() never throws for any known-or-imagined pathological time value",
    "[pathological_time_repr]")
{
    const std::string path = std::string(DATA_PATH) + "/pathological_times.cdf";
    auto cd_opt = cdf::io::load(path);
    REQUIRE(cd_opt != std::nullopt);
    auto cd = *cd_opt;

    GIVEN("the FILLVAL/VALIDMIN/VALIDMAX attributes on each time variable - the exact shape "
          "that crashed on the real file (a plain attribute entry on the time variable itself)")
    {
        THEN("Epoch's are safe, and the well-defined ones match the expected ISO string")
        {
            auto& var = cd.variables["Epoch"];
            check_attr_no_throw_and_known(var.attributes["FILLVAL"], "9999-12-31T23:59:59.999");
            check_attr_no_throw_and_known(
                var.attributes["VALIDMIN"], "1950-01-01T00:00:00.000000000");
            check_attr_no_throw_and_known(
                var.attributes["VALIDMAX"], "2100-12-31T23:59:59.999000000");
        }
        THEN("Epoch16's and TT2000's are safe too")
        {
            auto& e16 = cd.variables["Epoch16"];
            REQUIRE_NOTHROW(repr(e16.attributes["FILLVAL"].get<epoch16>()[0]));
            REQUIRE_NOTHROW(repr(e16.attributes["VALIDMIN"].get<epoch16>()[0]));
            REQUIRE_NOTHROW(repr(e16.attributes["VALIDMAX"].get<epoch16>()[0]));

            auto& tt = cd.variables["TT2000"];
            REQUIRE_NOTHROW(repr(tt.attributes["FILLVAL"].get<tt2000_t>()[0]));
            REQUIRE_NOTHROW(repr(tt.attributes["VALIDMIN"].get<tt2000_t>()[0]));
            REQUIRE_NOTHROW(repr(tt.attributes["VALIDMAX"].get<tt2000_t>()[0]));
        }
    }

    GIVEN("every pathological CDF_EPOCH case")
    {
        THEN("none throw, and the platform-independent ones match exactly")
        {
            check_cases<epoch>(cd.variables["Epoch"].attributes["PATHOLOGICAL_CASES"],
                {
                    { "FILL", "9999-12-31T23:59:59.999" },
                    { "PAD_YEAR0", "0000-01-01T00:00:00.000" },
                    { "PRE_1970", "1950-01-01T00:00:00.000000000" },
                    { "SAFE_2100", "2100-12-31T23:59:59.999000000" },
                    { "NEAR_FILL_NOT_EXACT", std::nullopt }, // overflows to a HW-dependent value
                    { "GENUINE_9999_NON_SENTINEL", std::nullopt }, // ditto
                    { "NAN", std::nullopt }, // float->int64 "invalid" conversion is HW-dependent
                    { "DBL_MAX", std::nullopt }, // ditto
                });
        }
    }

    GIVEN("every pathological CDF_EPOCH16 case")
    {
        THEN("none throw, and the platform-independent ones match exactly")
        {
            check_cases<epoch16>(cd.variables["Epoch16"].attributes["PATHOLOGICAL_CASES"],
                {
                    { "FILL", "9999-12-31T23:59:59.999999999" },
                    { "PAD_YEAR0", "0000-01-01T00:00:00.000000000000" },
                    { "PRE_1970", "1950-01-01T00:00:00.000000000" },
                    { "SAFE_2100", "2100-12-31T23:59:59.999000000" },
                    { "PARTIAL_FILL_MISMATCH", std::nullopt }, // seconds==-1e31 but ps!=-1e31:
                                                                // misses the exact-match sentinel
                                                                // check, overflows instead
                    { "GENUINE_9999_NON_SENTINEL", std::nullopt },
                    { "NAN", std::nullopt },
                    { "DBL_MAX", std::nullopt },
                });
        }
    }

    GIVEN("every pathological CDF_TIME_TT2000 case")
    {
        THEN("none throw, and the platform-independent ones match exactly")
        {
            check_cases<tt2000_t>(cd.variables["TT2000"].attributes["PATHOLOGICAL_CASES"],
                {
                    { "FILL", "9999-12-31T23:59:59.999999999" },
                    { "PADVALUE", "0000-01-01T00:00:00.000000000" },
                    { "MYSTERY_ALIAS", "9999-12-31T23:59:59.999999999" },
                    { "UNHANDLED_GAP", std::nullopt }, // INT64_MIN+2: not one of the three
                                                        // recognized sentinel bit patterns
                    { "PRE_1970", "1950-01-01T00:00:00.000000000" },
                    { "SAFE_2100", "2100-12-31T23:59:59.999000000" },
                    { "INT64_MAX", std::nullopt }, // nseconds + tt2000_offset overflows int64
                });
        }
    }
}
