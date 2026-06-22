#include <optional>
#include <string>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cdfpp/cdf-file.hpp"
#include "cdfpp/cdf-io/cdf-io.hpp"

#include "tests_config.hpp"

using namespace cdf;

namespace
{
CDF load_fixture(const std::string& name, bool lazy_load)
{
    auto opt = io::load(std::string(DATA_PATH) + "/" + name, true, lazy_load);
    REQUIRE(opt != std::nullopt);
    return std::move(*opt);
}
}

SCENARIO("Variable.is_zvariable distinguishes zVariables from legacy rVariables", "[introspection]")
{
    GIVEN("a CDF containing only zVariables")
    {
        auto cd = load_fixture("contiguous.cdf", true);
        THEN("its variables report is_zvariable == true")
        {
            REQUIRE(cd["whole_zvar"].is_zvariable());
        }
    }
    GIVEN("a CDF containing a legacy rVariable")
    {
        auto cd = load_fixture("rvariable.cdf", true);
        THEN("the rVariable reports is_zvariable == false")
        {
            REQUIRE_FALSE(cd["legacy_rvar"].is_zvariable());
        }
    }
}

SCENARIO("Variable.is_contiguous() detects fragmented variable record blocks", "[introspection]")
{
    GIVEN("a zVariable written in a single VVR block")
    {
        auto cd = load_fixture("contiguous.cdf", true);
        THEN("it reports is_contiguous() == true")
        {
            REQUIRE(cd["whole_zvar"].is_contiguous());
        }
    }
    GIVEN("a variable whose contiguous records span two VVR blocks")
    {
        auto cd = load_fixture("fragmented.cdf", true);
        THEN("the split variable reports is_contiguous() == false")
        {
            REQUIRE_FALSE(cd["split_zvar"].is_contiguous());
        }
        THEN("a single-block variable in the same file reports is_contiguous() == true")
        {
            REQUIRE(cd["filler"].is_contiguous());
        }
    }
    GIVEN("a legacy rVariable stored in a single block")
    {
        auto cd = load_fixture("rvariable.cdf", true);
        THEN("it reports is_contiguous() == true")
        {
            REQUIRE(cd["legacy_rvar"].is_contiguous());
        }
    }
    GIVEN("the fragmented file loaded eagerly")
    {
        auto cd = load_fixture("fragmented.cdf", false);
        THEN("is_contiguous() gives the same answer as in lazy mode")
        {
            REQUIRE_FALSE(cd["split_zvar"].is_contiguous());
            REQUIRE(cd["filler"].is_contiguous());
        }
    }
}
