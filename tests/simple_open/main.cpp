#include <algorithm>
#include <cstdint>
#include <optional>
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

#include <chrono>


#include "cdfpp/attribute.hpp"
#include "cdfpp/cdf-debug.hpp"
#include "cdfpp/cdf-file.hpp"
#include "cdfpp/cdf-io/cdf-io.hpp"
#include "cdfpp/chrono/cdf-chrono.hpp"

#include "tests_config.hpp"

bool file_exists(const std::string& path)
{
    if (auto file = fopen(path.c_str(), "r"))
    {
        fclose(file);
        return true;
    }
    else
    {
        return false;
    }
}

bool has_attribute(const cdf::CDF& cd, const std::string& name)
{
    return cd.attributes.find(name) != cd.attributes.cend();
}

bool has_variable(const cdf::CDF& cd, const std::string& name)
{
    return cd.variables.find(name) != cd.variables.cend();
}

template <int index, typename value_type>
std::enable_if_t<std::is_same_v<value_type, const char*>, bool> impl_compare_attribute_value(
    const cdf::Attribute& attribute, const value_type& value)
{
    auto attr_value = attribute.get<char>(index);
    return std::string { attr_value.data(), std::size(attr_value) } == std::string { value };
}

template <int index, typename value_type>
std::enable_if_t<std::is_same_v<value_type, const char*>, bool> impl_compare_attribute_value(
    const cdf::VariableAttribute& attribute, const value_type& value)
{
    auto attr_value = attribute.get<char>();
    return std::string { attr_value.data(), std::size(attr_value) } == std::string { value };
}

template <int index, typename value_type>
std::enable_if_t<std::is_scalar_v<value_type> && !std::is_same_v<value_type, const char*>, bool>
impl_compare_attribute_value(const cdf::Attribute& attribute, const value_type& value)
{
    return attribute.get<value_type>(index) == no_init_vector<value_type> { value };
}

template <int index, typename value_type>
std::enable_if_t<std::is_scalar_v<value_type> && !std::is_same_v<value_type, const char*>, bool>
impl_compare_attribute_value(const cdf::VariableAttribute& attribute, const value_type& value)
{
    return attribute.get<value_type>() == no_init_vector<value_type> { value };
}

template <int index, typename attr_t, typename value_type>
auto impl_compare_attribute_value(const attr_t& attribute,
    const value_type& value) -> decltype(std::cbegin(value), value.at(0), attribute.value(), true)
{
    return attribute.template get<
               typename std::remove_const_t<std::remove_reference_t<value_type>>::value_type>()
        == value;
}

template <int index, typename attr_t, typename value_type>
auto impl_compare_attribute_value(const attr_t& attribute,
    const value_type& value) -> decltype(std::cbegin(value), value.at(0), attribute[0], true)
{
    return attribute.template get<
               typename std::remove_const_t<std::remove_reference_t<value_type>>::value_type>(index)
        == value;
}

template <int index, typename attr_t, typename T>
bool compare_attribute_value(const attr_t& attribute, const T& values)
{
    auto value = std::get<index>(values);
    return impl_compare_attribute_value<index>(attribute, value);
}

template <typename attr_t, typename T, std::size_t... I>
bool impl_compare_attribute_values(attr_t& attribute, T values, std::index_sequence<I...>)
{
    return (... && compare_attribute_value<I>(attribute, values));
}

template <typename attr_t, typename... Ts>
bool compare_attribute_values(attr_t& attribute, Ts... values)
{
    return impl_compare_attribute_values(
        attribute, std::make_tuple(values...), std::make_index_sequence<sizeof...(values)> {});
}

bool compare_shape(const cdf::Variable& variable, std::initializer_list<uint32_t> values)
{
    return variable.shape() == no_init_vector<uint32_t> { values };
}


template <typename value_type>
bool check_variable(const cdf::Variable& var, std::initializer_list<uint32_t> expected_shape,
    no_init_vector<value_type> ref)
{
    auto values = var.get<value_type>();
    bool is_valid = compare_shape(var, expected_shape);
    auto diff = std::inner_product(std::cbegin(values), std::cend(values), std::cbegin(ref), 0.,
        std::plus<value_type>(), std::minus<value_type>());
    is_valid &= (std::abs(diff) < 1e-9);
    return is_valid;
}

