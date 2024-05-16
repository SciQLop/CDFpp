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


#include "cdfpp/cdf-io/loading/records-loading.hpp"
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
            std::string buffer { "cd" };
            cdf::io::load_record(s, buffer.c_str(), 0);
            REQUIRE(s.a == 'c');
            REQUIRE(s.b == 'd');
        }
    }
    GIVEN("a simple two char fields record with an unused field")
    {
        struct two_chars
        {
            char a;
            unused_field<char> b;
        };
        two_chars s { 'a', 'b' };
        THEN("we can load it from a buffer")
        {
            std::string buffer { "cd" };
            cdf::io::load_record(s, buffer.c_str(), 0);
            REQUIRE(s.a == 'c');
            REQUIRE(s.b.value == 'b');
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
            std::array<char, 21> buffer { 0x2A, 0x40, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A };
            cdf::io::load_record(s, buffer.data(), 0);
            REQUIRE(s.a == 42);
            REQUIRE(s.b == 42.);
            REQUIRE(s.c == 42);
            REQUIRE(s.d == 42);
        }
        THEN("we can load it from a buffer with an offset")
        {
            complex_record s { 0, 0., 0, 0 };
            std::array<char, 22> buffer { 0x11, 0x2A, 0x40, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A };
            cdf::io::load_record(s, buffer.data(), 1);
            REQUIRE(s.a == 42);
            REQUIRE(s.b == 42.);
            REQUIRE(s.c == 42);
            REQUIRE(s.d == 42);
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
            std::array<char, 6> buffer { 0x2A, 0x0, 0x2A, 0x0, 0x2A, 0x2A };
            cdf::io::load_record(s, buffer.data(), 0);
            REQUIRE(s.a == 42);
            REQUIRE(s.b.a == 42);
            REQUIRE(s.b.b == 42);
            REQUIRE(s.c == 42);
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
            std::array<char, 25> buffer { 0x2A, 0x40, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'h',
                'e', 'l', 'l', 'o', 0, 0, 0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A };
            cdf::io::load_record(s, buffer.data(), 0);
            REQUIRE(s.a == 42);
            REQUIRE(s.b == 42.);
            REQUIRE(s.c.value == "hello");
            REQUIRE(s.d == 42);
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
            record_table_fields s;
            std::array<char, 33> buffer { 0x4, 0x40, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0,
                0x01, 0x0, 0x2, 0x0, 0x3, 0x0, 0x4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A,
                0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02 };
            cdf::io::load_record(s, buffer.data(), 0);
            REQUIRE(s.a == 4);
            REQUIRE(s.b == 42.);
            REQUIRE(s.c.values == std::vector<uint16_t> { 1, 2, 3, 4 });
            REQUIRE(s.d == 42);
            REQUIRE(s.e.values == no_init_vector<uint32_t> { 1, 2 });
            static_assert(count_members<decltype(s)> == 5);
        }
    }
}
