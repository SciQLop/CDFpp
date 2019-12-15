#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <cdf-io.hpp>
#include <cdf.hpp>
#include <iostream>


SCENARIO("Loading a cdf file", "[CDF]")
{
    GIVEN("a cdf file")
    {
        WHEN("file doesn't exist") { REQUIRE(cdf::io::load("wrongfile.cdf") == std::nullopt); }
        WHEN("file exists but isn't a cdf file")
        {
            auto path = std::string(DATA_PATH) + "/not_a_cdf.cdf";
            REQUIRE(std::filesystem::exists(path));
            REQUIRE(cdf::io::load(path) == std::nullopt);
        }
        WHEN("file exists and is a cdf file")
        {
            auto path = std::string(DATA_PATH) + "/a_cdf.cdf";
            REQUIRE(std::filesystem::exists(path));
            REQUIRE(cdf::io::load(path) != std::nullopt);
        }
    }
}
