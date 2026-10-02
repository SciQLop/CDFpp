#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <numeric>
#include <thread>
#include <utility>
#include <vector>


#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>


#include <chrono>
#include <filesystem>


#include "cdfpp/attribute.hpp"
#include "cdfpp/cdf-debug.hpp"
#include "cdfpp/cdf-file.hpp"
#include "cdfpp/cdf-io/cdf-io.hpp"
#include "cdfpp/cdf-io/endianness.hpp"
#include "cdfpp/chrono/cdf-chrono.hpp"
#include "cdfpp/variable.hpp"

#include "tests_config.hpp"


template <typename T>
struct cos_gen
{
    const T step;
    cos_gen(T step) : step { step } { }
    no_init_vector<T> operator()(std::size_t size)
    {
        no_init_vector<T> values(size);
        std::generate(std::begin(values), std::end(values),
            [i = T(0.), step = step]() mutable
            {
                auto v = std::cos(i);
                i += step;
                return v;
            });
        return values;
    }
};

template <typename T>
struct ones
{
    no_init_vector<T> operator()(std::size_t size)
    {
        no_init_vector<T> values(size);
        std::generate(std::begin(values), std::end(values), []() mutable { return T(1); });
        return values;
    }
};

template <typename T>
struct zeros
{
    no_init_vector<T> operator()(std::size_t size)
    {
        no_init_vector<T> values(size);
        std::generate(std::begin(values), std::end(values), []() mutable { return T(0); });
        return values;
    }
};


SCENARIO("Saving a cdf file", "[CDF]")
{
    const auto cdf_path = (std::filesystem::temp_directory_path() / "cdfpp_saving_a_cdf_file.cdf").string();

    {
        CDF cdf_obj;
        cdf_obj.attributes.emplace("some global attr",
            cdf::Attribute { "some global attr",
                { data_t { no_init_vector<double> { 1., 2., 3. }, CDF_Types::CDF_DOUBLE } } });
        cdf_obj.attributes.emplace("another global attr",
            cdf::Attribute { "another global attr",
                { data_t {
                    no_init_vector<char> { 'h', 'e', 'l', 'l', 'o' }, CDF_Types::CDF_CHAR } } });

        cdf_obj.variables.emplace("var1",
            Variable { "var1", 0, data_t { zeros<float> {}(100), CDF_Types::CDF_FLOAT }, { 100 } });
        cdf_obj.variables["var1"].set_compression_type(cdf_compression_type::gzip_compression);
        REQUIRE(cdf::io::save(cdf_obj, cdf_path));
    }
    {
        auto cdf_obj = cdf::io::load(cdf_path);
        REQUIRE(cdf_obj->attributes.count("some global attr"));
        REQUIRE(cdf_obj->attributes.count("another global attr"));
        REQUIRE(cdf_obj->variables.count("var1"));
    }
    std::filesystem::remove(cdf_path);
}

namespace
{
// Loads a real, on-disk fixture, saves it to an in-memory buffer, then reloads
// that buffer. Comparing the reloaded CDF against the original (rather than the
// original file's own bytes) tolerates legitimate layout differences (e.g.
// update_size choices) while still catching any bug that corrupts record
// headers, values, or metadata on the way through save()+load().
std::pair<CDF, CDF> saved_and_reloaded(const std::string& fixture_name)
{
    auto path = std::string(DATA_PATH) + "/" + fixture_name;
    auto original = cdf::io::load(path);
    REQUIRE(original != std::nullopt);

    auto saved = cdf::io::save(*original);
    REQUIRE(std::size(saved) > 0);

    // Named lvalue, loaded eagerly (lazy_load=false): cdf::io::load's std::vector<char>
    // overload wraps a non-owning view over this buffer, so the buffer must outlive
    // any lazy/deferred read of the reloaded CDF's variable values. Passing it as a
    // temporary and/or with lazy_load=true would leave the reloaded CDF holding a
    // dangling view the moment the buffer goes away.
    std::vector<char> buffer(std::cbegin(saved), std::cend(saved));
    auto reloaded = cdf::io::load(buffer, true, false);
    REQUIRE(reloaded != std::nullopt);

    return { *original, *reloaded };
}
}

