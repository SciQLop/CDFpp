#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cdfpp/cdf-io/cdf-io.hpp"
#include "cdfpp/cdf-io/debug/record_stream.hpp"
#include "tests_config.hpp"
#include <atomic>
#include <memory>
#include <thread>
#include <cpp_utils/io/local_range_reader.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using cpp_utils::io::local_range_reader;
using cpp_utils::io::page_size;
using cpp_utils::io::remote_buffer_options;

namespace
{
std::vector<std::string> resource_cdfs()
{
    std::vector<std::string> paths;
    for (const auto& entry : std::filesystem::directory_iterator { DATA_PATH })
        if (entry.path().extension() == ".cdf" && entry.path().filename() != "not_a_cdf.cdf")
            paths.push_back(entry.path().string());
    return paths;
}

remote_buffer_options page_by_page()
{
    return remote_buffer_options { .read_ahead = page_size(), .tail_prefetch = 0 };
}

void require_same_structure(const cdf::CDF& remote, const cdf::CDF& local)
{
    REQUIRE(std::size(remote.variables) == std::size(local.variables));
    REQUIRE(std::size(remote.attributes) == std::size(local.attributes));
    for (const auto& [name, variable] : local.variables)
        REQUIRE(remote.variables.at(name).shape() == variable.shape());
}

// Files holding NaN values never compare equal, even to a second load from disk: for those,
// only the structure is compared.
void require_same_file(const cdf::CDF& remote, const cdf::CDF& local, const std::string& path)
{
    require_same_structure(remote, local);
    if (local == *cdf::io::load(path, true, false))
        REQUIRE(remote == local);
    REQUIRE(remote.majority == local.majority);
    REQUIRE(remote.compression == local.compression);
    REQUIRE(remote.distribution_version == local.distribution_version);
    for (const auto& [name, variable] : local.variables)
        REQUIRE(remote.variables.at(name).compression_type() == variable.compression_type());
}

std::string copy_truncated(const std::string& source, std::size_t size, const std::string& name)
{
    const auto path = (std::filesystem::current_path() / name).string();
    std::ifstream in { source, std::ios::binary };
    std::vector<char> bytes(size);
    in.read(bytes.data(), static_cast<std::streamsize>(size));
    std::ofstream { path, std::ios::binary }.write(bytes.data(), static_cast<std::streamsize>(size));
    return path;
}
}

SCENARIO("Scenario: a CDF loaded through a range reader equals the mmap load", "[remote]")
{
    for (const auto& path : resource_cdfs())
    {
        INFO(path);
        for (const bool lazy : { false, true })
        {
            INFO("lazy: " << lazy);
            const auto local = cdf::io::load(path, true, lazy);
            REQUIRE(local.has_value());
            const auto remote
                = cdf::io::load(local_range_reader { path }, true, lazy, page_by_page());
            REQUIRE(remote.has_value());
            require_same_file(*remote, *local, path);
        }
    }
}

SCENARIO("Scenario: a CDF cut inside its first record raises instead of crashing", "[remote]")
{
    const auto path
        = copy_truncated(std::string { DATA_PATH } + "/a_cdf.cdf", 20, "cdfpp_cut_cdf.cdf");
    REQUIRE_THROWS_AS(cdf::io::load(local_range_reader { path }, true, false, page_by_page()),
        std::runtime_error);
    std::filesystem::remove(path);
}

SCENARIO("Scenario: an empty remote file loads as nothing", "[remote]")
{
    const auto path = copy_truncated(std::string { DATA_PATH } + "/a_cdf.cdf", 0, "cdfpp_empty.cdf");
    REQUIRE_FALSE(cdf::io::load(local_range_reader { path }).has_value());
    std::filesystem::remove(path);
}

SCENARIO("Scenario: ensure_record fetches through a const remote buffer", "[remote]")
{
    local_range_reader reader { std::string { DATA_PATH } + "/a_cdf.cdf" };
    const auto buffer = cdf::io::buffers::make_shared_remote_adapter(reader, page_by_page());
    cdf::io::ensure_record<cdf::io::cdf_CDR_t<cdf::io::v3x_tag>>(buffer, 8);
    REQUIRE(reader.bytes_served() > 0);
    REQUIRE_NOTHROW(buffer.check_present(8, 12));
}

namespace
{
struct open_cost
{
    std::size_t batches;
    std::size_t bytes;
};

open_cost lazy_open_cost(const std::string& file, remote_buffer_options options)
{
    local_range_reader reader { std::string { DATA_PATH } + "/" + file };
    const auto cdf = cdf::io::load(reader, true, true, std::move(options));
    REQUIRE(cdf.has_value());
    return { std::size(reader.batches()), reader.bytes_served() };
}
}

