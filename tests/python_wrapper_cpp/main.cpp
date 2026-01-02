#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cdfpp/attribute.hpp>
#include <cdfpp/cdf-map.hpp>
#include <cdfpp/cdf-repr.hpp>
#include <cdfpp/no_init_vector.hpp>
#include <cdfpp/variable.hpp>
#include <cdfpp_config.h>

#include "pycdfpp/collections.hpp"
#include "pycdfpp/data_types.hpp"
#include "pycdfpp/enums.hpp"

using namespace cdf;

#include <pybind11/chrono.h>
#include <pybind11/embed.h>
#include <pybind11/iostream.h>
#include <pybind11/native_enum.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <datetime.h>

namespace py = pybind11;


PYBIND11_EMBEDDED_MODULE(test_module, m)
{
    if (!PyDateTimeAPI)
        PyDateTime_IMPORT;
    def_enums_wrappers(m);
    py::native_enum<BestTypeId>(m, "BestTypeId", "enum.Enum")
        .value("NONE", BestTypeId::None)
        .value("DateTime", BestTypeId::DateTime)
        .value("Float", BestTypeId::Float)
        .value("Int8", BestTypeId::Int8)
        .value("UInt8", BestTypeId::UInt8)
        .value("Int16", BestTypeId::Int16)
        .value("UInt16", BestTypeId::UInt16)
        .value("Int32", BestTypeId::Int32)
        .value("UInt32", BestTypeId::UInt32)
        .value("Int64", BestTypeId::Int64)
        .export_values()
        .finalize();

    m.def("analyze_collection_cdf_type",
        [](py::list& input)
        {
            auto r = analyze_collection(input);
            return r.inferred_cdf_type;
        });
    m.def("analyze_collection_cdf_shape",
        [](py::list& input)
        {
            auto r = analyze_collection(input);
            return r.shape;
        });
}

