#include "all_headers.hpp"
#include "tests_config.hpp"

#include <catch2/catch_all.hpp>

SCENARIO("CDFpp headers can be included from several source files", "[CDF]")
{
    THEN("the program links, and both files see the same CDF")
    {
        const auto cdf = cdf::io::load(std::string(DATA_PATH) + "/a_cdf.cdf");
        REQUIRE(cdf != std::nullopt);
        REQUIRE(variables_count(*cdf) == std::size(cdf->variables));
    }
}
