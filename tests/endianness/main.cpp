#define CATCH_CONFIG_MAIN
#if __has_include(<catch2/catch.hpp>)
#include <catch2/catch.hpp>
#else
#include <catch.hpp>
#endif
#include <cdf-endianness.hpp>
#include <cstdint>
#include <iostream>


TEST_CASE("", "")
{
    using namespace cdf::endianness;
    REQUIRE(0x01 == decode<uint8_t>("\1"));
    REQUIRE(0x0102 == decode<uint16_t>("\1\2"));
    REQUIRE(0x01020304 == decode<uint32_t>("\1\2\3\4"));
    REQUIRE(0x0102030405060701 == decode<uint64_t>("\1\2\3\4\5\6\7\1"));
}
