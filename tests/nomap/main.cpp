#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>


#if __has_include(<catch2/catch_all.hpp>)
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#else
#include <catch.hpp>
#endif


#include "cdfpp/nomap.hpp"
#include "cdfpp/variable.hpp"

#include "tests_config.hpp"


SCENARIO("nomap", "[CDF]")
{
    GIVEN("a map")
    {
        nomap<std::string,cdf::Variable> map;
        WHEN("the map is empty")
        {
            THEN("its size must be 0")
            {
                REQUIRE(std::size(map)==0);
            }
            THEN("empty must return true")
            {
                REQUIRE(map.empty());
            }
        }
        WHEN("adding some items")
        {
            auto var = cdf::Variable("Var1",0,cdf::data_t{},{0,1},cdf::cdf_majority::row, false);
            map["Var1"]=std::move(var);
            THEN("its size must be equals to the number of items added")
            {
                REQUIRE(std::size(map)==1);
            }
            THEN("empty must return false")
            {
                REQUIRE(!map.empty());
            }
        }
    }
}
