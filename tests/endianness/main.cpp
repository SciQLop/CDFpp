#if __has_include(<catch2/catch_all.hpp>)
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#else
#include <catch.hpp>
#endif
#include "cdfpp/cdf-io/endianness.hpp"
#include <cstdint>


TEST_CASE("", "")
{
    using namespace cdf::endianness;
    REQUIRE(0x01 == decode<big_endian_t, uint8_t>("\1"));
    REQUIRE(0x0102 == decode<big_endian_t, uint16_t>("\1\2"));
    REQUIRE(0x01020304 == decode<big_endian_t, uint32_t>("\1\2\3\4"));
    REQUIRE(0x0102030405060701 == decode<big_endian_t, uint64_t>("\1\2\3\4\5\6\7\1"));
}
