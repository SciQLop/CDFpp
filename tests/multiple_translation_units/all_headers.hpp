#pragma once
#include "cdfpp/attribute.hpp"
#include "cdfpp/cdf-data.hpp"
#include "cdfpp/cdf-debug.hpp"
#include "cdfpp/cdf-enums.hpp"
#include "cdfpp/cdf-file.hpp"
#include "cdfpp/cdf-helpers.hpp"
#include "cdfpp/cdf-io/cdf-io.hpp"
#include "cdfpp/cdf-io/debug/nasa_compat_repr.hpp"
#include "cdfpp/cdf-io/debug/record_repr.hpp"
#include "cdfpp/cdf-io/debug/record_stream.hpp"
#include "cdfpp/cdf-map.hpp"
#include "cdfpp/cdf-repr.hpp"
#include "cdfpp/cdf.hpp"
#include "cdfpp/chrono/cdf-chrono.hpp"
#include "cdfpp/variable.hpp"

std::size_t variables_count(const cdf::CDF& cdf);
