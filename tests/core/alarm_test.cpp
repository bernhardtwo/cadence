#include <cadence/core/alarm.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace cadence::core;
using namespace std::chrono_literals;

namespace {

constexpr Date anyDate{std::chrono::year{2026}, std::chrono::September, std::chrono::day{22}};

Instant at(int hours, int minutes, int seconds = 0) {
    return toInstant(TimePoint{anyDate, timeOfDay(hours, minutes)}) + Seconds{seconds};
}

BlockTemplate anchored(const char* id, Minutes start, Minutes duration) {
    BlockTemplate block;
    block.activityId = id;
    block.kind = BlockKind::Anchored;
    block.start = start;
    block.durationMinutes = duration;
    return block;
}

BlockTemplate flexible(const char* id, Minutes duration) {
    BlockTemplate block;
    block.activityId = id;
    block.kind = BlockKind::Flexible;
    block.durationMinutes = duration;
    return block;
}

BlockTemplate soft(const char* id, Minutes earliest, Minutes duration) {
    BlockTemplate block;
    block.activityId = id;
    block.kind = BlockKind::Soft;
    block.start = earliest;
    block.durationMinutes = duration;
    return block;
}

// work 08:30 to 15:00, lunch 60, french 55, reading soft from 21:00.
DayTemplate weekday() {
    DayTemplate day;
    day.dayStart = timeOfDay(8, 30);
    day.dayCutoff = timeOfDay(23, 0);
    day.blocks = {
        anchored("work", timeOfDay(8, 30), 390min),
        flexible("lunch", 60min),
        flexible("french", 55min),
        soft("reading", timeOfDay(21, 0), 30min),
    };
    return day;
}

struct Harness {
    DayTemplate day = weekday();
    DayProgress progress;
    AlarmScheduler scheduler;

    std::vector<Alarm> eval(Instant now, const PomodoroEvents& events = {}) {
        const DayPlan planned = plan(day, progress, toTimePoint(now));
        return scheduler.evaluate(day, planned, progress, events, now);
    }
};

std::vector<AlarmKind> kindsOf(const std::vector<Alarm>& alarms) {
    std::vector<AlarmKind> kinds;
    for (const Alarm& alarm : alarms) {
        kinds.push_back(alarm.kind);
    }
    return kinds;
}

} // namespace

TEST_CASE("the first evaluation fires nothing from the past", "[alarm]") {
    Harness h;
    CHECK(h.eval(at(10, 0)).empty());
    CHECK(h.eval(at(10, 0, 1)).empty());
}

TEST_CASE("BlockStart fires once when an anchored block starts", "[alarm]") {
    Harness h;
    CHECK(h.eval(at(8, 29)).empty());
    CHECK(h.eval(at(8, 29, 59)).empty());

    const std::vector<Alarm> due = h.eval(at(8, 30));
    REQUIRE(due.size() == 1);
    CHECK(due[0] == Alarm{AlarmKind::BlockStart, 0, at(8, 30)});

    CHECK(h.eval(at(8, 30, 1)).empty());
    CHECK(h.eval(at(8, 31)).empty());
}

TEST_CASE("a block the user already started gets no start alarm", "[alarm]") {
    Harness h;
    h.progress.at(0).actualStart = timeOfDay(8, 25);
    CHECK(h.eval(at(8, 29, 59)).empty());
    CHECK(h.eval(at(8, 30)).empty());
}

TEST_CASE("BlockStart fires for a flexible block when the chain reaches it", "[alarm]") {
    Harness h;
    h.progress.at(0).actualStart = timeOfDay(8, 30);
    h.progress.at(0).actualEnd = timeOfDay(14, 40);

    CHECK(h.eval(at(14, 39, 59)).empty());
    const std::vector<Alarm> due = h.eval(at(14, 40));
    REQUIRE(due.size() == 1);
    CHECK(due[0] == Alarm{AlarmKind::BlockStart, 1, at(14, 40)});

    // The unstarted block keeps moving with the clock; that never re-fires it.
    CHECK(h.eval(at(14, 40, 1)).empty());
    CHECK(h.eval(at(14, 45)).empty());
}

TEST_CASE("BlockEndingSoon fires once five minutes before an active block ends", "[alarm]") {
    Harness h;
    CHECK(h.eval(at(14, 54, 59)).empty());

    const std::vector<Alarm> due = h.eval(at(14, 55));
    REQUIRE(due.size() == 1);
    CHECK(due[0] == Alarm{AlarmKind::BlockEndingSoon, 0, at(14, 55)});

    CHECK(h.eval(at(14, 55, 1)).empty());
    CHECK(h.eval(at(14, 59)).empty());
}

