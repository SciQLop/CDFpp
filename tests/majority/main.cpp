#if __has_include(<catch2/catch_all.hpp>)
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#else
#include <catch.hpp>
#endif
#include "cdfpp/cdf-io/majority-swap.hpp"
#include "vector"


SCENARIO("Swapping from col to row major", "[CDF]")
{
    GIVEN("a column major array")
    {
        std::vector<double> input { 0., 3., 6., 1., 4., 7., 2., 5., 8., 9., 12., 15., 10., 13., 16.,
            11., 14., 17., 18., 21., 24., 19., 22., 25., 20., 23., 26. };
        WHEN("Swapping to row major")
        {
            cdf::majority::swap(input, std::array { 3, 3, 3 });
            THEN("array should be row major")
            {
                REQUIRE(input
                    == std::vector<double> { 0., 1., 2., 3., 4., 5., 6., 7., 8., 9., 10., 11., 12.,
                        13., 14., 15., 16., 17., 18., 19., 20., 21., 22., 23., 24., 25., 26. });
            }
            WHEN("Swapping again")
            {
                cdf::majority::swap(input, std::array { 3, 3, 3 });
                THEN("array should be back to column major")
                {
                    REQUIRE(input
                        == std::vector<double> { 0., 3., 6., 1., 4., 7., 2., 5., 8., 9., 12., 15.,
                            10., 13., 16., 11., 14., 17., 18., 21., 24., 19., 22., 25., 20., 23.,
                            26. });
                }
            }
        }
    }
}