template <typename generator_t>
bool check_variable(
    const cdf::Variable& var, std::initializer_list<uint32_t> expected_shape, generator_t generator)
{
    using value_type = typename decltype(std::declval<generator_t>()(0UL))::value_type;
    auto values = var.get<value_type>();
    bool is_valid = compare_shape(var, expected_shape);
    auto ref = generator(std::size(values));
    auto diff = std::inner_product(std::cbegin(values), std::cend(values), std::cbegin(ref), 0.,
        std::plus<value_type>(), std::minus<value_type>());
    is_valid &= (std::abs(diff) < 1e-9);
    return is_valid;
}

template <typename time_t>
bool check_time_variable(const cdf::Variable& var, std::initializer_list<uint32_t> expected_shape)
{
    using namespace std::chrono;
    auto offset = 0;
    if constexpr (std::is_same_v<time_t, cdf::tt2000_t>)
        offset = 5; // skip 1971 issue with tt2000
    auto values = no_init_vector<decltype(cdf::to_time_point(time_t {}))>();
    std::transform(std::cbegin(var.get<time_t>()), std::cend(var.get<time_t>()),
        std::back_inserter(values), [](const time_t& v) { return cdf::to_time_point(v); });
    bool is_valid = compare_shape(var, expected_shape);
    auto ref = no_init_vector<decltype(cdf::to_time_point(time_t {}))>(std::size(values));
    std::generate(std::begin(ref), std::end(ref),
        [i = 0]() mutable { return time_point<system_clock> {} + hours(24 * (i++ * 180)) + 0ns; });
    is_valid &= std::inner_product(std::begin(ref) + offset, std::end(ref),
        std::begin(values) + offset, is_valid, std::logical_and<>(), std::equal_to<>());
    return is_valid;
}

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

std::size_t filesize(std::fstream& file)
{
    file.seekg(0, file.beg);
    auto pos = file.tellg();
    file.seekg(0, file.end);
    pos = file.tellg() - pos;
    file.seekg(0, file.beg);
    return static_cast<std::size_t>(pos);
}

#define CHECK_CDF_FILE(cd, expected_version, expected_majority, expected_compression)              \
    REQUIRE(cd.distribution_version == expected_version);                                          \
    REQUIRE(cd.majority == expected_majority);                                                     \
    REQUIRE(cd.compression == expected_compression);


#define CHECK_ATTRIBUTES(cd)                                                                       \
    REQUIRE(std::size(cd.attributes) == 8);                                                        \
    REQUIRE(has_attribute(cd, "attr"));                                                            \
    REQUIRE(compare_attribute_values(cd.attributes["attr"], "a cdf text attribute"));              \
    REQUIRE(has_attribute(cd, "attr_float"));                                                      \
    REQUIRE(compare_attribute_values(cd.attributes["attr_float"],                                  \
        no_init_vector<float> { 1.f, 2.f, 3.f }, no_init_vector<float> { 4.f, 5.f, 6.f }));        \
    REQUIRE(has_attribute(cd, "attr_int"));                                                        \
    REQUIRE(compare_attribute_values(cd.attributes["attr_int"],                                    \
        no_init_vector<int8_t> { int8_t { 1 }, int8_t { 2 }, int8_t { 3 } }));                     \
    REQUIRE(has_attribute(cd, "attr_multi"));                                                      \
    REQUIRE(compare_attribute_values(cd.attributes["attr_multi"],                                  \
        no_init_vector<int8_t> { int8_t { 1 }, int8_t { 2 } }, no_init_vector<float> { 2.f, 3.f }, \
        "hello"));                                                                                 \
    REQUIRE(has_attribute(cd, "empty"));                                                           \
    REQUIRE(std::size(cd.attributes["empty"]) == 0UL);                                             \
    REQUIRE(has_attribute(cd, "epoch"))


