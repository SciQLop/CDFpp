#include <chrono>
#include <ctime>

#define CATCH_CONFIG_RUNNER
#if __has_include(<catch2/catch.hpp>)
#include <catch2/catch.hpp>
#else
#include <catch.hpp>
#endif

#include <pybind11/embed.h>
namespace py = pybind11;

#include "attribute.hpp"
#include "cdf-chrono.hpp"
#include "cdf-file.hpp"
#include "cdf-io/cdf-io.hpp"
#include "date/date.h"

constexpr auto py_functions = R"(
from astropy.time import Time
def seconds_since_0_AD_no_leap(date_str):
    date = Time(date_str)
    if date_str[0:4]=="0000":
      return (date - Time('0000-01-01 00:00:00') ).sec
    else:
      dt = (date - Time('0000-01-01 00:00:00') ).datetime
      return dt.days * 86400 + date.datetime.hour * 3600 + date.datetime.minute * 60 + date.datetime.second

def seconds_since_J2000(date_str):
   date = Time(date_str)
   return int((date - Time('2000-01-01 12:00:00') ).sec)
)";

using namespace date;
using namespace std::chrono;

constexpr auto dates = { sys_days { 0000_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 500_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1500_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1800_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1900_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1960_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1961_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1962_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1968_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1970_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1996_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 1996_y / October / 23 } + 8h + 19min + 3s,
    sys_days { 2000_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 2010_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 2020_y / January / 1 } + 0h + 0min + 0s,
    sys_days { 2020_y / July / 25 } + 2h + 35min + 8s };

TEST_CASE("")
{
    std::for_each(std::cbegin(dates), std::cend(dates), [&](const auto& date) {
        std::cout << date << std::endl;
    });
}

TEST_CASE("From std::time_point to cdf epoch", "")
{
    auto seconds_since_0_AD_no_leap = py::globals()["seconds_since_0_AD_no_leap"];
    std::for_each(std::cbegin(dates), std::cend(dates), [&](const auto& date) {
        auto ms = seconds_since_0_AD_no_leap(format("%FT%T", floor<seconds>(date)))
                      .template cast<double>()
            * 1000.;
        REQUIRE(cdf::to_epoch(date).value == ms);
    });
}

TEST_CASE("From std::time_point to cdf epoch16", "")
{
    auto seconds_since_0_AD_no_leap = py::globals()["seconds_since_0_AD_no_leap"];
    std::for_each(std::cbegin(dates), std::cend(dates), [&](const auto& date) {
        auto s = seconds_since_0_AD_no_leap(format("%FT%T", floor<seconds>(date)))
                     .template cast<double>();
        REQUIRE(cdf::to_epoch16(date).seconds == s);
    });
}

TEST_CASE("From std::time_point to cdf tt2000", "")
{
    auto seconds_since_J2000 = py::globals()["seconds_since_J2000"];
    std::for_each(std::cbegin(dates), std::cend(dates), [&](const auto& date) {
        auto ns
            = seconds_since_J2000(format("%FT%T", floor<seconds>(date))).template cast<int64_t>()
            * 1000 * 1000 * 1000;
        if (date > sys_days { 1972_y / January / 1 } + 0h + 0min + 0s)
            REQUIRE(cdf::to_tt2000(date).value == ns);
    });
}

TEST_CASE("From cdf epoch to std::time_point", "")
{
    auto seconds_since_0_AD_no_leap = py::globals()["seconds_since_0_AD_no_leap"];
    std::for_each(std::cbegin(dates), std::cend(dates), [&](const auto& date) {
        auto ms = seconds_since_0_AD_no_leap(format("%FT%T", floor<seconds>(date)))
                      .template cast<double>()
            * 1000.;
        auto res = cdf::to_time_point(cdf::epoch{ms}) ;
        std::cout << date << std::endl;
        std::cout << res << std::endl;
        REQUIRE(res == date);
    });
}

TEST_CASE("From cdf epoch16 to std::time_point", "")
{
    auto seconds_since_0_AD_no_leap = py::globals()["seconds_since_0_AD_no_leap"];
    std::for_each(std::cbegin(dates), std::cend(dates), [&](const auto& date) {
        auto s = seconds_since_0_AD_no_leap(format("%FT%T", floor<seconds>(date)))
                     .template cast<double>();
        REQUIRE(cdf::to_time_point(cdf::epoch16{s,0.})==date);
    });
}

TEST_CASE("From cdf tt2000 to std::time_point", "")
{
    auto seconds_since_J2000 = py::globals()["seconds_since_J2000"];
    std::for_each(std::cbegin(dates), std::cend(dates), [&](const auto& date) {
        auto ns
            = seconds_since_J2000(format("%FT%T", floor<seconds>(date))).template cast<int64_t>()
            * 1000 * 1000 * 1000;
        if (date > sys_days { 1972_y / January / 1 } + 0h + 0min + 0s)
            REQUIRE(cdf::to_time_point(cdf::tt2000_t { ns }) == date);
    });
}


int main(int argc, char* argv[])
{
    py::scoped_interpreter guard {};
    py::exec(py_functions);
    int result = Catch::Session().run(argc, argv);
    return result;
}
