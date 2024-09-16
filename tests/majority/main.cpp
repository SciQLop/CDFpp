#if __has_include(<catch2/catch_all.hpp>)
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#else
#include <catch.hpp>
#endif
#include "cdfpp/cdf-io/majority-swap.hpp"
#include "vector"


SCENARIO("Generating flat indexes")
{
    {
        std::array index = { 0, 0, 0, 0 };
        std::array shape = { 2, 3, 4, 5 };
        REQUIRE(cdf::majority::flat_index(index, shape) == 0);
        REQUIRE(cdf::majority::inverted_flat_index(index, shape) == 0);
    }
    {
        std::array index = { 3, 3, 3 };
        std::array shape = { 4, 4, 4 };
        REQUIRE(cdf::majority::flat_index(index, shape) == 63);
        REQUIRE(cdf::majority::inverted_flat_index(index, shape) == 63);
    }
    {
        std::array index = { 1, 1 };
        std::array shape = { 4, 4 };
        REQUIRE(cdf::majority::flat_index(index, shape) == 5);
        REQUIRE(cdf::majority::inverted_flat_index(index, shape) == 5);
    }
    {
        std::array index = { 1, 2 };
        std::array shape = { 3, 4 };
        REQUIRE(cdf::majority::flat_index(index, shape) == 7);
        REQUIRE(cdf::majority::inverted_flat_index(index, shape) == 6);
    }
    {
        std::array index = { 1, 1, 1, 1 };
        std::array shape = { 2, 3, 4, 5 };

        REQUIRE(cdf::majority::flat_index(index, shape) == 33);
        REQUIRE(cdf::majority::inverted_flat_index(index, shape) == 86);
    }
}


SCENARIO("Swapping from col to row major", "[CDF]")
{
    GIVEN("a column major array")
    {
        // clang-format off
        std::vector<double> input {
              1.,  21.,   0.,
              6.,   0.,   0.,
             11.,   0.,   0.,
             16.,   0.,   0.,

              2.,   0.,   0.,
              7.,   0.,   0.,
             12.,   0.,   0.,
             17.,   0.,   0.,

              3.,   0.,   0.,
              8.,   0.,   0.,
             13.,   0.,   0.,
             18.,   0.,   0.,

              4.,   0.,   0.,
              9.,   0.,   0.,
             14.,   0.,   0.,
             19.,   0.,   0.,

              5.,   0.,   0.,
             10.,   0.,   0.,
             15.,   0.,   0.,
             20.,   0.,   0.,

            111., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
            0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
            0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0. };
        // clang-format on
        WHEN("Swapping to row major")
        {
            cdf::majority::swap<false>(input, std::array { 2, 3, 4, 5 });
            THEN("array should be row major")
            {
                // clang-format off
                REQUIRE(input
    == std::vector<double> {
              1.,   2.,   3.,
              4.,   5.,   6.,
              7.,   8,    9.,
             10.,  11.,  12.,

             13.,  14.,  15.,
             16.,  17.,  18.,
             19.,  20.,  21.,
              0.,   0.,   0.,

              0.,   0.,   0.,
              0.,   0.,   0.,
              0.,   0.,   0.,
              0.,   0.,   0.,

              0.,   0.,   0.,
              0.,   0.,   0.,
              0.,   0.,   0.,
              0.,   0.,   0.,

              0.,   0.,   0.,
              0.,   0.,   0.,
              0.,   0.,   0.,
              0.,   0.,   0.,

                        111., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
                        0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
                        0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.,
                        0., 0., 0., 0. });
                // clang-format on
            }
        }
    }
}
