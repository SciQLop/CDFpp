#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cdfpp/cdf-io/debug/record_repr.hpp"
#include "cdfpp/cdf-io/debug/record_stream.hpp"
#include "tests_config.hpp"
#include <sstream>
#include <string>

using namespace cdf::io::debug;

namespace
{
// Walks path in physical order and prints the first record matching record_t, so each
// GIVEN below only has to say which type and fixture it needs, not how to find it.
template <typename record_t>
std::string print_first_matching_record(const std::string& path, bool& printed)
{
    std::ostringstream oss;
    printed = false;
    for_each_record(path,
        [&](std::size_t offset, const auto& record)
        {
            using T = std::decay_t<decltype(record)>;
            if constexpr (std::is_same_v<T, record_t>)
            {
                if (!printed)
                {
                    print_record(oss, offset, record);
                    printed = true;
                }
            }
        });
    return oss.str();
}
}

SCENARIO("cdf_attr_scope_str names every scope, including the assumed variants", "[record_repr]")
{
    REQUIRE(cdf::cdf_attr_scope_str(cdf::cdf_attr_scope::global) == "global");
    REQUIRE(cdf::cdf_attr_scope_str(cdf::cdf_attr_scope::variable) == "variable");
    REQUIRE(cdf::cdf_attr_scope_str(cdf::cdf_attr_scope::global_assumed) == "global (assumed)");
    REQUIRE(cdf::cdf_attr_scope_str(cdf::cdf_attr_scope::variable_assumed) == "variable (assumed)");
    REQUIRE(cdf::cdf_attr_scope_str(static_cast<cdf::cdf_attr_scope>(99)) == "Unknown");
}

SCENARIO("print_record generically prints every field of a real record by name", "[record_repr]")
{
    GIVEN("the CDR of a real v3 fixture")
    {
        const std::string path = std::string(DATA_PATH) + "/a_cdf.cdf";
        bool printed = false;
        const std::string out
            = print_first_matching_record<cdf::io::cdf_CDR_t<cdf::io::v3x_tag>>(path, printed);

        THEN("the header line names the record type, offset and size")
        {
            REQUIRE(printed);
            REQUIRE(out.find("CDR") != std::string::npos);
            REQUIRE(out.find("@8") != std::string::npos);
            REQUIRE(out.find("312") != std::string::npos);
        }
        THEN("every real field name appears, purely from reflection - no per-type printer")
        {
            for (const std::string field : { "GDRoffset", "Version", "Release", "Encoding", "Flags",
                     "Increment", "Identifier", "copyright" })
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
        bool printed = false;
        const std::string out
            = print_first_matching_record<cdf::io::cdf_zVDR_t<cdf::io::v3x_tag>>(path, printed);

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
        THEN(
            "the on-disk PadValues bytes render with a real byte count, not the hardcoded-empty "
            "placeholder this fixture's \"var\" zVariable used to report")
        {
            REQUIRE(out.find("PadValues: <8 bytes>") != std::string::npos);
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