#define CHECK_VARIABLES(cd)                                                                        \
    REQUIRE(std::size(cd.variables) == 18);                                                        \
    REQUIRE(has_variable(cd, "var"));                                                              \
    REQUIRE(compare_shape(cd.variables["var"], { 101 }));                                          \
    REQUIRE(check_variable(                                                                        \
        cd.variables["var"], { 101 }, cos_gen<double>(3.141592653589793 * 2. / 100.)));            \
    REQUIRE(compare_attribute_values(                                                              \
        cd.variables["var"].attributes["var_attr"], "a variable attribute"));                      \
    REQUIRE(compare_attribute_values(cd.variables["var"].attributes["DEPEND0"], "epoch"));         \
    REQUIRE(has_variable(cd, "epoch"));                                                            \
    REQUIRE(compare_shape(cd.variables["epoch"], { 101 }));                                        \
    REQUIRE(check_time_variable<cdf::epoch>(cd.variables["epoch"], { 101 }));                      \
    REQUIRE(has_variable(cd, "epoch16"));                                                          \
    REQUIRE(compare_shape(cd.variables["epoch16"], { 101 }));                                      \
    REQUIRE(check_time_variable<cdf::epoch16>(cd.variables["epoch16"], { 101 }));                  \
    REQUIRE(has_variable(cd, "tt2000"));                                                           \
    REQUIRE(compare_shape(cd.variables["tt2000"], { 101 }));                                       \
    REQUIRE(check_time_variable<cdf::tt2000_t>(cd.variables["tt2000"], { 101 }));                  \
    REQUIRE(has_variable(cd, "var2d"));                                                            \
    REQUIRE(compare_shape(cd.variables["var2d"], { 3, 4 }));                                       \
    REQUIRE(check_variable(cd.variables["var2d"], { 3, 4 }, ones<double>()));                      \
    REQUIRE(compare_shape(cd.variables["zeros"], { 2048 }));                                       \
    REQUIRE(check_variable(cd.variables["zeros"], { 2048 }, zeros<double>()));                     \
    REQUIRE(compare_shape(cd.variables["bytes"], { 10 }));                                         \
    REQUIRE(check_variable(cd.variables["bytes"], { 10 }, ones<int8_t>()));                        \
    REQUIRE(has_variable(cd, "var3d"));                                                            \
    REQUIRE(compare_shape(cd.variables["var3d"], { 4, 3, 2 }));                                    \
    REQUIRE(check_variable(cd.variables["var3d"], { 4, 3, 2 }, ones<double>()));                   \
    REQUIRE(compare_attribute_values(cd.variables["var3d"].attributes["var3d_attr_multi"],         \
        no_init_vector<double> { 10., 11. }));                                                     \
    REQUIRE(has_variable(cd, "var2d_counter"));                                                    \
    REQUIRE(compare_shape(cd.variables["var2d_counter"], { 10, 10 }));                             \
    REQUIRE(check_variable(cd.variables["var2d_counter"], { 10, 10 },                              \
        [](const auto&)                                                                            \
        {                                                                                          \
            no_init_vector<double> result(100);                                                    \
            std::iota(std::begin(result), std::end(result), 0.);                                   \
            return result;                                                                         \
        }));                                                                                       \
    REQUIRE(check_variable(cd.variables["var_string"], { 1, 16 },                                  \
        no_init_vector<char> {                                                                     \
            'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 'a', ' ', 's', 't', 'r', 'i', 'n', 'g' }));    \
    REQUIRE(check_variable(cd.variables["var_string_uchar"], { 1, 16 },                            \
        no_init_vector<unsigned char> {                                                            \
            'T', 'h', 'i', 's', ' ', 'i', 's', ' ', 'a', ' ', 's', 't', 'r', 'i', 'n', 'g' }))


SCENARIO("Loading cdf files", "[CDF]")
{
    GIVEN("a cdf file")
    {
        WHEN("file doesn't exist")
        {
            THEN("Loading file returns nullopt")
            {
                REQUIRE(cdf::io::load("wrongfile.cdf") == std::nullopt);
            }
        }
        WHEN("file exists but isn't a cdf file")
        {
            THEN("Loading file returns nullopt")
            {
                auto path = std::string(DATA_PATH) + "/not_a_cdf.cdf";
                REQUIRE(file_exists(path));
                REQUIRE(cdf::io::load(path) == std::nullopt);
            }
        }
        WHEN("file exists and is an uncompressed cdf file")
        {
            auto path = std::string(DATA_PATH) + "/a_cdf.cdf";
            REQUIRE(file_exists(path));
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("Version, majority and compression type are retrieved")
            {
                static constexpr auto version
                    = std::tuple<uint32_t, uint32_t, uint32_t> { 3, 9, 0 };
                CHECK_CDF_FILE(
                    cd, version, cdf::cdf_majority::row, cdf::cdf_compression_type::no_compression);
            }
            THEN("All expected attributes are loaded")
            {
                CHECK_ATTRIBUTES(cd);
            }
            THEN("All expected variables are loaded")
            {
                CHECK_VARIABLES(cd);
            }
        }
        WHEN("In memory data as std::vector is a cdf file")
        {
            auto path = std::string(DATA_PATH) + "/a_cdf.cdf";
            REQUIRE(file_exists(path));
            auto data = [&]() -> std::vector<char>
            {
                std::fstream file { path, std::ios::binary | std::ios::in };
                if (file.is_open())
                {
                    std::vector<char> data(filesize(file));
                    file.read(data.data(), static_cast<int64_t>(std::size(data)));
                    return data;
                }
                return {};
            }();
            auto cd_opt = cdf::io::load(data);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("Version and majority are retrieved")
            {
                static constexpr auto version
                    = std::tuple<uint32_t, uint32_t, uint32_t> { 3, 9, 0 };
                CHECK_CDF_FILE(
                    cd, version, cdf::cdf_majority::row, cdf::cdf_compression_type::no_compression);
            }
            THEN("All expected attributes are loaded")
            {
                CHECK_ATTRIBUTES(cd);
            }
            THEN("All expected variables are loaded")
            {
                CHECK_VARIABLES(cd);
            }
        }
        WHEN("In memory data as char* is a cdf file")
        {
            auto path = std::string(DATA_PATH) + "/a_cdf.cdf";
            REQUIRE(file_exists(path));
            THEN("It can be eagerly loaded")
            {
                auto [data, size] = [&]()
                {
                    std::fstream file { path, std::ios::binary | std::ios::in };
                    if (file.is_open())
                    {
                        std::size_t size = filesize(file);
                        char* data = new char[size];
                        file.read(data, static_cast<int64_t>(size));
                        return std::make_tuple(data, size);
                    }
                    return std::make_tuple(static_cast<char*>(nullptr), std::size_t { 0UL });
                }();
                auto cd_opt = cdf::io::load(data, size);
                delete[] data;
                REQUIRE(cd_opt != std::nullopt);
                auto cd = *cd_opt;
                THEN("Version and majority are retrieved")
                {
                    static constexpr auto version
                        = std::tuple<uint32_t, uint32_t, uint32_t> { 3, 9, 0 };
                    CHECK_CDF_FILE(cd, version, cdf::cdf_majority::row,
                        cdf::cdf_compression_type::no_compression);
                }
                THEN("All expected attributes are loaded")
                {
                    CHECK_ATTRIBUTES(cd);
                }
                THEN("All expected variables are loaded")
                {
                    CHECK_VARIABLES(cd);
                }
            }
            THEN("It can be lazy loaded")
            {
                auto [data, size] = [&]()
                {
                    std::fstream file { path, std::ios::binary | std::ios::in };
                    if (file.is_open())
                    {
                        std::size_t size = filesize(file);
                        char* data = new char[size];
                        file.read(data, static_cast<int64_t>(size));
                        return std::make_tuple(data, size);
                    }
                    return std::make_tuple(static_cast<char*>(nullptr), std::size_t { 0UL });
                }();
                auto cd_opt = cdf::io::load(data, size, true);
                REQUIRE(cd_opt != std::nullopt);
                auto cd = *cd_opt;
                THEN("Version and majority are retrieved")
                {
                    static constexpr auto version
                        = std::tuple<uint32_t, uint32_t, uint32_t> { 3, 9, 0 };
                    CHECK_CDF_FILE(cd, version, cdf::cdf_majority::row,
                        cdf::cdf_compression_type::no_compression);
                }
                THEN("All expected attributes are loaded")
                {
                    CHECK_ATTRIBUTES(cd);
                }
                THEN("All expected variables are loaded")
                {
                    CHECK_VARIABLES(cd);
                }
                delete[] data;
            }
        }
        WHEN("file exists and is a compressed cdf file (GZIP)")
        {
            auto path = std::string(DATA_PATH) + "/a_compressed_cdf.cdf";
            REQUIRE(file_exists(path));
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("Version and majority are retrieved")
            {
                static constexpr auto version
                    = std::tuple<uint32_t, uint32_t, uint32_t> { 3, 9, 0 };
                CHECK_CDF_FILE(cd, version, cdf::cdf_majority::row,
                    cdf::cdf_compression_type::gzip_compression);
            }
            THEN("All expected attributes are loaded")
            {
                CHECK_ATTRIBUTES(cd);
            }
            THEN("All expected variables are loaded")
            {
                CHECK_VARIABLES(cd);
            }
        }
        WHEN("file exists and is a compressed cdf file (RLE)")
        {
            auto path = std::string(DATA_PATH) + "/a_rle_compressed_cdf.cdf";
            REQUIRE(file_exists(path));
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("Version and majority are retrieved")
            {
                static constexpr auto version
                    = std::tuple<uint32_t, uint32_t, uint32_t> { 3, 9, 0 };
                CHECK_CDF_FILE(cd, version, cdf::cdf_majority::row,
                    cdf::cdf_compression_type::rle_compression);
            }
            THEN("All expected attributes are loaded")
            {
                CHECK_ATTRIBUTES(cd);
            }
            THEN("All expected variables are loaded")
            {
                CHECK_VARIABLES(cd);
            }
        }
        WHEN("file exists and is a cdf file with compressed variables")
        {
            auto path = std::string(DATA_PATH) + "/a_cdf_with_compressed_vars.cdf";
            REQUIRE(file_exists(path));
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("Version and majority are retrieved")
            {
                static constexpr auto version
                    = std::tuple<uint32_t, uint32_t, uint32_t> { 3, 9, 0 };
                CHECK_CDF_FILE(
                    cd, version, cdf::cdf_majority::row, cdf::cdf_compression_type::no_compression);
            }
            THEN("All expected attributes are loaded")
            {
                CHECK_ATTRIBUTES(cd);
            }
            THEN("All expected variables are loaded")
            {
                CHECK_VARIABLES(cd);
            }
        }
        WHEN("file exists and is a column major cdf file")
        {
            auto path = std::string(DATA_PATH) + "/a_col_major_cdf.cdf";
            REQUIRE(file_exists(path));
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("Version and majority are retrieved")
            {
                static constexpr auto version
                    = std::tuple<uint32_t, uint32_t, uint32_t> { 3, 9, 0 };
                CHECK_CDF_FILE(cd, version, cdf::cdf_majority::column,
                    cdf::cdf_compression_type::no_compression);
            }
            THEN("All expected attributes are loaded")
            {
                CHECK_ATTRIBUTES(cd);
            }
            THEN("All expected variables are loaded")
            {
                CHECK_VARIABLES(cd);
            }
        }
        WHEN("file is a 2.4.x cdf")
        {
            auto path = std::string(DATA_PATH) + "/ia_k0_epi_19970102_v01.cdf";
            REQUIRE(file_exists(path));
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("All expected variables are found")
            {
                REQUIRE(std::size(cd.variables) == 10);
            }
        }
        WHEN("file is a 2.4.x cdf")
        {
            auto path = std::string(DATA_PATH) + "/ge_k0_cpi_19921231_v02.cdf";
            REQUIRE(file_exists(path));
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("All expected variables are found")
            {
                REQUIRE(std::size(cd.variables) == 25);
            }
        }
        WHEN("file is a 2.5.x cdf")
        {
            auto path = std::string(DATA_PATH) + "/ac_h2_sis_20101105_v06.cdf";
            REQUIRE(file_exists(path));
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("All expected variables are found")
            {
                REQUIRE(std::size(cd.variables) == 61);
            }
        }
    }
}
