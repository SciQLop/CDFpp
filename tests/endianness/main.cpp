#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <cdf-endianness.hpp>
#include <cstdint>
#include <iostream>


TEST_CASE("", "")
{
    using namespace cdf::endianness;
    REQUIRE(0x01 == read<uint8_t>("\1"));
    REQUIRE(0x0102 == read<uint16_t>("\1\2"));
    REQUIRE(0x01020304 == read<uint32_t>("\1\2\3\4"));
    REQUIRE(0x0102030405060701 == read<uint64_t>("\1\2\3\4\5\6\7\1"));
}
