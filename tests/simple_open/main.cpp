#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "attribute.hpp"
#include "cdf-file.hpp"
#include "cdf-io/cdf-io.hpp"

#define CATCH_CONFIG_MAIN
#if __has_include(<catch2/catch.hpp>)
#include <catch2/catch.hpp>
#else
#include <catch.hpp>
#endif


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
    return attribute.get<std::string>(index) == std::string { value };
}

template <int index, typename value_type>
std::enable_if_t<std::is_scalar_v<value_type> && !std::is_same_v<value_type, const char*>, bool>
impl_compare_attribute_value(const cdf::Attribute& attribute, const value_type& value)
{
    return attribute.get<value_type>(index) == std::vector<value_type> { value };
}

template <int index, typename value_type>
auto impl_compare_attribute_value(const cdf::Attribute& attribute, const value_type& value)
    -> decltype(std::cbegin(value), value.at(0), true)
{
    return attribute
               .get<typename std::remove_const_t<std::remove_reference_t<value_type>>::value_type>(
                   index)
        == value;
}

template <int index, typename T>
bool compare_attribute_value(const cdf::Attribute& attribute, const T& values)
{
    auto value = std::get<index>(values);
    return impl_compare_attribute_value<index>(attribute, value);
}

template <typename T, std::size_t... I>
bool impl_compare_attribute_values(cdf::Attribute& attribute, T values, std::index_sequence<I...>)
{
    return (... && compare_attribute_value<I>(attribute, values));
}

template <typename... Ts>
bool compare_attribute_values(cdf::Attribute& attribute, Ts... values)
{
    return impl_compare_attribute_values(
        attribute, std::make_tuple(values...), std::make_index_sequence<sizeof...(values)> {});
}

bool compare_shape(const cdf::Variable& variable, std::initializer_list<uint32_t> values)
{
    return variable.shape() == std::vector<uint32_t> { values };
}

template <typename generator_t>
bool check_variable(
    const cdf::Variable& var, std::initializer_list<uint32_t> expected_shape, generator_t generator)
{
    auto values = var.get<double>();
    bool is_valid = compare_shape(var, expected_shape);
    auto ref = generator(std::size(values));
    auto diff = std::inner_product(std::cbegin(values), std::cend(values), std::cbegin(ref), 0.,
        std::plus<double>(), std::minus<double>());
    is_valid &= (std::abs(diff) < 1e-9);
    return is_valid;
}