SCENARIO("Round-tripping real CDF fixtures through save and reload", "[CDF]")
{
    GIVEN("a real, uncompressed CDF fixture")
    {
        THEN("saving then reloading it yields a structurally and value-equal CDF")
        {
            auto [original, reloaded] = saved_and_reloaded("a_cdf.cdf");
            REQUIRE(reloaded == original);
        }
    }
    GIVEN("a real, GZIP-compressed CDF fixture")
    {
        THEN("saving then reloading it yields a structurally and value-equal CDF")
        {
            auto [original, reloaded] = saved_and_reloaded("a_compressed_cdf.cdf");
            REQUIRE(reloaded == original);
        }
    }
    GIVEN("a real, RLE-compressed CDF fixture")
    {
        THEN("saving then reloading it yields a structurally and value-equal CDF")
        {
            auto [original, reloaded] = saved_and_reloaded("a_rle_compressed_cdf.cdf");
            REQUIRE(reloaded == original);
        }
    }
    GIVEN("a real CDF fixture with per-variable (CVVR) compressed variables")
    {
        THEN("saving then reloading it yields a structurally and value-equal CDF")
        {
            auto [original, reloaded] = saved_and_reloaded("a_cdf_with_compressed_vars.cdf");
            REQUIRE(reloaded == original);
        }
    }
}

namespace
{
// cpp_utils::serde::unused<T>'s deserialize path discards whatever it reads into a
// local, never writing it back to .value (see cpp_utils/serde/deserialization.hpp's
// load_field(..., unused_field auto&)) -- so round-tripping through cdf::io::load()
// cannot catch a wrong reserved-field default on save; only inspecting the raw
// on-disk bytes can. Every field read here is a fixed, spec-mandated sentinel per
// the CDF Internal Format Description (v3.9): CDR/ADR's rfuE, and AEDR's rfuD/rfuE,
// must always be -1 (0xFFFFFFFF), regardless of attribute scope or content.
template <typename T>
T read_be(const std::vector<char>& buffer, std::size_t offset)
{
    return cdf::endianness::decode<cdf::endianness::big_endian_t, T>(
        reinterpret_cast<const unsigned char*>(buffer.data() + offset));
}

struct edr_reserved_fields_check_t
{
    int global = 0;
    int variable = 0;
};

// Walks the ADR linked list starting at adr, asserting every ADR.rfuE and every
// linked AgrEDR/AzEDR's rfuD/rfuE are -1, and counts how many of each were seen.
edr_reserved_fields_check_t check_adr_chain_reserved_fields(
    const std::vector<char>& buffer, int64_t adr)
{
    edr_reserved_fields_check_t counts;
    while (adr != 0)
    {
        REQUIRE(read_be<int32_t>(buffer, adr + 64) == -1); // ADR.rfuE
        auto agr_edr_head = read_be<int64_t>(buffer, adr + 20);
        auto az_edr_head = read_be<int64_t>(buffer, adr + 48);
        if (agr_edr_head != 0)
        {
            REQUIRE(read_be<int32_t>(buffer, agr_edr_head + 48) == -1); // AgrEDR.rfuD
            REQUIRE(read_be<int32_t>(buffer, agr_edr_head + 52) == -1); // AgrEDR.rfuE
            counts.global++;
        }
        if (az_edr_head != 0)
        {
            REQUIRE(read_be<int32_t>(buffer, az_edr_head + 48) == -1); // AzEDR.rfuD
            REQUIRE(read_be<int32_t>(buffer, az_edr_head + 52) == -1); // AzEDR.rfuE
            counts.variable++;
        }
        adr = read_be<int64_t>(buffer, adr + 12);
    }
    return counts;
}
}

