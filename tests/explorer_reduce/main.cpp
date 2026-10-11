#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "wacdfpp/reduce.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

using namespace wacdfpp;

namespace
{
constexpr double unset = std::numeric_limits<double>::quiet_NaN();
constexpr value_mask no_mask { unset, unset, unset };

// values[r][a][b] = 100*r + 10*a + b, shape (2, 2, 3)
std::vector<int16_t> grid()
{
    std::vector<int16_t> v;
    for (int r = 0; r < 2; ++r)
        for (int a = 0; a < 2; ++a)
            for (int b = 0; b < 3; ++b)
                v.push_back(static_cast<int16_t>(100 * r + 10 * a + b));
    return v;
}

const std::vector<std::size_t> grid_shape { 2, 2, 3 };

std::vector<double> run(
    std::vector<int32_t> modes, reduce_op op = reduce_op::sum, value_mask mask = no_mask)
{
    const auto values = grid();
    return reduce<int16_t>(values, grid_shape, modes, op, mask);
}
}

SCENARIO("Reducing a record-varying array to one row per record")
{
    REQUIRE(run({ keep, keep }) == std::vector<double> { 0, 1, 2, 10, 11, 12, 100, 101, 102, 110, 111, 112 });
    REQUIRE(run({ collapse, keep }) == std::vector<double> { 10, 12, 14, 210, 212, 214 });
    REQUIRE(run({ keep, collapse }) == std::vector<double> { 3, 33, 303, 333 });
    REQUIRE(run({ collapse, collapse }) == std::vector<double> { 36, 636 });
    REQUIRE(run({ collapse, keep }, reduce_op::mean) == std::vector<double> { 5, 6, 7, 105, 106, 107 });
    REQUIRE(run({ collapse, collapse }, reduce_op::mean) == std::vector<double> { 6, 106 });
    REQUIRE(run({ 1, keep }) == std::vector<double> { 10, 11, 12, 110, 111, 112 });
    REQUIRE(run({ keep, 2 }) == std::vector<double> { 2, 12, 102, 112 });
    REQUIRE(run({ 0, collapse }, reduce_op::mean) == std::vector<double> { 1, 101 });
}

SCENARIO("Invalid values are left out of sums and means")
{
    WHEN("a value equals FILLVAL")
    {
        REQUIRE(run({ keep, collapse }, reduce_op::sum, { 1, unset, unset })
            == std::vector<double> { 2, 33, 303, 333 });
        REQUIRE(run({ keep, collapse }, reduce_op::mean, { 1, unset, unset })
            == std::vector<double> { 1, 11, 101, 111 });
    }
    WHEN("values fall outside VALIDMIN/VALIDMAX")
    {
        REQUIRE(run({ keep, collapse }, reduce_op::sum, { unset, 2, 110 })
            == std::vector<double> { 2, 33, 303, 110 });
    }
    WHEN("every value of an output cell is invalid")
    {
        const auto out = run({ keep, keep }, reduce_op::sum, { unset, 5, unset });
        REQUIRE(std::isnan(out[0]));
        REQUIRE(out[3] == 10);
    }
    WHEN("a float32 FILLVAL is given as a double")
    {
        const std::vector<float> values { -1e31f, 2.f };
        const std::vector<std::size_t> shape { 1, 2 };
        const std::vector<int32_t> modes { collapse };
        REQUIRE(reduce<float>(values, shape, modes, reduce_op::sum, { -1e31, unset, unset })
            == std::vector<double> { 2 });
    }
    WHEN("values are NaN")
    {
        const std::vector<float> values { 1.f, std::nanf(""), 2.f, 3.f };
        const std::vector<std::size_t> shape { 2, 2 };
        const std::vector<int32_t> modes { collapse };
        REQUIRE(reduce<float>(values, shape, modes, reduce_op::sum, no_mask)
            == std::vector<double> { 1, 5 });
    }
}

SCENARIO("Long runs are summed across SIMD batches and the scalar tail")
{
    std::vector<double> values(2 * 37);
    std::iota(std::begin(values), std::end(values), 0.);
    values[5] = -1e31;
    values[36] = std::numeric_limits<double>::infinity();
    values[40] = 1e9;
    const std::vector<std::size_t> shape { 2, 37 };
    const std::vector<int32_t> modes { collapse };
    const auto expected_sum = [&](std::size_t r)
    {
        double sum = 0.;
        for (std::size_t i = 37 * r; i < 37 * (r + 1); ++i)
            if (std::isfinite(values[i]) && values[i] != -1e31 && values[i] <= 1e6)
                sum += values[i];
        return sum;
    };
    const value_mask mask { -1e31, unset, 1e6 };
    REQUIRE(reduce<double>(values, shape, modes, reduce_op::sum, mask)
        == std::vector<double> { expected_sum(0), expected_sum(1) });
    REQUIRE(reduce<double>(values, shape, modes, reduce_op::mean, mask)
        == std::vector<double> { expected_sum(0) / 35, expected_sum(1) / 36 });
}

SCENARIO("Bad reduction requests are rejected")
{
    REQUIRE_THROWS_AS(run({ keep }), std::invalid_argument);
    REQUIRE_THROWS_AS(run({ keep, 3 }), std::out_of_range);
    REQUIRE_THROWS_AS(run({ keep, -3 }), std::invalid_argument);
}

SCENARIO("Variables with no records reduce to nothing")
{
    const std::vector<int16_t> values;
    const std::vector<std::size_t> shape { 0, 2, 3 };
    const std::vector<int32_t> modes { collapse, keep };
    REQUIRE(reduce<int16_t>(values, shape, modes, reduce_op::sum, no_mask).empty());
}
