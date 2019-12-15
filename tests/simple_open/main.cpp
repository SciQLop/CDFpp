#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <cdf-io.hpp>
#include <cdf.hpp>
#include <iostream>


SCENARIO("Loading a cdf file", "[CDF]")
{
    GIVEN("a cdf file")
    {
        WHEN("file doesn't exists") { REQUIRE(cdf::io::load("wrongfile.cdf") == std::nullopt); }
    }
}
