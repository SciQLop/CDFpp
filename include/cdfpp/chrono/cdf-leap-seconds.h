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
#include <array>
#include <chrono>
#include <utility>
namespace cdf::chrono::leap_seconds
{
using namespace std::chrono;

static inline constexpr uint32_t last_updated = 20170101;

constexpr std::array leap_seconds_tt2000 = {
    // ('1', 'Jan', '1972')
    std::pair { 63072000000000000, 10000000000 },
    // ('1', 'Jul', '1972')
    std::pair { 78796800000000000, 11000000000 },
    // ('1', 'Jan', '1973')
    std::pair { 94694400000000000, 12000000000 },
    // ('1', 'Jan', '1974')
    std::pair { 126230400000000000, 13000000000 },
    // ('1', 'Jan', '1975')
    std::pair { 157766400000000000, 14000000000 },
    // ('1', 'Jan', '1976')
    std::pair { 189302400000000000, 15000000000 },
    // ('1', 'Jan', '1977')
    std::pair { 220924800000000000, 16000000000 },
    // ('1', 'Jan', '1978')
    std::pair { 252460800000000000, 17000000000 },
    // ('1', 'Jan', '1979')
    std::pair { 283996800000000000, 18000000000 },
    // ('1', 'Jan', '1980')
    std::pair { 315532800000000000, 19000000000 },
    // ('1', 'Jul', '1981')
    std::pair { 362793600000000000, 20000000000 },
    // ('1', 'Jul', '1982')
    std::pair { 394329600000000000, 21000000000 },
    // ('1', 'Jul', '1983')
    std::pair { 425865600000000000, 22000000000 },
    // ('1', 'Jul', '1985')
    std::pair { 489024000000000000, 23000000000 },
    // ('1', 'Jan', '1988')
    std::pair { 567993600000000000, 24000000000 },
    // ('1', 'Jan', '1990')
    std::pair { 631152000000000000, 25000000000 },
    // ('1', 'Jan', '1991')
    std::pair { 662688000000000000, 26000000000 },
    // ('1', 'Jul', '1992')
    std::pair { 709948800000000000, 27000000000 },
    // ('1', 'Jul', '1993')
    std::pair { 741484800000000000, 28000000000 },
    // ('1', 'Jul', '1994')
    std::pair { 773020800000000000, 29000000000 },
    // ('1', 'Jan', '1996')
    std::pair { 820454400000000000, 30000000000 },
    // ('1', 'Jul', '1997')
    std::pair { 867715200000000000, 31000000000 },
    // ('1', 'Jan', '1999')
    std::pair { 915148800000000000, 32000000000 },
    // ('1', 'Jan', '2006')
    std::pair { 1136073600000000000, 33000000000 },
    // ('1', 'Jan', '2009')
    std::pair { 1230768000000000000, 34000000000 },
    // ('1', 'Jul', '2012')
    std::pair { 1341100800000000000, 35000000000 },
    // ('1', 'Jul', '2015')
    std::pair { 1435708800000000000, 36000000000 },
    // ('1', 'Jan', '2017')
    std::pair { 1483228800000000000, 37000000000 },
};


constexpr std::array leap_seconds_tt2000_reverse = {
    // ('1', 'Jan', '1972')
    std::pair { -883655957816000000, 10000000000 },
    // ('1', 'Jul', '1972')
    std::pair { -867931156816000000, 11000000000 },
    // ('1', 'Jan', '1973')
    std::pair { -852033555816000000, 12000000000 },
    // ('1', 'Jan', '1974')
    std::pair { -820497554816000000, 13000000000 },
    // ('1', 'Jan', '1975')
    std::pair { -788961553816000000, 14000000000 },
    // ('1', 'Jan', '1976')
    std::pair { -757425552816000000, 15000000000 },
    // ('1', 'Jan', '1977')
    std::pair { -725803151816000000, 16000000000 },
    // ('1', 'Jan', '1978')
    std::pair { -694267150816000000, 17000000000 },
    // ('1', 'Jan', '1979')
    std::pair { -662731149816000000, 18000000000 },
    // ('1', 'Jan', '1980')
    std::pair { -631195148816000000, 19000000000 },
    // ('1', 'Jul', '1981')
    std::pair { -583934347816000000, 20000000000 },
    // ('1', 'Jul', '1982')
    std::pair { -552398346816000000, 21000000000 },
    // ('1', 'Jul', '1983')
    std::pair { -520862345816000000, 22000000000 },
    // ('1', 'Jul', '1985')
    std::pair { -457703944816000000, 23000000000 },
    // ('1', 'Jan', '1988')
    std::pair { -378734343816000000, 24000000000 },
    // ('1', 'Jan', '1990')
    std::pair { -315575942816000000, 25000000000 },
    // ('1', 'Jan', '1991')
    std::pair { -284039941816000000, 26000000000 },
    // ('1', 'Jul', '1992')
    std::pair { -236779140816000000, 27000000000 },
    // ('1', 'Jul', '1993')
    std::pair { -205243139816000000, 28000000000 },
    // ('1', 'Jul', '1994')
    std::pair { -173707138816000000, 29000000000 },
    // ('1', 'Jan', '1996')
    std::pair { -126273537816000000, 30000000000 },
    // ('1', 'Jul', '1997')
    std::pair { -79012736816000000, 31000000000 },
    // ('1', 'Jan', '1999')
    std::pair { -31579135816000000, 32000000000 },
    // ('1', 'Jan', '2006')
    std::pair { 189345665184000000, 33000000000 },
    // ('1', 'Jan', '2009')
    std::pair { 284040066184000000, 34000000000 },
    // ('1', 'Jul', '2012')
    std::pair { 394372867184000000, 35000000000 },
    // ('1', 'Jul', '2015')
    std::pair { 488980868184000000, 36000000000 },
    // ('1', 'Jan', '2017')
    std::pair { 536500869184000000, 37000000000 },
};

// Before 1972, TAI-UTC drifted: base + (MJD - mjd_ref) * rate seconds, over periods starting at
// the given UTC dates (NASA's CDF library, cdftt2000.c LTS table). TAI-UTC is 0 before 1960.
struct drift_period
{
    int64_t start_ns_from_1970;
    double base;
    double mjd_ref;
    double rate;
};

constexpr std::array drift_periods_before_1972 = {
    // 1960-01-01
    drift_period { -315619200000000000, 1.4178180, 37300.0, 0.0012960 },
    // 1961-01-01
    drift_period { -283996800000000000, 1.4228180, 37300.0, 0.0012960 },
    // 1961-08-01
    drift_period { -265680000000000000, 1.3728180, 37300.0, 0.0012960 },
    // 1962-01-01
    drift_period { -252460800000000000, 1.8458580, 37665.0, 0.0011232 },
    // 1963-11-01
    drift_period { -194659200000000000, 1.9458580, 37665.0, 0.0011232 },
    // 1964-01-01
    drift_period { -189388800000000000, 3.2401300, 38761.0, 0.0012960 },
    // 1964-04-01
    drift_period { -181526400000000000, 3.3401300, 38761.0, 0.0012960 },
    // 1964-09-01
    drift_period { -168307200000000000, 3.4401300, 38761.0, 0.0012960 },
    // 1965-01-01
    drift_period { -157766400000000000, 3.5401300, 38761.0, 0.0012960 },
    // 1965-03-01
    drift_period { -152668800000000000, 3.6401300, 38761.0, 0.0012960 },
    // 1965-07-01
    drift_period { -142128000000000000, 3.7401300, 38761.0, 0.0012960 },
    // 1965-09-01
    drift_period { -136771200000000000, 3.8401300, 38761.0, 0.0012960 },
    // 1966-01-01
    drift_period { -126230400000000000, 4.3131700, 39126.0, 0.0025920 },
    // 1968-02-01
    drift_period { -60480000000000000, 4.2131700, 39126.0, 0.0025920 },
};

} // namespace cdf