SCENARIO("Reserved fields are saved as their spec-mandated sentinel value", "[CDF]")
{
    GIVEN("a CDF with both a global (file) attribute and a variable attribute")
    {
        CDF cdf_obj;
        cdf_obj.attributes.emplace("file_attr",
            cdf::Attribute { "file_attr",
                { data_t { no_init_vector<char> { 'h', 'i' }, CDF_Types::CDF_CHAR } } });
        cdf_obj.variables.emplace("var1",
            Variable { "var1", 0, data_t { zeros<float> {}(3), CDF_Types::CDF_FLOAT }, { 3 } });
        cdf_obj.variables["var1"].attributes.emplace("var_attr",
            cdf::VariableAttribute {
                "var_attr", data_t { no_init_vector<char> { 'h', 'i' }, CDF_Types::CDF_CHAR } });

        auto saved = cdf::io::save(cdf_obj);
        REQUIRE(std::size(saved) > 0);
        std::vector<char> buffer(std::cbegin(saved), std::cend(saved));

        THEN("the CDR's rfuE is -1")
        {
            REQUIRE(read_be<int32_t>(buffer, 8 + 52) == -1);
        }
        THEN("every ADR's rfuE is -1, and every AEDR's rfuD/rfuE are -1, regardless of scope")
        {
            auto gdr_offset = read_be<int64_t>(buffer, 8 + 12);
            auto adr = read_be<int64_t>(buffer, gdr_offset + 28);
            REQUIRE(adr != 0);
            auto counts = check_adr_chain_reserved_fields(buffer, adr);
            REQUIRE(counts.global == 1);
            REQUIRE(counts.variable == 1);
        }
    }
}

SCENARIO("RLE-compressed CDFs carry a spec-conformant Compression Parameters Record",
    "[CDF]")
{
    GIVEN("a whole-file RLE-compressed CDF")
    {
        CDF cdf_obj;
        cdf_obj.variables.emplace("var1",
            Variable { "var1", 0, data_t { zeros<float> {}(100), CDF_Types::CDF_FLOAT }, { 100 } });
        cdf_obj.compression = cdf_compression_type::rle_compression;

        auto saved = cdf::io::save(cdf_obj);
        REQUIRE(std::size(saved) > 0);
        std::vector<char> buffer(std::cbegin(saved), std::cend(saved));

        THEN("the CPR's pCount is 1 and cParms[0] is 0, per the CDF Internal Format "
             "Description's documented default for RLE (\"cParms[0] is 0\", \"pCount... "
             "is 1\") -- an empty cParms/pCount=0 is rejected by the NASA reference "
             "cdfvalidate tool as UNKNOWN_COMPRESSION")
        {
            auto cpr_offset = read_be<int64_t>(buffer, 8 + 12); // CCR.CPRoffset
            REQUIRE(read_be<int32_t>(buffer, cpr_offset + 12) == 1); // CPR.cType == RLE
            REQUIRE(read_be<int32_t>(buffer, cpr_offset + 20) == 1); // CPR.pCount
            REQUIRE(read_be<int32_t>(buffer, cpr_offset + 24) == 0); // CPR.cParms[0]
        }
    }
}

