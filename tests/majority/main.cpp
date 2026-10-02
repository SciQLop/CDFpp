#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "cdfpp/cdf-io/majority-swap.hpp"
#include "vector"
#include <array>
#include <cstdint>
#include <numeric>
#include <random>


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

SCENARIO("Swapping from row to column major undoes the opposite swap", "[CDF]")
{
    GIVEN("an array of 2 records of 3x4x5 values")
    {
        std::vector<double> input(2 * 3 * 4 * 5);
        std::iota(std::begin(input), std::end(input), 0.);
        const auto original = input;
        WHEN("swapping it to column major and back to row major")
        {
            cdf::majority::swap<false, std::array<int, 4>, std::vector<double>, true>(
                input, std::array { 2, 3, 4, 5 });
            THEN("the first record is in column major order")
            {
                REQUIRE(input[1] == 20.);
                REQUIRE(input[2] == 40.);
                REQUIRE(input[3] == 5.);
            }
            cdf::majority::swap<false>(input, std::array { 2, 3, 4, 5 });
            THEN("the array is back") { REQUIRE(input == original); }
        }
    }
}

namespace
{
// Element `index` of a record of `dims` in row major order lands at the returned position in
// column major order: the oracle the swaps are checked against.
std::size_t column_major_position(std::size_t index, const std::vector<uint32_t>& dims)
{
    std::size_t position = 0, column_stride = 1;
    std::vector<std::size_t> nd(std::size(dims));
    for (auto d = std::size(dims); d-- > 0;)
    {
        nd[d] = index % dims[d];
        index /= dims[d];
    }
    for (auto d = 0UL; d < std::size(dims); d++)
    {
        position += nd[d] * column_stride;
        column_stride *= dims[d];
    }
    return position;
}

// `shape` is records first; strings keep their last dimension, the characters, together.
template <typename T>
std::vector<T> to_column_major_reference(
    const std::vector<T>& row, const std::vector<uint32_t>& shape, bool is_string)
{
    const std::vector<uint32_t> dims(
        std::cbegin(shape) + 1, std::cend(shape) - (is_string ? 1 : 0));
    const std::size_t element = is_string ? shape.back() : 1;
    const std::size_t per_record = std::accumulate(
        std::cbegin(dims), std::cend(dims), std::size_t { 1 }, std::multiplies<>());
    std::vector<T> column(std::size(row));
    for (std::size_t r = 0; r < shape[0]; r++)
        for (std::size_t i = 0; i < per_record; i++)
            std::copy_n(row.data() + (r * per_record + i) * element, element,
                column.data() + (r * per_record + column_major_position(i, dims)) * element);
    return column;
}

// Sizes that divide into the SIMD blocks, and some that don't.
constexpr std::array<uint32_t, 7> dimension_sizes { 1, 2, 3, 4, 5, 8, 16 };

template <typename T, bool is_string, cdf::CDF_Types type>
void check_random_shapes()
{
    std::mt19937 rng { 7 };
    for (int trial = 0; trial < 300; trial++)
    {
        std::vector<uint32_t> shape { 1 + rng() % 3 };
        const auto record_dims = 2 + rng() % 3;
        for (auto d = 0U; d < record_dims; d++)
            shape.push_back(dimension_sizes[rng() % std::size(dimension_sizes)]);
        if (is_string)
            shape.push_back(1 + rng() % 4);
        const std::size_t size = std::accumulate(
            std::cbegin(shape), std::cend(shape), std::size_t { 1 }, std::multiplies<>());
        std::vector<T> row(size);
        std::iota(std::begin(row), std::end(row), T {});
        const auto expected = to_column_major_reference(row, shape, is_string);
        cdf::data_t copied { no_init_vector<T>(size), type };
        cdf::majority::copy_to_column_major(reinterpret_cast<const char*>(row.data()), copied,
            no_init_vector<uint32_t>(std::cbegin(shape), std::cend(shape)));
        REQUIRE(std::equal(std::cbegin(expected), std::cend(expected),
            reinterpret_cast<const T*>(copied.bytes_ptr())));
        auto values = row;
        cdf::majority::swap<is_string, std::vector<uint32_t>, std::vector<T>, true>(values, shape);
        REQUIRE(values == expected);
        cdf::majority::swap<is_string>(values, shape);
        REQUIRE(values == row);
    }
}
}

SCENARIO("Majority swaps match a plain transposition on random shapes", "[CDF]")
{
    check_random_shapes<float, false, cdf::CDF_Types::CDF_FLOAT>();
    check_random_shapes<double, false, cdf::CDF_Types::CDF_DOUBLE>();
    check_random_shapes<uint16_t, false, cdf::CDF_Types::CDF_UINT2>();
    check_random_shapes<char, true, cdf::CDF_Types::CDF_CHAR>();
}