template <typename T>
struct cos_gen
{
    const T step;
    cos_gen(T step) : step { step } {}
    std::vector<T> operator()(std::size_t size)
    {
        std::vector<T> values(size);
        std::generate(std::begin(values), std::end(values), [i = T(0.), step = step]() mutable {
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
    std::vector<T> operator()(std::size_t size)
    {
        std::vector<T> values(size);
        std::generate(std::begin(values), std::end(values), []() mutable { return T(1); });
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

SCENARIO("Loading a cdf file", "[CDF]")
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
                REQUIRE(std::filesystem::exists(path));
                REQUIRE(cdf::io::load(path) == std::nullopt);
            }
        }
        WHEN("file exists and is a cdf file")
        {
            auto path = std::string(DATA_PATH) + "/a_cdf.cdf";
            REQUIRE(std::filesystem::exists(path));
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("All expected attributes are loaded")
            {
                REQUIRE(std::size(cd.attributes) == 5);
                REQUIRE(has_attribute(cd, "attr"));
                REQUIRE(compare_attribute_values(cd.attributes["attr"], "a cdf text attribute"));
                REQUIRE(has_attribute(cd, "attr_float"));
                REQUIRE(compare_attribute_values(cd.attributes["attr_float"],
                    std::vector { 1.f, 2.f, 3.f }, std::vector { 4.f, 5.f, 6.f }));
                REQUIRE(has_attribute(cd, "attr_int"));
                REQUIRE(compare_attribute_values(cd.attributes["attr_int"],
                    std::vector { int8_t { 1 }, int8_t { 2 }, int8_t { 3 } }));
                REQUIRE(has_attribute(cd, "attr_multi"));
                REQUIRE(compare_attribute_values(cd.attributes["attr_multi"],
                    std::vector { int8_t { 1 }, int8_t { 2 } }, std::vector { 2.f, 3.f }, "hello"));
                REQUIRE(has_attribute(cd, "empty"));
                REQUIRE(cd.attributes["empty"].len() == 0UL);
            }
            THEN("All expected variables are loaded")
            {
                REQUIRE(std::size(cd.variables) == 5);
                REQUIRE(has_variable(cd, "var"));
                REQUIRE(compare_shape(cd.variables["var"], { 101 }));
                REQUIRE(check_variable(
                    cd.variables["var"], { 101 }, cos_gen<double>(3.141592653589793 * 2. / 100.)));
                REQUIRE(compare_attribute_values(
                    cd.variables["var"].attributes["var_attr"], "a variable attribute"));
                REQUIRE(
                    compare_attribute_values(cd.variables["var"].attributes["DEPEND0"], "epoch"));
                REQUIRE(has_variable(cd, "epoch"));
                REQUIRE(compare_shape(cd.variables["epoch"], { 101 }));
                REQUIRE(has_variable(cd, "var2d"));
                REQUIRE(compare_shape(cd.variables["var2d"], { 3, 4 }));
                REQUIRE(check_variable(cd.variables["var2d"], { 3, 4 }, ones<double>()));
                REQUIRE(has_variable(cd, "var3d"));
                REQUIRE(compare_shape(cd.variables["var3d"], { 4, 3, 2 }));
                REQUIRE(check_variable(cd.variables["var3d"], { 4, 3, 2 }, ones<double>()));
                REQUIRE(
                    compare_attribute_values(cd.variables["var3d"].attributes["var3d_attr_multi"],
                        std::vector { 10., 11. }));

                REQUIRE(has_variable(cd, "var2d_counter"));
                REQUIRE(compare_shape(cd.variables["var2d_counter"], { 2, 2 }));
                REQUIRE(check_variable(cd.variables["var2d_counter"], { 2, 2 }, [](const auto&) {
                    return std::vector<double> { 1., 2., 3., 4. };
                }));
            }
        }
        WHEN("In memory data as std::vector is a cdf file")
        {
            auto path = std::string(DATA_PATH) + "/a_cdf.cdf";
            REQUIRE(std::filesystem::exists(path));
            auto data = [&]() -> std::vector<char> {
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
            THEN("All expected attributes are loaded") { REQUIRE(std::size(cd.attributes) == 5); }
            THEN("All expected variables are loaded") { REQUIRE(std::size(cd.variables) == 5); }
        }
        WHEN("In memory data as char* is a cdf file")
        {
            auto path = std::string(DATA_PATH) + "/a_cdf.cdf";
            REQUIRE(std::filesystem::exists(path));
            auto [data, size] = [&]() {
                std::fstream file { path, std::ios::binary | std::ios::in };
                if (file.is_open())
                {
                    std::size_t size = filesize(file);
                    char* data = new char[size];
                    file.read(data, static_cast<int64_t>(size));
                    return std::make_tuple(data, size);
                }
                return std::make_tuple(static_cast<char*>(nullptr), 0UL);
            }();
            auto cd_opt = cdf::io::load(data, size);
            delete[] data;
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("All expected attributes are loaded") { REQUIRE(std::size(cd.attributes) == 5); }
            THEN("All expected variables are loaded") { REQUIRE(std::size(cd.variables) == 5); }
        }
        WHEN("file exists and is a cdf file")
        {
            auto path = std::string(DATA_PATH) + "/a_compressed_cdf.cdf";
            REQUIRE(std::filesystem::exists(path));
            auto cd_opt = cdf::io::load(path);
            REQUIRE(cd_opt != std::nullopt);
            auto cd = *cd_opt;
            THEN("All expected attributes are loaded")
            {
                REQUIRE(std::size(cd.attributes) == 5);
                REQUIRE(has_attribute(cd, "attr"));
                REQUIRE(compare_attribute_values(cd.attributes["attr"], "a cdf text attribute"));
                REQUIRE(has_attribute(cd, "attr_float"));
                REQUIRE(compare_attribute_values(cd.attributes["attr_float"],
                    std::vector { 1.f, 2.f, 3.f }, std::vector { 4.f, 5.f, 6.f }));
                REQUIRE(has_attribute(cd, "attr_int"));
                REQUIRE(compare_attribute_values(cd.attributes["attr_int"],
                    std::vector { int8_t { 1 }, int8_t { 2 }, int8_t { 3 } }));
                REQUIRE(has_attribute(cd, "attr_multi"));
                REQUIRE(compare_attribute_values(cd.attributes["attr_multi"],
                    std::vector { int8_t { 1 }, int8_t { 2 } }, std::vector { 2.f, 3.f }, "hello"));
                REQUIRE(has_attribute(cd, "empty"));
                REQUIRE(cd.attributes["empty"].len() == 0UL);
            }
            THEN("All expected variables are loaded")
            {
                REQUIRE(std::size(cd.variables) == 5);
                REQUIRE(has_variable(cd, "var"));
                REQUIRE(compare_shape(cd.variables["var"], { 101 }));
                REQUIRE(check_variable(
                    cd.variables["var"], { 101 }, cos_gen<double>(3.141592653589793 * 2. / 100.)));
                REQUIRE(compare_attribute_values(
                    cd.variables["var"].attributes["var_attr"], "a variable attribute"));
                REQUIRE(
                    compare_attribute_values(cd.variables["var"].attributes["DEPEND0"], "epoch"));
                REQUIRE(has_variable(cd, "epoch"));
                REQUIRE(compare_shape(cd.variables["epoch"], { 101 }));
                REQUIRE(has_variable(cd, "var2d"));
                REQUIRE(compare_shape(cd.variables["var2d"], { 3, 4 }));
                REQUIRE(check_variable(cd.variables["var2d"], { 3, 4 }, ones<double>()));
                REQUIRE(has_variable(cd, "var3d"));
                REQUIRE(compare_shape(cd.variables["var3d"], { 4, 3, 2 }));
                REQUIRE(check_variable(cd.variables["var3d"], { 4, 3, 2 }, ones<double>()));
                REQUIRE(
                    compare_attribute_values(cd.variables["var3d"].attributes["var3d_attr_multi"],
                        std::vector { 10., 11. }));
            }
        }
    }
}
