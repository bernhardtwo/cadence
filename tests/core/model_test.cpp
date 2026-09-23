#include <cadence/core/block.hpp>
#include <cadence/core/clock.hpp>
#include <cadence/core/time.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace cadence::core;
using namespace std::chrono_literals;

TEST_CASE("time of day parses and formats HH:MM", "[time]") {
    CHECK(parseTimeOfDay("08:30") == timeOfDay(8, 30));
    CHECK(parseTimeOfDay("00:00") == Minutes{0});
    CHECK(parseTimeOfDay("23:59") == Minutes{1439});

    CHECK_FALSE(parseTimeOfDay("24:00").has_value());
    CHECK_FALSE(parseTimeOfDay("08:60").has_value());
    CHECK_FALSE(parseTimeOfDay("8:30").has_value());
    CHECK_FALSE(parseTimeOfDay("08-30").has_value());
    CHECK_FALSE(parseTimeOfDay("ab:cd").has_value());
    CHECK_FALSE(parseTimeOfDay("").has_value());

    CHECK(formatTimeOfDay(timeOfDay(8, 30)) == "08:30");
    CHECK(formatTimeOfDay(Minutes{0}) == "00:00");
    CHECK(formatTimeOfDay(Minutes{1439}) == "23:59");

    CHECK(isValidTimeOfDay(Minutes{0}));
    CHECK(isValidTimeOfDay(Minutes{1439}));
    CHECK_FALSE(isValidTimeOfDay(Minutes{1440}));
    CHECK_FALSE(isValidTimeOfDay(Minutes{-1}));
}

TEST_CASE("time points convert to instants and back", "[time]") {
    const TimePoint point{Date{std::chrono::year{2026}, std::chrono::March, std::chrono::day{1}}, timeOfDay(21, 15)};
    const Instant instant = toInstant(point);

    CHECK(toTimePoint(instant) == point);
    CHECK(toTimePoint(instant + 59s) == point);
    CHECK(toTimePoint(instant + 60s).minuteOfDay == timeOfDay(21, 16));
    CHECK(point < TimePoint{point.date, timeOfDay(21, 16)});
}

TEST_CASE("fake clock advances on demand only", "[clock]") {
    const TimePoint start{Date{std::chrono::year{2026}, std::chrono::January, std::chrono::day{5}}, timeOfDay(9, 0)};
    FakeClock clock(start);
    const IClock& iface = clock;

    CHECK(iface.now() == toInstant(start));
    clock.advance(90s);
    CHECK(toTimePoint(iface.now()).minuteOfDay == timeOfDay(9, 1));
    clock.set(TimePoint{start.date, timeOfDay(12, 0)});
    CHECK(toTimePoint(iface.now()).minuteOfDay == timeOfDay(12, 0));
}

TEST_CASE("a weekday without a template is a free day", "[template]") {
    WeekTemplate week;
    DayTemplate monday;
    monday.dayStart = timeOfDay(8, 30);
    week.day(std::chrono::Monday) = monday;

    CHECK_FALSE(week.isFreeDay(std::chrono::Monday));
    CHECK(week.isFreeDay(std::chrono::Saturday));
    CHECK(week.isFreeDay(std::chrono::Sunday));
    CHECK(WeekTemplate::indexOf(std::chrono::Monday) == 0);
    CHECK(WeekTemplate::indexOf(std::chrono::Sunday) == 6);
    CHECK(week.day(std::chrono::Monday)->dayStart == timeOfDay(8, 30));
}

TEST_CASE("block duration resolves from the explicit value or the pomodoro count", "[template]") {
    BlockTemplate block;
    CHECK_FALSE(resolvedDuration(block).has_value());

    block.pomodoro = PomodoroPlan{};
    CHECK_FALSE(resolvedDuration(block).has_value());

    block.pomodoro->count = 3;
    CHECK(resolvedDuration(block) == 85min);

    block.durationMinutes = 100min;
    CHECK(resolvedDuration(block) == 100min);
}
