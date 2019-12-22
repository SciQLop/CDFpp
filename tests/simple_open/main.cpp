#define CATCH_CONFIG_MAIN
#if __has_include(<catch2/catch.hpp>)
#include <catch2/catch.hpp>
#else
#include <catch.hpp>
#endif
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
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            REQUIRE(cd.attributes.find("attr") != cd.attributes.cend());
            REQUIRE(cd.attributes["attr"].get<std::string>(0) == "a cdf text attribute");
            REQUIRE(cd.attributes.find("attr_float") != cd.attributes.cend());
            REQUIRE(cd.attributes["attr_float"].get<std::vector<float>>(0)
                == std::vector<float> { 1.f});
            REQUIRE(cd.attributes["attr_float"].get<std::vector<float>>(1)
                == std::vector<float> { 2.f});
            REQUIRE(cd.attributes["attr_float"].get<std::vector<float>>(2)
                == std::vector<float> { 3.f});
            REQUIRE(cd.attributes.find("attr_int") != cd.attributes.cend());
            REQUIRE(cd.attributes["attr_int"].get<std::vector<int8_t>>(0) == std::vector<int8_t>{1});
            REQUIRE(cd.attributes["attr_int"].get<std::vector<int8_t>>(1) == std::vector<int8_t>{2});
            REQUIRE(cd.attributes["attr_int"].get<std::vector<int8_t>>(2) == std::vector<int8_t>{3});

        }
    }
}
