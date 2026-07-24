#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cdfpp/cdf-io/debug/record_repr.hpp"
#include "cdfpp/cdf-io/debug/record_stream.hpp"
#include "tests_config.hpp"
#include <sstream>
#include <string>

using namespace cdf::io::debug;

SCENARIO("print_record generically prints every field of a real record by name", "[record_repr]")
{
    GIVEN("the CDR of a real v3 fixture")
    {
        const std::string path = std::string(DATA_PATH) + "/a_cdf.cdf";
        std::ostringstream oss;
        bool printed = false;
        for_each_record(
            path,
            [&](std::size_t offset, const auto& record)
            {
                using T = std::decay_t<decltype(record)>;
                if constexpr (std::is_same_v<T, cdf::io::cdf_CDR_t<cdf::io::v3x_tag>>)
                {
                    if (!printed)
                    {
                        print_record(oss, offset, record);
                        printed = true;
                    }
                }
            });
        const std::string out = oss.str();

        THEN("the header line names the record type, offset and size")
        {
            REQUIRE(printed);
            REQUIRE(out.find("CDR") != std::string::npos);
            REQUIRE(out.find("@8") != std::string::npos);
            REQUIRE(out.find("312") != std::string::npos);
        }
        THEN("every real field name appears, purely from reflection - no per-type printer")
        {
            for (const std::string field :
                { "GDRoffset", "Version", "Release", "Encoding", "Flags", "Increment",
                    "Identifier", "copyright" })
            {
                INFO("looking for field: " << field);
                REQUIRE(out.find(field) != std::string::npos);
            }
        }
        THEN("enum fields render their symbolic name, not a bare integer")
        {
            REQUIRE(out.find("IBMPC") != std::string::npos);
        }
        THEN("the string field renders quoted")
        {
            REQUIRE(out.find('"') != std::string::npos);
        }
        THEN("reserved fields are labelled rather than silently dropped")
        {
            REQUIRE(out.find("reserved") != std::string::npos);
        }
        THEN("the header field itself is not printed as a second, opaque, nested block")
        {
            REQUIRE(out.find("header") == std::string::npos);
        }
    }
    GIVEN("a zVDR record, to exercise dynamic-array and bounded-string fields")
    {
        const std::string path = std::string(DATA_PATH) + "/a_cdf.cdf";
        std::ostringstream oss;
        bool printed = false;
        for_each_record(
            path,
            [&](std::size_t offset, const auto& record)
            {
                using T = std::decay_t<decltype(record)>;
                if constexpr (std::is_same_v<T, cdf::io::cdf_zVDR_t<cdf::io::v3x_tag>>)
                {
                    if (!printed)
                    {
                        print_record(oss, offset, record);
                        printed = true;
                    }
                }
            });
        const std::string out = oss.str();

        THEN("it printed without throwing and named the record")
        {
            REQUIRE(printed);
            REQUIRE(out.find("zVDR") != std::string::npos);
            REQUIRE(out.find("Name") != std::string::npos);
        }
        THEN("dynamic-array fields render as a bracketed list, not raw container internals")
        {
            REQUIRE(out.find("DimVarys") != std::string::npos);
            REQUIRE(out.find('[') != std::string::npos);
            REQUIRE(out.find(']') != std::string::npos);
        }
    }
    GIVEN("an undecoded (SPR/UIR) record placeholder")
    {
        undecoded_record_t r { cdf::cdf_record_type::UIR, 42 };
        std::ostringstream oss;
        THEN("it prints a graceful, honest placeholder rather than failing to compile/run")
        {
            print_record(oss, 12345, r);
            const std::string out = oss.str();
            REQUIRE(out.find("UIR") != std::string::npos);
            REQUIRE(out.find("42") != std::string::npos);
            REQUIRE(out.find("not decoded") != std::string::npos);
        }
    }
}
