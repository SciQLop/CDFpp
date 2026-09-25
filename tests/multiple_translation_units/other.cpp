// Includes every public header a second time, in another translation unit: if any function
// defined in a header isn't inline, linking this test fails with "multiple definition".
#include "all_headers.hpp"

std::size_t variables_count(const cdf::CDF& cdf)
{
    return std::size(cdf.variables);
}
