#include <cadence/core/alarm.hpp>
#include <cadence/core/clock.hpp>
#include <cadence/core/planner.hpp>
#include <cadence/core/pomodoro.hpp>
#include <cadence/core/progress_json.hpp>
#include <cadence/core/restore.hpp>
#include <cadence/core/summary.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace cadence::core;
using namespace std::chrono_literals;

namespace {

constexpr Date anyDate{std::chrono::year{2026}, std::chrono::September, std::chrono::day{24}};

TimePoint at(int hours, int minutes) {
    return TimePoint{anyDate, timeOfDay(hours, minutes)};
}

Seconds secondOfDay(const FakeClock& clock) {
    return std::chrono::duration_cast<Seconds>(clock.now() - std::chrono::local_days{anyDate});
}

Minutes minuteOfDay(const FakeClock& clock) {
    return toTimePoint(clock.now()).minuteOfDay;
}

BlockTemplate anchoredPomodoros(const char* id, Minutes start, Minutes duration) {
    BlockTemplate block;
    block.activityId = id;
    block.kind = BlockKind::Anchored;
    block.start = start;
    block.durationMinutes = duration;
    block.pomodoro = PomodoroPlan{};
    return block;
}

BlockTemplate flexible(const char* id, Minutes duration) {
    BlockTemplate block;
    block.activityId = id;
    block.kind = BlockKind::Flexible;
    block.durationMinutes = duration;
    return block;
}

BlockTemplate flexiblePomodoros(const char* id, int count) {
    BlockTemplate block;
    block.activityId = id;
    block.kind = BlockKind::Flexible;
    block.pomodoro = PomodoroPlan{};
    block.pomodoro->count = count;
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

// The shipped weekday: work 08:30 to 15:00 with 25/5/15 pomodoros, lunch, french, logic, reading.
DayTemplate weekday() {
    DayTemplate day;
    day.dayStart = timeOfDay(8, 30);
    day.dayCutoff = timeOfDay(23, 0);
    day.blocks = {
        anchoredPomodoros("work", timeOfDay(8, 30), 390min),
        flexible("lunch", 60min),
        flexiblePomodoros("french", 2),
        flexiblePomodoros("logic", 1),
        soft("reading", timeOfDay(21, 0), 30min),
    };
    return day;
}

// Work started on time with its first two pomodoros recorded, the third focus still running.
DayProgress workRunning() {
    DayProgress progress;
    BlockProgress& work = progress.at(0);
    work.actualStart = timeOfDay(8, 30);
    work.phases = {
        PhaseRecord{PhaseKind::Focus, 0, 8h + 30min, 8h + 55min},
        PhaseRecord{PhaseKind::ShortBreak, 0, 8h + 55min, 9h},
        PhaseRecord{PhaseKind::Focus, 1, 9h, 9h + 25min},
        PhaseRecord{PhaseKind::ShortBreak, 1, 9h + 25min, 9h + 30min},
        PhaseRecord{PhaseKind::Focus, 2, 9h + 30min, std::nullopt},
    };
    return progress;
}

} // namespace

TEST_CASE("skipping a running block closes its records and stamps the skip", "[restore]") {
    FakeClock clock(at(12, 5));
    DayProgress progress = workRunning();
    progress.at(0).pauses.push_back(PauseInterval{timeOfDay(12, 0), std::nullopt});

    skipBlock(progress.at(0), minuteOfDay(clock), secondOfDay(clock));

    const BlockProgress& work = progress.at(0);
    CHECK(work.skipped);
    REQUIRE(work.skips.size() == 1);
    CHECK(work.skips.back().at == timeOfDay(12, 5));
    CHECK_FALSE(work.skips.back().restoredAt);
    CHECK(work.pauses.back().end == timeOfDay(12, 5));
    REQUIRE(work.phases.size() == 5);
    CHECK(work.phases.back().end == Seconds{12h + 5min});
    CHECK(plan(weekday(), progress, at(12, 5)).at(0).state == BlockState::Skipped);
}

TEST_CASE("an anchored block restored mid-window continues at the clock with the time left", "[restore]") {
    FakeClock clock(at(12, 5));
    const DayTemplate day = weekday();
    DayProgress progress = workRunning();
    skipBlock(progress.at(0), minuteOfDay(clock), secondOfDay(clock));
    clock.advance(5min);

    REQUIRE_FALSE(restoreRefusal(day, progress, 0, at(12, 10)));
    const auto result = restoreBlock(day, progress, 0, clock.now());
    REQUIRE(result);

    const BlockProgress& work = progress.at(0);
    CHECK_FALSE(work.skipped);
    REQUIRE(work.skips.size() == 1);
    CHECK(work.skips.back().restoredAt == timeOfDay(12, 10));
    CHECK(work.actualStart == timeOfDay(8, 30));
    // Phases from before the skip stay so the actual time of the block is still on record.
    CHECK(work.phases.size() == 5);
    CHECK(work.phases.back().end == Seconds{12h + 5min});

    const DayPlan planned = plan(day, progress, at(12, 10));
    CHECK(planned.at(0).state == BlockState::Active);
    CHECK(planned.at(0).start == timeOfDay(8, 30));
    CHECK(planned.at(0).end == timeOfDay(15, 0));
    CHECK(planned.at(1).start == timeOfDay(15, 0));

    // 170 minutes are left: five pomodoros with their four breaks take 155, six would take 185.
    CHECK(result->pomodoroCount == 5);
    CHECK(work.pomodoroCount == 5);
    CHECK_FALSE(result->displaced);

    SECTION("the summary counts the time before the skip and nothing of the gap") {
        CHECK(workedMinutes(planned.at(0), &work, timeOfDay(12, 10)) == 215min);
        CHECK(workedMinutes(planned.at(0), &work, timeOfDay(13, 0)) == 265min);
    }

    SECTION("a restart rebuilds the recalculated session, not the template's") {
        BlockProgress later = work;
        later.phases.push_back(PhaseRecord{PhaseKind::Focus, 0, 12h + 10min, std::nullopt});
        const auto session =
            PomodoroSession::restore(day.blocks[0], later.phases, std::chrono::local_days{anyDate}, later.pomodoroCount);
        REQUIRE(session);
        CHECK(session->totalCount() == 5);
        CHECK(session->currentIndex() == 0);
        CHECK(session->remaining(toInstant(at(12, 20))) == 15min);
    }

    SECTION("once the block is done its skipped minutes are not completed minutes") {
        progress.at(0).actualEnd = timeOfDay(15, 0);
        const DayPlan done = plan(day, progress, at(15, 0));
        CHECK(done.at(0).state == BlockState::Done);
        CHECK(done.completedMinutes(progress) == 385min);
        CHECK(workedMinutes(done.at(0), &progress.at(0), timeOfDay(15, 0)) == 385min);
    }
}

TEST_CASE("an anchored block that was never started is restored as running from now", "[restore]") {
    const DayTemplate day = weekday();
    DayProgress progress;
    skipBlock(progress.at(0), timeOfDay(9, 0), 9h);

    const auto result = restoreBlock(day, progress, 0, toInstant(at(9, 10)));

    REQUIRE(result);
    CHECK(progress.at(0).actualStart == timeOfDay(9, 10));
    // 350 minutes left: eleven pomodoros take 350 exactly with two long breaks.
    CHECK(result->pomodoroCount == 11);
    CHECK(plan(day, progress, at(9, 10)).at(0).state == BlockState::Active);
}

TEST_CASE("a restore is refused once the window or the day is over", "[restore]") {
    const DayTemplate day = weekday();

    SECTION("an anchored block past its end") {
        DayProgress progress = workRunning();
        skipBlock(progress.at(0), timeOfDay(14, 50), 14h + 50min);
        CHECK(restoreRefusal(day, progress, 0, at(14, 59)) == std::nullopt);
        CHECK(restoreRefusal(day, progress, 0, at(15, 0)) == RestoreRefusal::WindowClosed);
        const DayProgress before = progress;
        CHECK_FALSE(restoreBlock(day, progress, 0, toInstant(at(15, 30))));
        CHECK(progress == before);
    }

    SECTION("a finished pause stretches the window like it stretches the block") {
        DayProgress progress = workRunning();
        progress.at(0).pauses.push_back(PauseInterval{timeOfDay(10, 0), timeOfDay(10, 20)});
        skipBlock(progress.at(0), timeOfDay(14, 50), 14h + 50min);
        CHECK(restoreRefusal(day, progress, 0, at(15, 10)) == std::nullopt);
        CHECK(restoreRefusal(day, progress, 0, at(15, 20)) == RestoreRefusal::WindowClosed);
    }

    SECTION("any block after the cutoff") {
        DayProgress progress;
        skipBlock(progress.at(1), timeOfDay(12, 0), 12h);
        skipBlock(progress.at(4), timeOfDay(12, 0), 12h);
        CHECK(restoreRefusal(day, progress, 1, at(22, 59)) == std::nullopt);
        CHECK(restoreRefusal(day, progress, 1, at(23, 0)) == RestoreRefusal::WindowClosed);
        CHECK(restoreRefusal(day, progress, 4, at(22, 59)) == std::nullopt);
        CHECK(restoreRefusal(day, progress, 4, at(23, 0)) == RestoreRefusal::WindowClosed);
    }

    SECTION("a block that is not skipped, or was denied after its time") {
        DayProgress progress;
        CHECK(restoreRefusal(day, progress, 1, at(12, 0)) == RestoreRefusal::NotSkipped);
        CHECK(restoreRefusal(day, progress, 9, at(12, 0)) == RestoreRefusal::NotSkipped);
        progress.at(0).confirmed = false;
        CHECK(restoreRefusal(day, progress, 0, at(15, 20)) == RestoreRefusal::WindowClosed);
    }
}

TEST_CASE("a flexible block restored goes back to the queue in template order", "[restore]") {
    const DayTemplate day = weekday();

    SECTION("skipped before its turn it simply rejoins the chain") {
        DayProgress progress;
        skipBlock(progress.at(1), timeOfDay(11, 50), 11h + 50min);
        CHECK(plan(day, progress, at(12, 0)).at(2).start == timeOfDay(15, 0));

        const auto result = restoreBlock(day, progress, 1, toInstant(at(12, 0)));

        REQUIRE(result);
        CHECK_FALSE(result->pomodoroCount);
        CHECK_FALSE(result->displaced);
        const DayPlan planned = plan(day, progress, at(12, 0));
        CHECK(planned.at(1).state == BlockState::Upcoming);
        CHECK(planned.at(1).start == timeOfDay(15, 0));
        CHECK(planned.at(2).start == timeOfDay(16, 0));
    }

    SECTION("skipped while running it waits again from now, its phases kept") {
        DayProgress progress;
        progress.at(0).actualStart = timeOfDay(8, 30);
        progress.at(0).actualEnd = timeOfDay(15, 0);
        progress.at(2).actualStart = timeOfDay(15, 0);
        progress.at(2).phases.push_back(PhaseRecord{PhaseKind::Focus, 0, 15h, std::nullopt});
        skipBlock(progress.at(2), timeOfDay(15, 20), 15h + 20min);

        const auto result = restoreBlock(day, progress, 2, toInstant(at(15, 30)));

        REQUIRE(result);
        const BlockProgress& french = progress.at(2);
        CHECK_FALSE(french.actualStart);
        CHECK(french.pauses.empty());
        REQUIRE(french.phases.size() == 1);
        CHECK(french.phases.back().end == Seconds{15h + 20min});
        const DayPlan planned = plan(day, progress, at(15, 30));
        CHECK(planned.at(1).start == timeOfDay(15, 30));
        CHECK(planned.at(2).state == BlockState::Upcoming);
        CHECK(planned.at(2).start == timeOfDay(16, 30));
    }

    SECTION("a postponed block restored takes its template place again") {
        DayProgress progress;
        progress.at(1).postponed = true;
        skipBlock(progress.at(1), timeOfDay(12, 0), 12h);
        REQUIRE(restoreBlock(day, progress, 1, toInstant(at(12, 10))));
        CHECK_FALSE(progress.at(1).postponed);
        CHECK(plan(day, progress, at(12, 10)).at(1).start == timeOfDay(15, 0));
    }

    SECTION("a block that no longer fits shows it like any other") {
        DayTemplate late = day;
        late.blocks[1].durationMinutes = 600min;
        DayProgress progress;
        skipBlock(progress.at(1), timeOfDay(12, 0), 12h);
        REQUIRE(restoreBlock(late, progress, 1, toInstant(at(12, 10))));
        CHECK(plan(late, progress, at(12, 10)).at(1).state == BlockState::DoesNotFit);
    }
}

TEST_CASE("restoring an anchored block returns the flexible block that took its window to Upcoming", "[restore]") {
    const DayTemplate day = weekday();
    DayProgress progress = workRunning();
    skipBlock(progress.at(0), timeOfDay(12, 5), 12h + 5min);
    // Lunch reflowed to 12:05 and the user started it there.
    REQUIRE(plan(day, progress, at(12, 5)).at(1).start == timeOfDay(12, 5));
    progress.at(1).actualStart = timeOfDay(12, 5);
    progress.at(1).pauses.push_back(PauseInterval{timeOfDay(12, 15), std::nullopt});
    progress.at(1).phases.push_back(PhaseRecord{PhaseKind::Focus, 0, 12h + 5min, std::nullopt});
    REQUIRE(plan(day, progress, at(12, 20)).at(1).state == BlockState::Active);

    const auto result = restoreBlock(day, progress, 0, toInstant(at(12, 20)));

    REQUIRE(result);
    CHECK(result->displaced == 1);
    const BlockProgress& lunch = progress.at(1);
    CHECK_FALSE(lunch.actualStart);
    CHECK_FALSE(lunch.actualEnd);
    CHECK(lunch.pauses.empty());
    REQUIRE(lunch.phases.size() == 1);
    CHECK(lunch.phases.back().end == Seconds{12h + 20min});
    CHECK(lunch.phases.back().pauses.empty());

    const DayPlan planned = plan(day, progress, at(12, 20));
    CHECK(planned.at(0).state == BlockState::Active);
    CHECK(planned.at(0).end == timeOfDay(15, 0));
    CHECK(planned.at(1).state == BlockState::Upcoming);
    CHECK(planned.at(1).start == timeOfDay(15, 0));
    CHECK(planned.at(2).start == timeOfDay(16, 0));
    // 160 minutes left fit five pomodoros.
    CHECK(result->pomodoroCount == 5);

    SECTION("a flexible block running outside the window is left alone") {
        DayProgress early;
        skipBlock(early.at(0), timeOfDay(8, 0), 8h);
        early.at(1).actualStart = timeOfDay(8, 0);
        const auto before = restoreBlock(day, early, 0, toInstant(at(8, 10)));
        REQUIRE(before);
        CHECK_FALSE(before->displaced);
        CHECK(early.at(1).actualStart == timeOfDay(8, 0));
        CHECK_FALSE(early.at(0).actualStart);
        CHECK(plan(day, early, at(8, 10)).at(0).state == BlockState::Upcoming);
    }
}

TEST_CASE("a skip and its restore survive the progress file", "[restore]") {
    const DayTemplate day = weekday();
    DayProgress progress = workRunning();
    skipBlock(progress.at(0), timeOfDay(12, 5), 12h + 5min);

    SECTION("while still skipped") {
        const DayProgress reloaded = parseDayProgress(serializeDayProgress(progress));
        CHECK(reloaded == progress);
        CHECK(plan(day, reloaded, at(12, 8)).at(0).state == BlockState::Skipped);
        CHECK(restoreRefusal(day, reloaded, 0, at(12, 8)) == std::nullopt);
    }

    SECTION("after the restore") {
        REQUIRE(restoreBlock(day, progress, 0, toInstant(at(12, 10))));
        const DayProgress reloaded = parseDayProgress(serializeDayProgress(progress));
        CHECK(reloaded == progress);
        REQUIRE(reloaded.find(0));
        CHECK(reloaded.find(0)->pomodoroCount == 5);
        CHECK(reloaded.find(0)->skips.back().restoredAt == timeOfDay(12, 10));
        CHECK(plan(day, reloaded, at(12, 20)).blocks == plan(day, progress, at(12, 20)).blocks);
        CHECK(workedMinutes(plan(day, reloaded, at(12, 20)).at(0), reloaded.find(0), timeOfDay(12, 20)) == 225min);
    }
}

TEST_CASE("a progress file written before skips were timed still restores", "[restore]") {
    const DayTemplate day = weekday();
    const DayProgress old = parseDayProgress(R"({"version": 1, "blocks": {"0": {"actualStart": 510, "skipped": true}}})");
    REQUIRE(old.find(0));
    CHECK(old.find(0)->skips.empty());

    DayProgress progress = old;
    const auto result = restoreBlock(day, progress, 0, toInstant(at(12, 10)));

    REQUIRE(result);
    CHECK_FALSE(progress.at(0).skipped);
    CHECK(progress.at(0).skips.empty());
    CHECK(plan(day, progress, at(12, 10)).at(0).state == BlockState::Active);
    // With no gap on record the whole window counts, as it did before skips were timed.
    CHECK(workedMinutes(plan(day, progress, at(12, 10)).at(0), progress.find(0), timeOfDay(12, 10)) == 220min);
}

TEST_CASE("rearming a restored block lets its alarms fire again, but never for the past", "[restore]") {
    const DayTemplate day = weekday();
    AlarmScheduler scheduler;

    SECTION("an ending soon alarm already fired does not repeat after the restore") {
        DayProgress progress = workRunning();
        scheduler.evaluate(day, plan(day, progress, at(14, 54)), progress, {}, toInstant(at(14, 54)));
        auto alarms = scheduler.evaluate(day, plan(day, progress, at(14, 55)), progress, {}, toInstant(at(14, 55)));
        REQUIRE(alarms.size() == 1);
        CHECK(alarms.front().kind == AlarmKind::BlockEndingSoon);

        skipBlock(progress.at(0), timeOfDay(14, 56), 14h + 56min);
        scheduler.evaluate(day, plan(day, progress, at(14, 56)), progress, {}, toInstant(at(14, 56)));
        REQUIRE(restoreBlock(day, progress, 0, toInstant(at(14, 57))));
        scheduler.rearm(0);
        alarms = scheduler.evaluate(day, plan(day, progress, at(14, 57)), progress, {}, toInstant(at(14, 57)));
        CHECK(alarms.empty());
        alarms = scheduler.evaluate(day, plan(day, progress, at(14, 58)), progress, {}, toInstant(at(14, 58)));
        CHECK(alarms.empty());
    }

    SECTION("an ending soon alarm still ahead fires once after the restore") {
        DayProgress progress = workRunning();
        scheduler.evaluate(day, plan(day, progress, at(12, 4)), progress, {}, toInstant(at(12, 4)));
        skipBlock(progress.at(0), timeOfDay(12, 5), 12h + 5min);
        scheduler.evaluate(day, plan(day, progress, at(12, 5)), progress, {}, toInstant(at(12, 5)));
        REQUIRE(restoreBlock(day, progress, 0, toInstant(at(12, 10))));
        scheduler.rearm(0);
        scheduler.evaluate(day, plan(day, progress, at(12, 10)), progress, {}, toInstant(at(12, 10)));
        scheduler.evaluate(day, plan(day, progress, at(14, 54)), progress, {}, toInstant(at(14, 54)));
        const auto alarms = scheduler.evaluate(day, plan(day, progress, at(14, 55)), progress, {}, toInstant(at(14, 55)));
        REQUIRE(alarms.size() == 1);
        CHECK(alarms.front().kind == AlarmKind::BlockEndingSoon);
        CHECK(alarms.front().blockIndex == 0);
    }

    SECTION("a soft block restored gets its reminder again") {
        DayProgress progress;
        progress.at(0).confirmed = true;
        for (std::size_t i = 1; i < 4; ++i) {
            progress.at(i).actualStart = timeOfDay(15, 0);
            progress.at(i).actualEnd = timeOfDay(15, 5);
        }
        scheduler.evaluate(day, plan(day, progress, at(20, 59)), progress, {}, toInstant(at(20, 59)));
        auto alarms = scheduler.evaluate(day, plan(day, progress, at(21, 0)), progress, {}, toInstant(at(21, 0)));
        REQUIRE(alarms.size() == 1);
        CHECK(alarms.front().kind == AlarmKind::SoftReminder);

        skipBlock(progress.at(4), timeOfDay(21, 2), 21h + 2min);
        scheduler.evaluate(day, plan(day, progress, at(21, 2)), progress, {}, toInstant(at(21, 2)));
        REQUIRE(restoreRefusal(day, progress, 4, at(21, 5)) == std::nullopt);
        REQUIRE(restoreBlock(day, progress, 4, toInstant(at(21, 5))));
        scheduler.rearm(4);
        // The block is placed at the clock again, so the reminder comes with the next evaluation.
        alarms = scheduler.evaluate(day, plan(day, progress, at(21, 5)), progress, {}, toInstant(at(21, 5)));
        REQUIRE(alarms.size() == 1);
        CHECK(alarms.front().kind == AlarmKind::SoftReminder);
        CHECK(alarms.front().blockIndex == 4);
        CHECK(scheduler.evaluate(day, plan(day, progress, at(21, 6)), progress, {}, toInstant(at(21, 6))).empty());
    }
}
