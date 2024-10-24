/*------------------------------------------------------------------------------
-- The MIT License (MIT)
--
-- Copyright © 2024, Laboratory of Plasma Physics- CNRS
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the “Software”), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is furnished to do
-- so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
-- INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
-- PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
-- HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
-- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
-- SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#pragma once
#ifdef CDFPP_ENABLE_ASSERT
#include <cassert>
#define CDFPP_ASSERT(x) assert(x)
#else
#define CDFPP_ASSERT(x)
#endif
#ifdef CDFPP_HEDLEY
#include <hedley.h>
#define CDFPP_NON_NULL(...) HEDLEY_NON_NULL(__VA_ARGS__)
#define CDF_WARN_UNUSED_RESULT HEDLEY_WARN_UNUSED_RESULT
#define CDFPP_DIAGNOSTIC_PUSH HEDLEY_DIAGNOSTIC_PUSH
#define CDFPP_DIAGNOSTIC_DISABLE_DEPRECATED HEDLEY_DIAGNOSTIC_DISABLE_DEPRECATED
#define CDFPP_DIAGNOSTIC_POP HEDLEY_DIAGNOSTIC_POP
#else
#define CDFPP_NON_NULL(...)
#define CDF_WARN_UNUSED_RESULT
#define CDFPP_DIAGNOSTIC_PUSH
#define CDFPP_DIAGNOSTIC_DISABLE_DEPRECATED
#define CDFPP_DIAGNOSTIC_POP
#endif