namespace
{
std::vector<cdf_compression_type> compiled_in_codecs()
{
    return {
        cdf_compression_type::rle_compression,
        cdf_compression_type::gzip_compression,
#ifdef CDFPP_USE_ZSTD
        cdf_compression_type::zstd_compression,
#endif
#ifdef CDFPP_USE_BLOSC2
        cdf_compression_type::blosc2_compression,
#endif
    };
}

no_init_vector<tt2000_t> increasing_epochs(std::size_t size)
{
    no_init_vector<tt2000_t> values(size);
    std::generate(std::begin(values), std::end(values),
        [t = int64_t { 631108869184000000 }]() mutable { return tt2000_t { t += 62'500'000 }; });
    return values;
}

CDF cdf_with_variables_compressed_as(cdf_compression_type codec)
{
    CDF cdf_obj;
    cdf_obj.variables.emplace("cos",
        Variable { "cos", 0, data_t { cos_gen<double> { 0.01 }(30000), CDF_Types::CDF_DOUBLE },
            { 10000, 3 } });
    cdf_obj.variables.emplace("epoch",
        Variable { "epoch", 0,
            data_t { increasing_epochs(10000), CDF_Types::CDF_TIME_TT2000 }, { 10000 } });
    for (auto& [_, variable] : cdf_obj.variables)
        variable.set_compression_type(codec);
    return cdf_obj;
}

std::optional<CDF> saved_and_reloaded(const CDF& cdf_obj, std::vector<char>& buffer)
{
    auto saved = cdf::io::save(cdf_obj);
    REQUIRE(std::size(saved) > 0);
    buffer.assign(std::cbegin(saved), std::cend(saved));
    return cdf::io::load(buffer, true, false);
}
}

SCENARIO("Every compiled-in codec round-trips through save and reload", "[CDF]")
{
    const auto codec = GENERATE(from_range(compiled_in_codecs()));
    std::vector<char> buffer;
    GIVEN("variables compressed with " + cdf_compression_type_str(codec))
    {
        const auto original = cdf_with_variables_compressed_as(codec);
        auto reloaded = saved_and_reloaded(original, buffer);
        REQUIRE(reloaded != std::nullopt);
        THEN("values and compression type survive")
        {
            REQUIRE(reloaded->variables["cos"] == original.variables.at("cos"));
            REQUIRE(reloaded->variables["epoch"] == original.variables.at("epoch"));
            REQUIRE(reloaded->variables["cos"].compression_type() == codec);
        }
    }
    GIVEN("a whole file compressed with " + cdf_compression_type_str(codec))
    {
        auto original = cdf_with_variables_compressed_as(cdf_compression_type::no_compression);
        original.compression = codec;
        auto reloaded = saved_and_reloaded(original, buffer);
        REQUIRE(reloaded != std::nullopt);
        THEN("values and compression type survive")
        {
            REQUIRE(reloaded->variables["cos"] == original.variables.at("cos"));
            REQUIRE(reloaded->compression == codec);
        }
    }
}

SCENARIO("Saving a lazily loaded CDF over its own file", "[CDF]")
{
    // Lazy variables read their values from the file when needed; saving over that file
    // must not destroy them before they are read.
    for (const std::string fixture : { "a_cdf.cdf", "a_cdf_with_compressed_vars.cdf" })
    {
        GIVEN(fixture + " loaded lazily from a copy")
        {
            const auto source = std::string(DATA_PATH) + "/" + fixture;
            const auto copy = std::filesystem::temp_directory_path() / ("cdfpp_overwrite_" + fixture);
            std::filesystem::copy_file(
                source, copy, std::filesystem::copy_options::overwrite_existing);
            const auto reference = cdf::io::load(source, true, false);
            auto lazy = cdf::io::load(copy.string());
            REQUIRE(reference != std::nullopt);
            REQUIRE(lazy != std::nullopt);

            THEN("saving it over that copy keeps every value")
            {
                REQUIRE(cdf::io::save(*lazy, copy.string()));
                const auto reloaded = cdf::io::load(copy.string(), true, false);
                REQUIRE(reloaded != std::nullopt);
                REQUIRE(*reloaded == *reference);
            }
            std::filesystem::remove(copy);
        }
    }
}

SCENARIO("Saving over a bigger file leaves none of its bytes", "[CDF]")
{
    const auto path = std::filesystem::temp_directory_path() / "cdfpp_save_over_bigger.cdf";
    CDF big;
    big.variables.emplace(
        "var", Variable { "var", 0, data_t { ones<double> {}(100000), CDF_Types::CDF_DOUBLE },
                   { 100000 } });
    CDF small;
    small.variables.emplace(
        "var", Variable { "var", 0, data_t { ones<double> {}(10), CDF_Types::CDF_DOUBLE }, { 10 } });
    REQUIRE(cdf::io::save(big, path.string()));
    REQUIRE(cdf::io::save(small, path.string()));
    REQUIRE(std::filesystem::file_size(path) == std::size(cdf::io::save(small)));
    const auto reloaded = cdf::io::load(path.string(), true, false);
    REQUIRE(reloaded != std::nullopt);
    REQUIRE(*reloaded == small);
    std::filesystem::remove(path);
}

