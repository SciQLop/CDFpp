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

template<int index, typename T>
bool compare_attribute_value(const cdf::Attribute& attribute , const T& values)
{
    auto value = std::get<index>(values);
    using value_type=std::remove_const_t<decltype (value)>;
    if constexpr(std::is_same_v<const char*,value_type >)
        return attribute.get<std::string>(index) ==  std::string{value} ;
    else
        return attribute.get<std::vector<value_type>>(index) ==  std::vector<value_type>{value};
}

template <typename T, std::size_t... I>
bool impl_compare_attribute_values(cdf::Attribute& attribute, T values, std::index_sequence<I...>)
{
    return  (... && compare_attribute_value<I>(attribute, values));
}

template <typename... Ts>
bool compare_attribute_values(cdf::Attribute& attribute, Ts... values)
{
    return impl_compare_attribute_values(attribute, std::make_tuple(values...), std::make_index_sequence<sizeof...(values)>{});
}

bool compare_shape(const cdf::Variable& variable, std::initializer_list<uint32_t> values)
{
    return variable.shape == std::vector<uint32_t>{values};
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
                REQUIRE(has_attribute(cd,"attr"));
                REQUIRE(std::size(cd.attributes) == 4);
                REQUIRE(compare_attribute_values(cd.attributes["attr"], "a cdf text attribute"));
                REQUIRE(has_attribute(cd,"attr_float"));
                REQUIRE(compare_attribute_values(cd.attributes["attr_float"], 1.f, 2.f, 3.f));
                REQUIRE(has_attribute(cd, "attr_int"));
                REQUIRE(compare_attribute_values(cd.attributes["attr_int"], int8_t{1}, int8_t{2}, int8_t{3}));
                REQUIRE(has_attribute(cd, "attr_multi"));
                REQUIRE(compare_attribute_values(cd.attributes["attr_multi"], int8_t{1}, 2.f, "hello"));
            }
            THEN("All expected variables are loaded")
            {
                REQUIRE(std::size(cd.variables) == 4);
                REQUIRE(has_variable(cd, "var"));
                REQUIRE(compare_shape(cd.variables["var"], {}));
                auto& var_data = cd.variables["var"].get<std::vector<double>>();
                REQUIRE(std::size(var_data)==101);
                REQUIRE(has_variable(cd, "epoch"));
                REQUIRE(compare_shape(cd.variables["epoch"],{}));
                REQUIRE(has_variable(cd, "var2d"));
                REQUIRE(compare_shape(cd.variables["var2d"], {4}));
                auto& var2d_data = cd.variables["var2d"].get<std::vector<double>>();
                REQUIRE(std::size(var2d_data)==3*4);
                REQUIRE(has_variable(cd, "var3d"));
                REQUIRE(compare_shape(cd.variables["var3d"], {3, 2}));
                auto& var3d_data = cd.variables["var3d"].get<std::vector<double>>();
                REQUIRE(std::size(var3d_data)==4*3*2);
            }
        }
    }
}