SCENARIO("Testing analyze_collection function", "[CDF]")
{
    using Catch::Matchers::ContainsSubstring;
    auto run_test = [](auto values)
    {
        py::gil_scoped_acquire_simple acquire;
        py::exec(fmt::format(R"(
from datetime import datetime
import test_module
if globals().get("values") is not None:
    del values
if globals().get("cdf_type") is not None:
    del cdf_type
if globals().get("shape") is not None:
    del shape
values = {}
cdf_type = test_module.analyze_collection_cdf_type(values)
shape = test_module.analyze_collection_cdf_shape(values)
    )",
            values));
        CDF_Types cdf_type = py::globals()["cdf_type"].cast<CDF_Types>();
        std::vector<std::size_t> shape = py::globals()["shape"].cast<std::vector<std::size_t>>();
        return std::make_pair(cdf_type, shape);
    };

    py::scoped_interpreter guard {};
    GIVEN("A bunch of well formed collections")
    {
        WHEN("Analyzing an empty list")
        {
            auto [cdf_type, shape] = run_test("[]");
            THEN("The inferred type is CDF_NONE and shape is empty")
            {
                REQUIRE(cdf_type == CDF_Types::CDF_NONE);
                REQUIRE(shape.empty());
            }
        }
        WHEN("Analyzing a simple list of integers")
        {
            auto [cdf_type, shape] = run_test("[1, 2, 3, 4]");
            THEN("The inferred type is CDF_UINT1 and shape is [4]")
            {
                REQUIRE(cdf_type == CDF_Types::CDF_UINT1);
                REQUIRE(shape == std::vector<std::size_t> { 4 });
            }
        }
        WHEN("Analyzing an ND list of integers")
        {
            auto [cdf_type, shape] = run_test(
                "[[[1, 2, 3], [4, 5, 6], [7, 8, 9]], [[10, 11, 12], [13, 14, 15], [16, 17, 18]]]");
            THEN("The inferred type is CDF_UINT1 and shape is [2, 3, 3]")
            {
                REQUIRE(cdf_type == CDF_Types::CDF_UINT1);
                REQUIRE(shape == std::vector<std::size_t> { 2, 3, 3 });
            }
        }
        WHEN("Analyzing a list of mixed list and tuples")
        {
            auto [cdf_type, shape] = run_test("[[1, 2, 3], (4, 5, 6), [7, 8, 9]]");
            THEN("The inferred type is CDF_UINT1 and shape is [3, 3]")
            {
                REQUIRE(cdf_type == CDF_Types::CDF_UINT1);
                REQUIRE(shape == std::vector<std::size_t> { 3, 3 });
            }
        }
        WHEN("Analyzing a list of floats")
        {
            auto [cdf_type, shape] = run_test("[1.0, 2.0, 3.0, 4.0]");
            THEN("The inferred type is CDF_DOUBLE and shape is [4]")
            {
                REQUIRE(cdf_type == CDF_Types::CDF_DOUBLE);
                REQUIRE(shape == std::vector<std::size_t> { 4 });
            }
        }
        WHEN("Analyzing a mixed list of integers and floats")
        {
            auto [cdf_type, shape] = run_test("[1, 2.0, 3, 4.0]");
            THEN("The inferred type is CDF_DOUBLE and shape is [4]")
            {
                REQUIRE(cdf_type == CDF_Types::CDF_DOUBLE);
                REQUIRE(shape == std::vector<std::size_t> { 4 });
            }
        }
        WHEN("Analyzing a list of datetime objects")
        {
            auto [cdf_type, shape] = run_test("[datetime(2020, 1, 1), datetime(2021, 1, 1)]");
            THEN("The inferred type is CDF_TIME_TT2000 and shape is [2]")
            {
                REQUIRE(cdf_type == CDF_Types::CDF_TIME_TT2000);
                REQUIRE(shape == std::vector<std::size_t> { 2 });
            }
        }
        GIVEN("A list of strings")
        {
            WHEN("All strings have the same length")
            {
                auto [cdf_type, shape] = run_test("['abc', 'def', 'ghi']");
                THEN("The inferred type is CDF_CHAR and shape is [3, 3]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_UCHAR);
                    REQUIRE(shape == std::vector<std::size_t> { 3, 3 });
                }
            }
            WHEN("Strings have varying lengths")
            {
                auto [cdf_type, shape] = run_test("['a', 'de', 'ghi']");
                THEN("The inferred type is CDF_CHAR and shape is [3, 3]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_UCHAR);
                    REQUIRE(shape == std::vector<std::size_t> { 3, 3 });
                }
            }
        }
        GIVEN("A list of strings with unicode characters")
        {
            WHEN("All strings have the same length")
            {
                auto [cdf_type, shape] = run_test("['√abc', '√def', '√ghi']");
                THEN("The inferred type is CDF_CHAR and shape is [3, 6]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_UCHAR);
                    REQUIRE(shape == std::vector<std::size_t> { 3, 6 });
                }
            }
            WHEN("Strings have varying lengths")
            {
                auto [cdf_type, shape] = run_test(
                    "['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥', 'Chinese: 社安', 'Other: ႡႢႣႤႥႦ']");
                THEN("The inferred type is CDF_CHAR and shape is [4, 28]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_UCHAR);
                    REQUIRE(shape == std::vector<std::size_t> { 4, 28 });
                }
            }
            WHEN("Strings have varying lengths (2D)")
            {
                auto [cdf_type, shape] = run_test(
                    "[['ASCII: ABCDEFG', 'Latin1: ©æêü÷Æ¼®¢¥'], ['Chinese: 社安', 'Other: "
                    "ႡႢႣႤႥႦ']]");
                THEN("The inferred type is CDF_CHAR and shape is [2, 2, 28]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_UCHAR);
                    REQUIRE(shape == std::vector<std::size_t> { 2, 2, 28 });
                }
            }
        }
        GIVEN("A list of integers")
        {
            WHEN("All values are within Int8 range")
            {
                auto [cdf_type, shape] = run_test("[-100, 0, 100, 127]");
                THEN("The inferred type is CDF_INT1 and shape is [4]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_INT1);
                    REQUIRE(shape == std::vector<std::size_t> { 4 });
                }
            }
            WHEN("Values exceed Int8 but within Int16 range")
            {
                auto [cdf_type, shape] = run_test("[-200, 0, 200, 30000]");
                THEN("The inferred type is CDF_INT2 and shape is [4]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_INT2);
                    REQUIRE(shape == std::vector<std::size_t> { 4 });
                }
            }
            WHEN("Values exceed Int16 but within Int32 range")
            {
                auto [cdf_type, shape] = run_test("[-70000, 0, 70000, 2000000000]");
                THEN("The inferred type is CDF_INT4 and shape is [4]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_INT4);
                    REQUIRE(shape == std::vector<std::size_t> { 4 });
                }
            }
            WHEN("Values exceed Int32 range")
            {
                auto [cdf_type, shape] = run_test("[-5000000000, 0, 5000000000, 9000000000000]");
                THEN("The inferred type is CDF_INT8 and shape is [4]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_INT8);
                    REQUIRE(shape == std::vector<std::size_t> { 4 });
                }
            }
            WHEN("All values are within UInt8 range")
            {
                auto [cdf_type, shape] = run_test("[0, 100, 200, 255]");
                THEN("The inferred type is CDF_UINT1 and shape is [4]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_UINT1);
                    REQUIRE(shape == std::vector<std::size_t> { 4 });
                }
            }
            WHEN("Values exceed UInt8 but within UInt16 range")
            {
                auto [cdf_type, shape] = run_test("[0, 1000, 20000, 60000]");
                THEN("The inferred type is CDF_UINT2 and shape is [4]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_UINT2);
                    REQUIRE(shape == std::vector<std::size_t> { 4 });
                }
            }
            WHEN("Values exceed UInt16 but within UInt32 range")
            {
                auto [cdf_type, shape] = run_test("[0, 100000, 2000000000, 4000000000]");
                THEN("The inferred type is CDF_UINT4 and shape is [4]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_UINT4);
                    REQUIRE(shape == std::vector<std::size_t> { 4 });
                }
            }
            WHEN("Values exceed UInt32 range")
            {
                auto [cdf_type, shape] = run_test("[0, 10000000000, 20000000000, 0x10000000000]");
                THEN("The inferred type is CDF_INT8 and shape is [4]")
                {
                    REQUIRE(cdf_type == CDF_Types::CDF_INT8);
                    REQUIRE(shape == std::vector<std::size_t> { 4 });
                }
            }
        }
    }
    GIVEN("A bunch of broken collections")
    {
        WHEN("Analyzing a list with unsupported types")
        {
            THEN("An exception is raised")
            {
                REQUIRE_THROWS_WITH(run_test("[None, None, None]"),
                    ContainsSubstring("RuntimeError: Unsupported data type encountered"));
                REQUIRE_THROWS_WITH(run_test("[{}, {}, {}]"),
                    ContainsSubstring("RuntimeError: Unsupported data type encountered"));
                REQUIRE_THROWS_WITH(run_test("[set(), set(), set()]"),
                    ContainsSubstring("RuntimeError: Unsupported data type encountered"));
            }
        }
        WHEN("Analyzing a list with irregular shapes")
        {
            THEN("An exception is raised")
            {
                REQUIRE_THROWS_WITH(run_test("[[1, 2], [3, 4, 5]]"),
                    ContainsSubstring("IndexError: Inconsistent shapes in nested lists/tuples"));
            }
        }
        WHEN("Analyzing a list with mixed incompatible types")
        {
            THEN("An exception is raised")
            {
                REQUIRE_THROWS_WITH(run_test("[[1,2,3], 'string', [4,5,6]]"),
                    ContainsSubstring("IndexError: Incompatible types in nested lists/tuples"));
                REQUIRE_THROWS_WITH(run_test("[ 1, [1,2,3]]"),
                    ContainsSubstring("IndexError: Incompatible types in nested lists/tuples"));
                REQUIRE_THROWS_WITH(run_test("[ 1, datetime(2020,1,1)]"),
                    ContainsSubstring("IndexError: Incompatible types in nested lists/tuples"));
                REQUIRE_THROWS_WITH(run_test("[ datetime(2020,1,1), 1.0]"),
                    ContainsSubstring("IndexError: Incompatible types in nested lists/tuples"));
            }
        }
    }
}