TEST_CASE("a soft block gets a single reminder that cannot be snoozed", "[alarm]") {
    Harness h;
    h.progress.at(0).confirmed = true;
    h.progress.at(1).skipped = true;
    h.progress.at(2).skipped = true;

    CHECK(h.eval(at(20, 59, 59)).empty());
    const std::vector<Alarm> due = h.eval(at(21, 0));
    REQUIRE(due.size() == 1);
    CHECK(due[0] == Alarm{AlarmKind::SoftReminder, 3, at(21, 0)});

    CHECK(h.eval(at(21, 0, 1)).empty());
    CHECK(h.eval(at(21, 5)).empty());
    CHECK_FALSE(h.scheduler.canSnooze(3));
    CHECK_FALSE(h.scheduler.snooze(3, at(21, 0, 5)));
}

TEST_CASE("pomodoro phase changes become focus end and break end alarms", "[alarm]") {
    Harness h;
    h.eval(at(9, 0));

    CHECK(kindsOf(h.eval(at(9, 0, 1), {PhaseStarted{PomodoroState::ShortBreak, 0}})) ==
          std::vector<AlarmKind>{AlarmKind::PomodoroFocusEnd});
    CHECK(kindsOf(h.eval(at(9, 0, 2), {PhaseStarted{PomodoroState::LongBreak, 3}})) ==
          std::vector<AlarmKind>{AlarmKind::PomodoroFocusEnd});
    CHECK(kindsOf(h.eval(at(9, 0, 3), {PhaseStarted{PomodoroState::Focus, 1}})) ==
          std::vector<AlarmKind>{AlarmKind::BreakEnd});
    CHECK(h.eval(at(9, 0, 4), {PhaseStarted{PomodoroState::Focus, 0}}).empty());
    CHECK(kindsOf(h.eval(at(9, 0, 5), {SessionCompleted{}})) == std::vector<AlarmKind>{AlarmKind::PomodoroFocusEnd});
    CHECK(h.eval(at(9, 0, 6), {PushupPrompt{0}}).empty());
}

TEST_CASE("alarms due in the same second both fire during normal ticking", "[alarm]") {
    Harness h;
    h.eval(at(8, 29, 59));
    const std::vector<Alarm> due = h.eval(at(8, 30), {PhaseStarted{PomodoroState::ShortBreak, 0}});
    CHECK(kindsOf(due) == std::vector<AlarmKind>{AlarmKind::BlockStart, AlarmKind::PomodoroFocusEnd});
}

TEST_CASE("waking from sleep collapses missed alarms into the most recent one", "[alarm]") {
    Harness h;
    h.day.blocks = {
        anchored("a", timeOfDay(9, 0), 30min),
        anchored("b", timeOfDay(10, 0), 30min),
        anchored("c", timeOfDay(11, 0), 30min),
    };
    h.eval(at(8, 59));

    const PomodoroEvents replayed = {PhaseStarted{PomodoroState::ShortBreak, 0}, PhaseStarted{PomodoroState::Focus, 1}};
    const std::vector<Alarm> due = h.eval(at(11, 10), replayed);

    // BlockStart for c and two pomodoro alarms were due; only the last survives, plus the questions
    // about the two blocks that elapsed meanwhile.
    CHECK(kindsOf(due) == std::vector<AlarmKind>{AlarmKind::BreakEnd, AlarmKind::UnconfirmedPending,
                                                 AlarmKind::UnconfirmedPending});
    CHECK(due[1].blockIndex == 0);
    CHECK(due[2].blockIndex == 1);

    // The suppressed start alarm counts as fired.
    CHECK(h.eval(at(11, 10, 1)).empty());
    CHECK(h.eval(at(11, 11)).empty());
}

