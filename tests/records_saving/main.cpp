#include <algorithm>
#include <optional>
#include <stdint.h>
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


#include "cdfpp/cdf-io/saving/records-saving.hpp"
#include "cdfpp/cdf-io/special-fields.hpp"

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
            REQUIRE(cdf::io::record_size(s)==2);
            static_assert(cdf::io::record_size(s)==2);
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
        THEN("we can load it from a buffer")
        {
            complex_record s { 0, 0., 0, 0 };
            REQUIRE(cdf::io::record_size(s)==21);
            static_assert(cdf::io::record_size(s)==21);
        }
    }
    GIVEN("a record with nested record")
    {
        struct inner_record
        {
            uint16_t a;
            uint16_t b;
        };
        struct outer_record
        {
            char a;
            inner_record b;
            char c;
        };
        THEN("we can load it from a buffer")
        {
            outer_record s { 0, { 0, 0 }, 0 };
            REQUIRE(cdf::io::record_size(s)==6);
            static_assert(cdf::io::record_size(s)==6);
        }
    }
    GIVEN("a record with string fields")
    {
        struct record_with_string
        {
            char a;
            double b;
            string_field<8> c;
            uint64_t d;
        };
        THEN("we can load it from a buffer")
        {
            record_with_string s { 0, 0., { "" }, 0 };

            REQUIRE(cdf::io::record_size(s)==25);
            static_assert(cdf::io::record_size(s)==25);
        }
    }
    GIVEN("a record with table fields")
    {
        struct record_table_fields
        {
            char a;
            double b;
            table_field<uint16_t, 0> c;
            uint64_t d;
            table_field<uint32_t, 1> e;

            std::size_t size(const table_field<uint16_t, 0>&) const
            {
                return this->a * sizeof(uint16_t);
            }
            std::size_t size(const table_field<uint32_t, 1>&) const { return 2 * sizeof(uint32_t); }
        };
        THEN("we can load it from a buffer")
        {
            record_table_fields s{0,0.,{},0,{}};
            REQUIRE(cdf::io::record_size(s)==25);
        }
    }
    GIVEN("a true CDF record")
    {

        THEN("we can load it from a buffer")
        {
            io::cdf_CDR_t<io::v3x_tag> s{};
            static_assert(io::is_cdf_DR_header_v<decltype(s.header)>);
            static_assert(cdf::io::record_size(s)==312);
        }
    }
}