SCENARIO("Saving to a path that can't be written reports a failure", "[CDF]")
{
    const auto path = std::filesystem::temp_directory_path() / "cdfpp_missing_dir" / "out.cdf";
    REQUIRE_FALSE(cdf::io::save(CDF {}, path.string()));
}

SCENARIO("A variable whose values are loaded no longer depends on its file", "[CDF]")
{
    // Windows can't overwrite a file that is still memory-mapped: once values are in memory,
    // nothing may keep the file's buffer alive (is_contiguous() used to).
    const auto copy = std::filesystem::temp_directory_path() / "cdfpp_detached_copy.cdf";
    std::filesystem::copy_file(std::string(DATA_PATH) + "/a_cdf.cdf", copy,
        std::filesystem::copy_options::overwrite_existing);
    {
        auto cdf = cdf::io::load(copy.string());
        REQUIRE(cdf != std::nullopt);
        for (const auto& [_, variable] : cdf->variables)
            variable.load_values();
        std::filesystem::resize_file(copy, 0);
        for (const auto& [_, variable] : cdf->variables)
            REQUIRE_NOTHROW(variable.is_contiguous());
    }
    std::filesystem::remove(copy);
}

SCENARIO("Big compressed variables round-trip through the parallel paths", "[CDF]")
{
    // Above min_bytes_worth_threads, blocks are compressed and decompressed on several threads.
    const auto codec = GENERATE(from_range(compiled_in_codecs()));
    constexpr std::size_t records = 4 * cdf::parallel::min_bytes_worth_threads / (3 * sizeof(double));
    CDF original;
    original.variables.emplace("cos",
        Variable { "cos", 0,
            data_t { cos_gen<double> { 0.01 }(records * 3), CDF_Types::CDF_DOUBLE },
            { static_cast<uint32_t>(records), 3 } });
    original.variables["cos"].set_compression_type(codec);
    std::vector<char> buffer;
    auto eager = saved_and_reloaded(original, buffer);
    REQUIRE(eager != std::nullopt);
    REQUIRE(eager->variables["cos"] == original.variables.at("cos"));
    auto lazy = cdf::io::load(buffer, true, true);
    REQUIRE(lazy != std::nullopt);
    REQUIRE(lazy->variables["cos"] == original.variables.at("cos"));
}

