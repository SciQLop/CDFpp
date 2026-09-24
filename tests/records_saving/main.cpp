#include <algorithm>
#include <optional>
#include <stdint.h>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>


#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>


#include "cdfpp/cdf-io/saving/link_records.hpp"
#include "cdfpp/cdf-io/saving/records-saving.hpp"
#include <cpp_utils/serde/serde.hpp>

using cpp_utils::serde::bounded_string;
using cpp_utils::serde::dynamic_array;

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
            bounded_string<8> c;
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
            dynamic_array<0, uint16_t> c;
            uint64_t d;
            dynamic_array<1, uint32_t> e;

            std::size_t field_size(const dynamic_array<0, uint16_t>&) const
            {
                return this->a;
            }
            std::size_t field_size(const dynamic_array<1, uint32_t>&) const
            {
                return 2;
            }
        };
        THEN("we can load it from a buffer")
        {
            // record_size (unlike load) sums each dynamic_array's OWN current
            // .size(), not field_size() — matching how CDFpp actually saves:
            // create_records.hpp always resizes an array to its intended element
            // count before update_size()/save_record() ever runs, so field_size()
            // only needs to be consulted on the load path (deserialize's
            // resolve_field_size). Populate `e` to the 2 elements field_size()
            // would have implied, matching real usage.
            record_table_fields s{0,0.,{},0,{}};
            s.e.resize(2);
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

// Files over 2 GiB put records past offset 2^31: linking must keep full 64-bit offsets.
// (A 32-bit accumulator used to wrap them negative, corrupting every chain past 2 GiB.)
TEST_CASE("Record chains keep offsets beyond 2 GiB and 4 GiB", "[saving]")
{
    using namespace cdf::io;
    using namespace cdf::io::saving;
    constexpr std::size_t past_2GiB = (std::size_t { 1 } << 31) + 1000;
    constexpr std::size_t past_4GiB = (std::size_t { 1 } << 32) + 2000;

    saving_context ctx {};
    for (auto offset : { past_2GiB, past_4GiB })
    {
        auto& vc = ctx.body.variables.emplace_back();
        vc.vdr.offset = offset;
        auto& vxr = vc.vxrs.emplace_back();
        vxr.offset = offset + 100;
        vxr.record.Offset.resize(1);
        record_wrapper<cdf_VVR_t<v3x_tag>> vvr {};
        vvr.offset = offset + 200;
        vc.values_records.emplace_back(std::move(vvr));
    }
    auto& vac = ctx.body.variable_attributes["attr"];
    vac.adr.offset = past_2GiB + 300;
    for (auto offset : { past_2GiB + 400, past_4GiB + 400 })
        vac.aedrs.emplace_back().offset = offset;
    auto& vac2 = ctx.body.variable_attributes["attr2"];
    vac2.adr.offset = past_4GiB + 300;

    link_vdrs(ctx);
    link_adrs(ctx);

    const auto& vars = ctx.body.variables;
    REQUIRE(vars[0].vdr.record.VDRnext == static_cast<int64_t>(past_4GiB));
    REQUIRE(vars[0].vdr.record.VXRhead == static_cast<int64_t>(past_2GiB + 100));
    REQUIRE(vars[1].vxrs[0].record.Offset[0] == static_cast<int64_t>(past_4GiB + 200));
    REQUIRE(ctx.body.variable_attributes["attr"].adr.record.ADRnext
        == static_cast<int64_t>(past_4GiB + 300));
    REQUIRE(ctx.body.variable_attributes["attr"].adr.record.AzEDRhead
        == static_cast<int64_t>(past_2GiB + 400));
    REQUIRE(ctx.body.variable_attributes["attr"].aedrs[0].record.AEDRnext
        == static_cast<int64_t>(past_4GiB + 400));
}
