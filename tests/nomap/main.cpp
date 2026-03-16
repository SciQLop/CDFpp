#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>


#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>



#include "cdfpp/nomap.hpp"
#include "cdfpp/variable.hpp"

#include "tests_config.hpp"


SCENARIO("nomap_node swap preserves keys and values", "[CDF][nomap]")
{
    GIVEN("two nomap_nodes with distinct keys and values")
    {
        nomap_node<int, int> a(10, 1);
        nomap_node<int, int> b(20, 2);

        WHEN("they are swapped")
        {
            swap(a, b);

            THEN("keys are exchanged correctly")
            {
                REQUIRE(a.first == 20);
                REQUIRE(b.first == 10);
            }
            THEN("values are exchanged correctly")
            {
                REQUIRE(a.second == 2);
                REQUIRE(b.second == 1);
            }
        }
    }
}

SCENARIO("nomap erase by const_iterator works correctly", "[CDF][nomap]")
{
    GIVEN("a nomap with three entries")
    {
        nomap<std::string, int> map;
        map["a"] = 1;
        map["b"] = 2;
        map["c"] = 3;

        WHEN("erasing via const_iterator")
        {
            auto cit = map.cbegin();
            auto erased_key = cit->first;
            map.erase(cit);

            THEN("size decreases")
            {
                REQUIRE(map.size() == 2);
            }
            THEN("erased key is no longer found")
            {
                REQUIRE(map.find(erased_key) == map.end());
            }
        }
    }
}

SCENARIO("nomap operator== is symmetric", "[CDF][nomap]")
{
    GIVEN("two maps where one has an extra key")
    {
        nomap<std::string, int> small_map;
        small_map["a"] = 1;

        nomap<std::string, int> big_map;
        big_map["a"] = 1;
        big_map["b"] = 2;

        THEN("they must not be equal in either direction")
        {
            REQUIRE_FALSE(small_map == big_map);
            REQUIRE_FALSE(big_map == small_map);
        }
    }
}

SCENARIO("nomap erase(begin, end) clears the map", "[CDF][nomap]")
{
    GIVEN("a nomap with three entries")
    {
        nomap<std::string, int> map;
        map["a"] = 1;
        map["b"] = 2;
        map["c"] = 3;

        WHEN("erasing the full range erase(begin(), end())")
        {
            map.erase(map.cbegin(), map.cend());

            THEN("the map is empty")
            {
                REQUIRE(map.size() == 0);
                REQUIRE(map.empty());
            }
        }

        WHEN("erasing a partial range ending at end()")
        {
            map.erase(map.cbegin() + 1, map.cend());

            THEN("only the first element remains")
            {
                REQUIRE(map.size() == 1);
            }
        }
    }
}

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
