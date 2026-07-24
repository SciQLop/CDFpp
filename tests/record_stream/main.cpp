#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cdfpp/cdf-io/debug/record_stream.hpp"
#include "cdfpp/cdf-io/loading/loading.hpp"
#include "tests_config.hpp"
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace cdf::io::debug;
using cdf::io::v3x_tag;

namespace
{
std::vector<char> read_file(const std::string& path)
{
    std::ifstream f { path, std::ios::binary };
    return { std::istreambuf_iterator<char> { f }, std::istreambuf_iterator<char> {} };
}

struct counting_corruption_handler
{
    int count = 0;
    recovery_action operator()(const corruption_report&)
    {
        count++;
        return { recovery_action::kind_t::abort };
    }
};
}

SCENARIO("for_each_record walks a well-formed v3 CDF's records in physical order", "[record_stream]")
{
    GIVEN("a real uncompressed v3 fixture")
    {
        const std::string path = std::string(DATA_PATH) + "/a_cdf.cdf";
        THEN("it visits every record exactly once, in file order, hitting EOF exactly")
        {
            std::vector<std::pair<std::size_t, cdf::cdf_record_type>> seen;
            counting_corruption_handler on_corruption;
            for_each_record(
                path,
                [&](std::size_t offset, const auto& record)
                {
                    using T = std::decay_t<decltype(record)>;
                    if constexpr (std::is_same_v<T, undecoded_record_t>)
                        seen.emplace_back(offset, record.type);
                    else
                        seen.emplace_back(offset, record.header.record_type);
                },
                std::ref(on_corruption));

            REQUIRE(on_corruption.count == 0);
            REQUIRE(seen.size() == 89);
            REQUIRE(seen.front() == std::pair { std::size_t { 8 }, cdf::cdf_record_type::CDR });
            REQUIRE(seen[1] == std::pair { std::size_t { 320 }, cdf::cdf_record_type::GDR });
            REQUIRE(seen.back()
                == std::pair { std::size_t { 122926 }, cdf::cdf_record_type::AgrEDR });
        }
        THEN("the number of zVDR records matches the number of loaded zVariables")
        {
            auto cdf_opt = cdf::io::load(path);
            REQUIRE(cdf_opt.has_value());
            std::size_t n_zvar = 0;
            for (const auto& [name, var] : cdf_opt->variables)
                if (var.is_zvariable())
                    n_zvar++;

            std::size_t n_zvdr = 0;
            for_each_record(
                path,
                [&](std::size_t, const auto& record)
                {
                    using T = std::decay_t<decltype(record)>;
                    if constexpr (std::is_same_v<T, cdf::io::cdf_zVDR_t<v3x_tag>>)
                        n_zvdr++;
                },
                counting_corruption_handler {});
            REQUIRE(n_zvdr == n_zvar);
        }
    }
}

SCENARIO("for_each_record handles compressed CDFs", "[record_stream]")
{
    GIVEN("a real compressed v3 fixture")
    {
        const std::string path = std::string(DATA_PATH) + "/a_compressed_cdf.cdf";
        THEN("it decompresses and walks the underlying records without reporting corruption")
        {
            counting_corruption_handler on_corruption;
            std::size_t n_records = 0;
            bool saw_cdr = false;
            bool saw_gdr = false;
            for_each_record(
                path,
                [&](std::size_t, const auto& record)
                {
                    using T = std::decay_t<decltype(record)>;
                    n_records++;
                    if constexpr (std::is_same_v<T, cdf::io::cdf_CDR_t<v3x_tag>>)
                        saw_cdr = true;
                    if constexpr (std::is_same_v<T, cdf::io::cdf_GDR_t<v3x_tag>>)
                        saw_gdr = true;
                },
                std::ref(on_corruption));

            REQUIRE(on_corruption.count == 0);
            REQUIRE(n_records > 0);
            REQUIRE(saw_cdr);
            REQUIRE(saw_gdr);
        }
    }
}

SCENARIO("for_each_record reports corruption instead of crashing or looping forever", "[record_stream]")
{
    GIVEN("a buffer built from a real CDR immediately followed by a real GDR, "
          "with the CDR's record_size corrupted to a bogus huge value")
    {
        auto file = read_file(std::string(DATA_PATH) + "/a_cdf.cdf");
        // magic(8) + CDR[8,320) (312 bytes) + GDR[320,404) (84 bytes)
        std::vector<char> corrupted(file.begin(), file.begin() + 404);
        std::uint64_t bogus_size = 0xFFFFFFFFFFFFFFFFULL;
        char be[8];
        for (int i = 0; i < 8; ++i)
            be[i] = static_cast<char>((bogus_size >> (8 * (7 - i))) & 0xff);
        std::memcpy(corrupted.data() + 8, be, 8);

        THEN("the default (abort) handler stops at the corrupted record and never sees the GDR")
        {
            std::vector<cdf::cdf_record_type> seen;
            corruption_report report {};
            bool corruption_seen = false;
            for_each_record(
                std::as_const(corrupted),
                [&](std::size_t, const auto& record)
                {
                    using T = std::decay_t<decltype(record)>;
                    if constexpr (!std::is_same_v<T, undecoded_record_t>)
                        seen.push_back(record.header.record_type);
                },
                [&](const corruption_report& r)
                {
                    corruption_seen = true;
                    report = r;
                    return recovery_action { recovery_action::kind_t::abort };
                });

            REQUIRE(corruption_seen);
            REQUIRE(report.kind == corruption_kind::invalid_record_size);
            REQUIRE(seen.empty());
        }
        THEN("a handler that skips the known-good record size recovers and still sees the GDR")
        {
            std::vector<cdf::cdf_record_type> seen;
            for_each_record(
                std::as_const(corrupted),
                [&](std::size_t, const auto& record)
                {
                    using T = std::decay_t<decltype(record)>;
                    if constexpr (!std::is_same_v<T, undecoded_record_t>)
                        seen.push_back(record.header.record_type);
                },
                [&](const corruption_report&) {
                    return recovery_action { recovery_action::kind_t::skip_bytes, 312 };
                });

            REQUIRE(seen == std::vector { cdf::cdf_record_type::GDR });
        }
    }
}
