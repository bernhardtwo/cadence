#include <cadence/core/planner.hpp>
#include <cadence/core/summary.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace cadence::core;
using namespace std::chrono_literals;

namespace {

constexpr Date anyDate{std::chrono::year{2026}, std::chrono::September, std::chrono::day{23}};

TimePoint at(int hours, int minutes) {
    return TimePoint{anyDate, timeOfDay(hours, minutes)};
}

BlockTemplate block(const char* id, BlockKind kind, std::optional<Minutes> start, Minutes duration) {
    BlockTemplate result;
    result.activityId = id;
    result.kind = kind;
    result.start = start;
    result.durationMinutes = duration;
    return result;
}

// The shipped weekday: work 08:30 for 390, lunch 60, french 55, logic 25, reading soft at 21:00.
DayTemplate weekday() {
    DayTemplate day;
    day.dayStart = timeOfDay(8, 30);
    day.blocks = {
        block("work", BlockKind::Anchored, timeOfDay(8, 30), 390min),
        block("lunch", BlockKind::Flexible, std::nullopt, 60min),
        block("french", BlockKind::Flexible, std::nullopt, 55min),
        block("logic", BlockKind::Flexible, std::nullopt, 25min),
        block("reading", BlockKind::Soft, timeOfDay(21, 0), 30min),
    };
    return day;
}

const std::vector<Activity> activities = {
    {"work", "Work", "#D6FF3F"},   {"lunch", "Lunch", "#FF6B3D"},     {"french", "French", "#7FB8FF"},
    {"logic", "Logic", "#C9A0FF"}, {"reading", "Reading", "#F2F0EA"},
};

} // namespace

TEST_CASE("the summary follows the activity called work", "[summary]") {
    CHECK(summaryActivity(weekday(), activities) == ActivityId{"work"});

    DayTemplate day = weekday();
    day.blocks[0].activityId = "deep";
    std::vector<Activity> renamed = activities;
    renamed[0] = Activity{"deep", "Deep focus", "#D6FF3F"};
    // No activity named work: the one with the most template time wins.
    CHECK(summaryActivity(day, renamed) == ActivityId{"deep"});

    CHECK_FALSE(summaryActivity(DayTemplate{}, activities).has_value());
}

TEST_CASE("the target is the template length whatever the actual end", "[summary]") {
    const DayTemplate day = weekday();
    DayProgress progress;
    progress.actualDayStart = timeOfDay(8, 35);
    progress.at(0).actualStart = timeOfDay(8, 35);
    progress.at(0).actualEnd = timeOfDay(14, 45);
    progress.at(1).skipped = true;
    progress.at(2).skipped = true;
    progress.at(3).actualStart = timeOfDay(16, 52);

    const TimePoint now = at(16, 53);
    const DayPlan plan = cadence::core::plan(day, progress, now);
    const ActivitySummary summary = summarizeActivity(day, plan, progress, "work", now.minuteOfDay);

    // The block ran 08:30 to 14:45 in the plan, fifteen minutes short of its 390.
    CHECK(summary.done == 375min);
    CHECK(summary.target == 390min);
}

TEST_CASE("a running block counts its elapsed time without pauses", "[summary]") {
    const DayTemplate day = weekday();
    DayProgress progress;
    progress.at(0).actualStart = timeOfDay(8, 30);
    progress.at(0).pauses.push_back(PauseInterval{timeOfDay(9, 0), timeOfDay(9, 10)});

    const TimePoint now = at(10, 0);
    const DayPlan plan = cadence::core::plan(day, progress, now);
    const ActivitySummary summary = summarizeActivity(day, plan, progress, "work", now.minuteOfDay);

    CHECK(summary.done == 80min);
    CHECK(summary.target == 390min);
}

TEST_CASE("untouched and skipped blocks count nothing but keep the target", "[summary]") {
    DayTemplate day = weekday();
    day.blocks.push_back(block("work", BlockKind::Flexible, std::nullopt, 60min));
    DayProgress progress;
    progress.at(5).skipped = true;

    const TimePoint now = at(8, 0);
    const DayPlan plan = cadence::core::plan(day, progress, now);
    const ActivitySummary summary = summarizeActivity(day, plan, progress, "work", now.minuteOfDay);

    CHECK(summary.done == 0min);
    CHECK(summary.target == 450min);
}