SCENARIO("Scenario: opening a remote CDF costs few batches", "[remote]")
{
    // a_cdf.cdf: 123 kB, metadata spread over 60 kB, some written at the end. With 4 KiB pages
    // it measured 16 batches page by page, 10 with a 32 KiB tail; counts scale with page size.
    const auto a = lazy_open_cost("a_cdf.cdf", page_by_page());
    const auto b
        = lazy_open_cost("a_cdf.cdf", { .read_ahead = page_size(), .tail_prefetch = 32 * 1024 });
    const auto c = lazy_open_cost("a_cdf.cdf", remote_buffer_options {});
    REQUIRE(a.batches > 1);
    REQUIRE(b.batches < a.batches);
    REQUIRE(c.batches == 1);
}

namespace
{
std::size_t count_records(const std::string& path, cdf::cdf_record_type type)
{
    std::size_t count = 0;
    cdf::io::debug::for_each_record(path,
        [&](std::size_t, const auto& record)
        {
            using T = std::decay_t<decltype(record)>;
            if constexpr (std::is_same_v<T, cdf::io::debug::undecoded_record_t>)
                count += record.type == type;
            else
                count += record.header.record_type == type;
        });
    return count;
}

const std::string many_blocks = std::string { DATA_PATH } + "/many_compressed_blocks.cdf";
}

SCENARIO("Scenario: loading a variable batches its index and data fetches", "[remote]")
{
    const std::string path = std::string { DATA_PATH } + "/fgm_blocks.cdf";
    local_range_reader reader { path };
    auto remote = cdf::io::load(reader, true, true, page_by_page());
    REQUIRE(remote.has_value());
    const auto batches_at_open = std::size(reader.batches());

    (void)remote->variables["b_gse"].bytes_ptr();

    const auto load_batches = std::size(reader.batches()) - batches_at_open;
    REQUIRE(count_records(path, cdf::cdf_record_type::CVVR) == 3);
    REQUIRE(load_batches <= count_records(path, cdf::cdf_record_type::VXR) + 2);
    REQUIRE(reader.bytes_served() <= std::filesystem::file_size(path));
    REQUIRE(*remote == *cdf::io::load(path, true, false));
}

namespace
{
// A gzip variable big enough to be decoded on several threads, saved as several blocks.
std::string write_threaded_fixture()
{
    const auto path
        = (std::filesystem::current_path() / "cdfpp_remote_threaded.cdf").string();
    constexpr std::size_t count = 2 * cdf::parallel::min_bytes_worth_threads / sizeof(double);
    no_init_vector<double> values(count);
    for (std::size_t i = 0; i < count; ++i)
        values[i] = static_cast<double>(i % 1000);
    cdf::CDF file;
    file.variables.emplace("values",
        cdf::Variable { "values", 0, cdf::data_t { std::move(values), cdf::CDF_Types::CDF_DOUBLE },
            { count } });
    file.variables["values"].set_compression_type(cdf::cdf_compression_type::gzip_compression);
    REQUIRE(cdf::io::save(file, path));
    return path;
}
}

// The reader maps the file until it is destroyed, and Windows can't delete a mapped file:
// the caller removes the file once this returns.
void require_fetches_on_calling_thread_only(const std::string& path)
{
    local_range_reader reader { path };
    auto remote = cdf::io::load(reader, true, true, page_by_page());
    REQUIRE(remote.has_value());
    REQUIRE_FALSE(remote->variables["values"].is_contiguous());
    REQUIRE(remote->variables["values"].bytes() >= cdf::parallel::min_bytes_worth_threads);
    (void)remote->variables["values"].bytes_ptr();
    for (const auto& id : reader.caller_threads())
        REQUIRE(id == std::this_thread::get_id());
    REQUIRE(*remote == *cdf::io::load(path, true, false));
}

SCENARIO("Scenario: threaded decoding fetches nothing off the calling thread", "[remote]")
{
    if (cdf::parallel::hardware_threads() < 2)
        SKIP("single-core run: no decoding threads");
    const auto path = write_threaded_fixture();
    require_fetches_on_calling_thread_only(path);
    std::filesystem::remove(path);
}

SCENARIO("Scenario: a thread that must not fetch raises instead of fetching", "[remote]")
{
    local_range_reader reader { many_blocks };
    auto remote = cdf::io::load(reader, true, true, page_by_page());
    REQUIRE(remote.has_value());
    const auto batches_at_open = std::size(reader.batches());

    cdf::io::no_fetch_thread = true;
    REQUIRE_THROWS_WITH(remote->variables["values"].bytes_ptr(),
        Catch::Matchers::ContainsSubstring("remote bytes not fetched"));
    cdf::io::no_fetch_thread = false;
    REQUIRE(std::size(reader.batches()) == batches_at_open);
}