SCENARIO("A variable saves borrowed values without owning them", "[CDF]")
{
    auto values = std::make_shared<no_init_vector<double>>(cos_gen<double> { 0.01 }(300));
    const std::weak_ptr<no_init_vector<double>> owner = values;
    const Variable copied { "cos", 0, data_t { *values, CDF_Types::CDF_DOUBLE }, { 100, 3 } };
    CDF cdf;
    cdf.variables.emplace("cos", Variable { "cos", 0, data_t {}, {} });
    auto& variable = cdf.variables["cos"];
    variable.set_data(borrowed_data { values, reinterpret_cast<const char*>(values->data()),
                          values->size() * sizeof(double), CDF_Types::CDF_DOUBLE },
        { 100, 3 });
    const auto* borrowed_bytes = reinterpret_cast<const char*>(values->data());
    values.reset();
    WHEN("it is saved")
    {
        std::vector<char> buffer;
        auto reloaded = saved_and_reloaded(cdf, buffer);
        THEN("the file holds the borrowed values, which are still borrowed")
        {
            REQUIRE(reloaded != std::nullopt);
            REQUIRE(reloaded->variables["cos"] == copied);
            REQUIRE(std::as_const(variable).bytes_ptr() == borrowed_bytes);
            REQUIRE_FALSE(owner.expired());
        }
    }
    WHEN("its values are accessed for writing")
    {
        variable.get<double>()[0] = 42.;
        THEN("they are copied first and the borrowed buffer is released")
        {
            REQUIRE(owner.expired());
            REQUIRE(variable.get<double>()[1] == copied.get<double>()[1]);
        }
    }
    WHEN("several threads read its values at once")
    {
        std::vector<double> sums(8);
        std::vector<std::thread> readers;
        for (auto& sum : sums)
            readers.emplace_back(
                [&sum, &variable]()
                {
                    const auto& values = std::as_const(variable).get<double>();
                    sum = std::accumulate(std::cbegin(values), std::cend(values), 0.);
                });
        for (auto& reader : readers)
            reader.join();
        THEN("they all read the same copy")
        {
            const auto& expected = copied.get<double>();
            const auto expected_sum
                = std::accumulate(std::cbegin(expected), std::cend(expected), 0.);
            REQUIRE(std::all_of(std::cbegin(sums), std::cend(sums),
                [expected_sum](double sum) { return sum == expected_sum; }));
            REQUIRE(owner.expired());
        }
    }
}

namespace
{
template <typename T>
no_init_vector<T> counting(std::size_t size)
{
    no_init_vector<T> values(size);
    for (std::size_t i = 0; i < size; i++)
        values[i] = static_cast<T>(i % 9973);
    return values;
}
}

SCENARIO("Multidimensional records round-trip through every majority and byte order", "[CDF]")
{
    for (const auto majority : { cdf_majority::row, cdf_majority::column })
        for (const auto encoding : { cdf_encoding::IBMPC, cdf_encoding::network })
        {
            CDF cdf_obj;
            cdf_obj.majority = majority;
            cdf_obj.encoding = encoding;
            // 6 MB: saved in several chunks.
            cdf_obj.variables.emplace("big", Variable { "big", 0,
                data_t { counting<float>(3000 * 4 * 8 * 16), CDF_Types::CDF_FLOAT },
                { 3000, 4, 8, 16 } });
            cdf_obj.variables.emplace("odd", Variable { "odd", 1,
                data_t { counting<double>(7 * 3 * 5), CDF_Types::CDF_DOUBLE }, { 7, 3, 5 } });
            cdf_obj.variables.emplace("short", Variable { "short", 2,
                data_t { counting<int16_t>(5 * 8 * 8), CDF_Types::CDF_INT2 }, { 5, 8, 8 } });
            no_init_vector<char> text(3 * 2 * 3 * 4);
            for (std::size_t i = 0; i < std::size(text); i++)
                text[i] = static_cast<char>('a' + i % 26);
            cdf_obj.variables.emplace("text", Variable { "text", 3,
                data_t { std::move(text), CDF_Types::CDF_CHAR }, { 3, 2, 3, 4 } });
            const auto saved = cdf::io::save(cdf_obj);
            std::vector<char> buffer(std::cbegin(saved), std::cend(saved));
            const auto reloaded = cdf::io::load(buffer, true, false);
            REQUIRE(reloaded != std::nullopt);
            for (const auto& name : { "big", "odd", "short", "text" })
                REQUIRE(reloaded->variables[name] == cdf_obj.variables[name]);
        }
}

SCENARIO("Values that don't match the variable's shape raise on first access", "[CDF]")
{
    // A corrupt file loads lazily: the size check runs on first access, which must throw
    // rather than end the program (the Python bindings reach it through bytes_ptr()).
    Variable variable { "v", 0,
        lazy_data { []() { return data_t { no_init_vector<double>(7), CDF_Types::CDF_DOUBLE }; },
            CDF_Types::CDF_DOUBLE },
        { 3, 2 } };
    REQUIRE_THROWS_AS(variable.bytes_ptr(), std::invalid_argument);
}
