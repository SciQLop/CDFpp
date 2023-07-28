#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>


#if __has_include(<catch2/catch_all.hpp>)
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#else
#include <catch.hpp>
#endif


#include "cdfpp/cdf-io/cdf-io-loading.hpp"


SCENARIO("record loading", "[CDF]")
{
    GIVEN("a simple two char fields record")
    {
        struct two_chars
        {
            char a;
            char b;
        };
        two_chars s { 'a', 'b' };
        THEN("we can load it from a buffer")
        {
            std::string buffer { "cd" };
            cdf::io::load_record(s, buffer.c_str(), 0);
            REQUIRE(s.a == 'c');
            REQUIRE(s.b == 'd');
        }
    }
    GIVEN("a more complex record")
    {
        struct complex_record
        {
            char a;
            double b;
            uint32_t c;
            uint64_t d;
        };
        complex_record s { 0, 0., 0, 0 };
        THEN("we can load it from a buffer")
        {
            std::string buffer { 0x2A, 0x40, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A };
            cdf::io::load_record(s, buffer.c_str(), 0);
            REQUIRE(s.a == 42);
            REQUIRE(s.b == 42.);
            REQUIRE(s.c == 42);
            REQUIRE(s.d == 42);
        }
    }
}