namespace
{
struct skip_last_block_prefetch
{
    skip_last_block_prefetch() { cdf::io::testing_skip_last_block_prefetch = true; }
    ~skip_last_block_prefetch() { cdf::io::testing_skip_last_block_prefetch = false; }
};

std::string copy_with_bytes(const std::string& source, std::size_t offset,
    const std::vector<char>& bytes, const std::string& name)
{
    const auto path = copy_truncated(source, std::filesystem::file_size(source), name);
    std::fstream file { path, std::ios::binary | std::ios::in | std::ios::out };
    file.seekp(static_cast<std::streamoff>(offset));
    file.write(bytes.data(), static_cast<std::streamsize>(std::size(bytes)));
    return path;
}
}

SCENARIO("Scenario: a short prefetch raises on the decode path, never fetches there", "[remote]")
{
    for (const auto& [file, variable] : { std::pair { "a_cdf.cdf", "zeros" },
             std::pair { "fgm_blocks.cdf", "b_gse" } })
    {
        INFO(file);
        local_range_reader reader { std::string { DATA_PATH } + "/" + file };
        auto remote = cdf::io::load(reader, true, true, page_by_page());
        REQUIRE(remote.has_value());
        const skip_last_block_prefetch skip;
        REQUIRE_THROWS_WITH(remote->variables[variable].bytes_ptr(),
            Catch::Matchers::ContainsSubstring("remote bytes not fetched"));
    }
}

SCENARIO("Scenario: a record smaller than its own header raises", "[remote]")
{
    const auto path = copy_with_bytes(std::string { DATA_PATH } + "/a_cdf.cdf", 8,
        std::vector<char>(8, 0), "cdfpp_zero_cdr_size.cdf");
    REQUIRE_THROWS_AS(
        cdf::io::load(local_range_reader { path }, true, true, page_by_page()), std::runtime_error);
    std::filesystem::remove(path);
}

SCENARIO("Scenario: a remote file shorter than the magic number loads as nothing", "[remote]")
{
    const auto path = copy_truncated(std::string { DATA_PATH } + "/a_cdf.cdf", 4, "cdfpp_4b.cdf");
    REQUIRE_FALSE(cdf::io::load(local_range_reader { path }).has_value());
    std::filesystem::remove(path);
}

SCENARIO("Scenario: the debug record walker works on a remote buffer", "[remote]")
{
    const std::string path = std::string { DATA_PATH } + "/a_cdf.cdf";
    std::size_t local_count = 0, remote_count = 0;
    cdf::io::debug::for_each_record(path, [&](std::size_t, const auto&) { ++local_count; });
    local_range_reader reader { path };
    cdf::io::debug::for_each_record(
        cdf::io::buffers::make_shared_remote_adapter(reader, page_by_page()),
        [&](std::size_t, const auto&) { ++remote_count; });
    REQUIRE(remote_count == local_count);
}

namespace
{
struct size_counting_reader
{
    local_range_reader inner;
    std::shared_ptr<std::atomic<int>> size_calls = std::make_shared<std::atomic<int>>(0);

    std::size_t size() const
    {
        ++*size_calls;
        return inner.size();
    }
    void read_ranges(std::span<const cpp_utils::io::byte_range> ranges,
        std::span<char* const> destinations) const
    {
        inner.read_ranges(ranges, destinations);
    }
};
}

SCENARIO("Scenario: loading asks a remote reader for its size once", "[remote]")
{
    size_counting_reader reader { local_range_reader { std::string { DATA_PATH } + "/a_cdf.cdf" } };
    REQUIRE(cdf::io::load(reader).has_value());
    REQUIRE(*reader.size_calls == 1);
}

SCENARIO("Scenario: a corrupt pointer far past the file raises instead of crashing", "[remote]")
{
    // The CDR's GDRoffset field: 8 (magic) + 12 (record header).
    const auto path = copy_with_bytes(std::string { DATA_PATH } + "/a_cdf.cdf", 20,
        std::vector<char>(8, static_cast<char>(0x7F)), "cdfpp_bad_gdr_offset.cdf");
    REQUIRE_THROWS_WITH(cdf::io::load(local_range_reader { path }, true, true, page_by_page()),
        Catch::Matchers::ContainsSubstring("outside the remote file"));
    std::filesystem::remove(path);
}

SCENARIO("Scenario: CDFpp reads ahead 1 MiB by default", "[remote]")
{
    // HelioCloud benchmark: all variables of a 53.6 MB file in 30 round trips instead of 142.
    REQUIRE(cdf::io::default_remote_options().read_ahead == 1024 * 1024);
    REQUIRE(cdf::io::default_remote_options().tail_prefetch == 256 * 1024);
}
