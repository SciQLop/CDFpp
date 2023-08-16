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
#include "cdfpp/variable.hpp"


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
    auto cdf_path = std::tmpnam(nullptr);

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
}