TEST_CASE("snooze re-fires BlockStart up to the limit", "[alarm]") {
    Harness h;
    h.eval(at(8, 29, 59));
    REQUIRE(h.eval(at(8, 30)).size() == 1);

    CHECK(h.scheduler.canSnooze(0));
    CHECK(h.scheduler.snooze(0, at(8, 30, 10)));
    CHECK(h.scheduler.snoozesUsed(0) == 1);
    CHECK(h.eval(at(8, 35, 9)).empty());
    std::vector<Alarm> due = h.eval(at(8, 35, 10));
    REQUIRE(due.size() == 1);
    CHECK(due[0] == Alarm{AlarmKind::BlockStart, 0, at(8, 35, 10)});
    CHECK(h.eval(at(8, 35, 11)).empty());

    CHECK(h.scheduler.snooze(0, at(8, 35, 20)));
    due = h.eval(at(8, 40, 20));
    REQUIRE(due.size() == 1);
    CHECK(due[0].kind == AlarmKind::BlockStart);

    CHECK_FALSE(h.scheduler.canSnooze(0));
    CHECK_FALSE(h.scheduler.snooze(0, at(8, 40, 30)));
    CHECK(h.scheduler.snoozesUsed(0) == 2);
    CHECK(h.eval(at(8, 45, 30)).empty());
    CHECK(h.eval(at(9, 0)).empty());
}

TEST_CASE("starting or dismissing a block cancels its pending snooze", "[alarm]") {
    Harness h;
    h.eval(at(8, 29, 59));
    h.eval(at(8, 30));
    REQUIRE(h.scheduler.snooze(0, at(8, 30, 10)));

    SECTION("started") {
        h.progress.at(0).actualStart = timeOfDay(8, 32);
        CHECK(h.eval(at(8, 32)).empty());
        CHECK(h.eval(at(8, 35, 10)).empty());
    }

    SECTION("dismissed") {
        h.scheduler.dismiss(0);
        CHECK(h.eval(at(8, 35, 10)).empty());
    }
}

TEST_CASE("DayNoLongerFits fires once and re-arms when the day fits again", "[alarm]") {
    Harness h;
    h.day.blocks[1].durationMinutes = 600min;

    std::vector<Alarm> due = h.eval(at(8, 30));
    REQUIRE(due.size() == 1);
    CHECK(due[0].kind == AlarmKind::DayNoLongerFits);
    CHECK(h.eval(at(8, 30, 1)).empty());
    CHECK(h.eval(at(9, 0)).empty());

    h.progress.at(1).skipped = true;
    CHECK(h.eval(at(9, 0, 1)).empty());

    h.progress.at(1).skipped = false;
    due = h.eval(at(9, 0, 2));
    REQUIRE(due.size() == 1);
    CHECK(due[0].kind == AlarmKind::DayNoLongerFits);
}

TEST_CASE("UnconfirmedPending fires once per block", "[alarm]") {
    Harness h;
    h.eval(at(14, 59, 59));

    // Lunch reaches its start at the same moment, which is a separate alarm.
    const std::vector<Alarm> due = h.eval(at(15, 0));
    REQUIRE(due.size() == 2);
    CHECK(due[0] == Alarm{AlarmKind::BlockStart, 1, at(15, 0)});
    CHECK(due[1] == Alarm{AlarmKind::UnconfirmedPending, 0, at(15, 0)});

    CHECK(h.eval(at(15, 0, 1)).empty());
    h.progress.at(0).confirmed = true;
    CHECK(h.eval(at(15, 0, 2)).empty());
}

TEST_CASE("reset forgets fired alarms for a new day", "[alarm]") {
    Harness h;
    h.eval(at(8, 29, 59));
    REQUIRE(h.eval(at(8, 30)).size() == 1);

    h.scheduler.reset();
    CHECK(h.eval(at(8, 29, 59)).empty());
    CHECK(h.eval(at(8, 30)).size() == 1);
}

TEST_CASE("a new policy applies to later snoozes and leads", "[alarm]") {
    Harness h;
    h.eval(at(8, 29, 59));
    REQUIRE(h.eval(at(8, 30)).size() == 1);
    REQUIRE(h.scheduler.snooze(0, at(8, 30, 10)));

    AlarmPolicy policy = h.scheduler.policy();
    policy.maxSnoozes = 1;
    policy.snoozeLength = 2min;
    h.scheduler.setPolicy(policy);

    CHECK_FALSE(h.scheduler.canSnooze(0));
    std::vector<Alarm> due = h.eval(at(8, 35, 10));
    REQUIRE(due.size() == 1);
    CHECK(due[0].kind == AlarmKind::BlockStart);

    policy.maxSnoozes = 3;
    h.scheduler.setPolicy(policy);
    CHECK(h.scheduler.canSnooze(0));
    REQUIRE(h.scheduler.snooze(0, at(8, 36)));
    CHECK(h.eval(at(8, 37, 59)).empty());
    due = h.eval(at(8, 38));
    REQUIRE(due.size() == 1);
    CHECK(due[0] == Alarm{AlarmKind::BlockStart, 0, at(8, 38)});
}
