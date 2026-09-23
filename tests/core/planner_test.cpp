#include <cadence/core/planner.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace cadence::core;
using namespace std::chrono_literals;

namespace {

constexpr Date anyDate{std::chrono::year{2026}, std::chrono::September, std::chrono::day{22}};

TimePoint at(int hours, int minutes) {
    return TimePoint{anyDate, timeOfDay(hours, minutes)};
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

BlockTemplate flexiblePomodoros(const char* id, int count) {
    BlockTemplate block;
    block.activityId = id;
    block.kind = BlockKind::Flexible;
    block.pomodoro = PomodoroPlan{};
    block.pomodoro->count = count;
    block.pushupsOnBreak = true;
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

// Mirrors resources/templates/default.json: work, lunch, french, logic, reading.
DayTemplate weekday() {
    DayTemplate day;
    day.dayStart = timeOfDay(8, 30);
    day.dayCutoff = timeOfDay(23, 0);
    day.blocks = {
        anchored("work", timeOfDay(8, 30), 390min),
        flexible("lunch", 60min),
        flexiblePomodoros("french", 2),
        flexiblePomodoros("logic", 1),
        soft("reading", timeOfDay(21, 0), 30min),
    };
    return day;
}

void expectBlock(const DayPlan& result, std::size_t index, Minutes start, Minutes end, BlockState state) {
    INFO("block " << index);
    const PlannedBlock& block = result.at(index);
    CHECK(block.templateIndex == index);
    CHECK(block.start == start);
    CHECK(block.end == end);
    CHECK(block.state == state);
}

} // namespace

TEST_CASE("an untouched day at day start matches the template", "[planner]") {
    const DayPlan result = plan(weekday(), DayProgress{}, at(8, 30));

    REQUIRE(result.blocks.size() == 5);
    expectBlock(result, 0, timeOfDay(8, 30), timeOfDay(15, 0), BlockState::Active);
    expectBlock(result, 1, timeOfDay(15, 0), timeOfDay(16, 0), BlockState::Upcoming);
    expectBlock(result, 2, timeOfDay(16, 0), timeOfDay(17, 0), BlockState::Upcoming);
    expectBlock(result, 3, timeOfDay(17, 0), timeOfDay(17, 30), BlockState::Upcoming);
    expectBlock(result, 4, timeOfDay(21, 0), timeOfDay(21, 30), BlockState::Upcoming);
    CHECK_FALSE(result.doesNotFit());
}

TEST_CASE("a late day start shifts flexible blocks and leaves anchored blocks alone", "[planner]") {
    DayTemplate day;
    day.dayStart = timeOfDay(8, 0);
    day.blocks = {
        flexible("warmup", 60min),
        anchored("standup", timeOfDay(12, 0), 60min),
        flexible("review", 30min),
    };
    DayProgress progress;
    progress.actualDayStart = timeOfDay(9, 0);

    const DayPlan result = plan(day, progress, at(9, 0));

    expectBlock(result, 0, timeOfDay(9, 0), timeOfDay(10, 0), BlockState::Upcoming);
    expectBlock(result, 1, timeOfDay(12, 0), timeOfDay(13, 0), BlockState::Upcoming);
    expectBlock(result, 2, timeOfDay(13, 0), timeOfDay(13, 30), BlockState::Upcoming);
}

TEST_CASE("paused time extends the active block and pushes the flexible blocks after it", "[planner]") {
    DayTemplate day;
    day.dayStart = timeOfDay(9, 0);
    day.blocks = {flexible("deep", 60min), flexible("mail", 30min)};
    DayProgress progress;
    progress.at(0).actualStart = timeOfDay(9, 0);

    SECTION("closed pause") {
        progress.at(0).pauses.push_back(PauseInterval{timeOfDay(9, 10), timeOfDay(9, 20)});
        const DayPlan result = plan(day, progress, at(9, 25));
        expectBlock(result, 0, timeOfDay(9, 0), timeOfDay(10, 10), BlockState::Active);
        expectBlock(result, 1, timeOfDay(10, 10), timeOfDay(10, 40), BlockState::Upcoming);
    }

    SECTION("pause still running counts up to now") {
        progress.at(0).pauses.push_back(PauseInterval{timeOfDay(9, 10), std::nullopt});
        const DayPlan result = plan(day, progress, at(9, 25));
        expectBlock(result, 0, timeOfDay(9, 0), timeOfDay(10, 15), BlockState::Active);
        expectBlock(result, 1, timeOfDay(10, 15), timeOfDay(10, 45), BlockState::Upcoming);
    }
}

TEST_CASE("an extended anchored block moves its end and reflows the chain", "[planner]") {
    DayProgress progress;
    progress.at(0).extended = 30min;

    const DayPlan result = plan(weekday(), progress, at(8, 30));

    expectBlock(result, 0, timeOfDay(8, 30), timeOfDay(15, 30), BlockState::Active);
    expectBlock(result, 1, timeOfDay(15, 30), timeOfDay(16, 30), BlockState::Upcoming);
    expectBlock(result, 2, timeOfDay(16, 30), timeOfDay(17, 30), BlockState::Upcoming);
}

TEST_CASE("a flexible block that would overlap an anchored block moves entirely after it", "[planner]") {
    DayTemplate day;
    day.dayStart = timeOfDay(8, 0);
    day.blocks = {flexible("writing", 120min), anchored("meeting", timeOfDay(9, 0), 60min)};

    const DayPlan result = plan(day, DayProgress{}, at(8, 0));

    expectBlock(result, 0, timeOfDay(10, 0), timeOfDay(12, 0), BlockState::Upcoming);
    expectBlock(result, 1, timeOfDay(9, 0), timeOfDay(10, 0), BlockState::Upcoming);
}

TEST_CASE("a skipped block leaves the chain", "[planner]") {
    DayProgress progress;
    progress.at(1).skipped = true;

    const DayPlan result = plan(weekday(), progress, at(8, 30));

    CHECK(result.at(1).state == BlockState::Skipped);
    expectBlock(result, 2, timeOfDay(15, 0), timeOfDay(16, 0), BlockState::Upcoming);
    expectBlock(result, 3, timeOfDay(16, 0), timeOfDay(16, 30), BlockState::Upcoming);
}

TEST_CASE("a postponed block moves to the end of the flexible chain", "[planner]") {
    DayProgress progress;
    progress.at(1).postponed = true;

    const DayPlan result = plan(weekday(), progress, at(8, 30));

    expectBlock(result, 2, timeOfDay(15, 0), timeOfDay(16, 0), BlockState::Upcoming);
    expectBlock(result, 3, timeOfDay(16, 0), timeOfDay(16, 30), BlockState::Upcoming);
    expectBlock(result, 1, timeOfDay(16, 30), timeOfDay(17, 30), BlockState::Upcoming);
}

TEST_CASE("blocks that end after the cutoff do not fit", "[planner]") {
    DayTemplate day = weekday();
    day.blocks[1].durationMinutes = 600min;

    const DayPlan result = plan(day, DayProgress{}, at(8, 30));

    CHECK(result.at(0).state == BlockState::Active);
    CHECK(result.at(1).state == BlockState::DoesNotFit);
    CHECK(result.at(2).state == BlockState::DoesNotFit);
    CHECK(result.at(3).state == BlockState::DoesNotFit);
    CHECK(result.at(4).state == BlockState::DoesNotFit);
    CHECK(result.doesNotFit());
}

TEST_CASE("a soft block waits for the chain and never pushes other blocks", "[planner]") {
    DayTemplate day;
    day.dayStart = timeOfDay(8, 0);
    day.blocks = {soft("reading", timeOfDay(8, 0), 30min), flexible("writing", 60min), flexible("mail", 15min)};

    SECTION("listed first, it still comes after the flexible chain") {
        const DayPlan result = plan(day, DayProgress{}, at(8, 0));
        expectBlock(result, 1, timeOfDay(8, 0), timeOfDay(9, 0), BlockState::Upcoming);
        expectBlock(result, 2, timeOfDay(9, 0), timeOfDay(9, 15), BlockState::Upcoming);
        expectBlock(result, 0, timeOfDay(9, 15), timeOfDay(9, 45), BlockState::Upcoming);
    }

    SECTION("its earliest start wins when the chain finishes before it") {
        day.blocks[0].start = timeOfDay(11, 0);
        const DayPlan result = plan(day, DayProgress{}, at(8, 0));
        expectBlock(result, 0, timeOfDay(11, 0), timeOfDay(11, 30), BlockState::Upcoming);
    }

    SECTION("once started it keeps its actual start") {
        DayProgress progress;
        progress.at(0).actualStart = timeOfDay(9, 30);
        const DayPlan result = plan(day, progress, at(9, 40));
        expectBlock(result, 0, timeOfDay(9, 30), timeOfDay(10, 0), BlockState::Active);
        expectBlock(result, 1, timeOfDay(9, 40), timeOfDay(10, 40), BlockState::Upcoming);
        expectBlock(result, 2, timeOfDay(10, 40), timeOfDay(10, 55), BlockState::Upcoming);
    }
}

TEST_CASE("an active block that runs late pushes the blocks after it", "[planner]") {
    DayTemplate day;
    day.dayStart = timeOfDay(9, 0);
    day.blocks = {flexible("deep", 60min), flexible("mail", 30min)};
    DayProgress progress;
    progress.at(0).actualStart = timeOfDay(9, 0);

    const DayPlan result = plan(day, progress, at(10, 30));

    expectBlock(result, 0, timeOfDay(9, 0), timeOfDay(10, 30), BlockState::Active);
    expectBlock(result, 1, timeOfDay(10, 30), timeOfDay(11, 0), BlockState::Upcoming);
}

TEST_CASE("an unstarted flexible block never starts in the past", "[planner]") {
    const DayPlan result = plan(weekday(), DayProgress{}, at(15, 20));

    expectBlock(result, 0, timeOfDay(8, 30), timeOfDay(15, 0), BlockState::Unconfirmed);
    expectBlock(result, 1, timeOfDay(15, 20), timeOfDay(16, 20), BlockState::Upcoming);
}

TEST_CASE("an untouched anchored block whose time has passed is unconfirmed", "[planner]") {
    SECTION("it is listed for the app to ask about and counts toward nothing") {
        const DayPlan result = plan(weekday(), DayProgress{}, at(15, 20));

        CHECK(result.at(0).state == BlockState::Unconfirmed);
        CHECK(result.unconfirmedBlocks() == std::vector<std::size_t>{0});
        CHECK(result.completedMinutes() == 0min);
    }

    SECTION("confirming it makes it done and counts its planned length") {
        DayProgress progress;
        progress.at(0).confirmed = true;
        const DayPlan result = plan(weekday(), progress, at(15, 20));

        expectBlock(result, 0, timeOfDay(8, 30), timeOfDay(15, 0), BlockState::Done);
        CHECK(result.unconfirmedBlocks().empty());
        CHECK(result.completedMinutes() == 390min);
    }

    SECTION("denying it makes it skipped") {
        DayProgress progress;
        progress.at(0).confirmed = false;
        const DayPlan result = plan(weekday(), progress, at(15, 20));

        CHECK(result.at(0).state == BlockState::Skipped);
        CHECK(result.unconfirmedBlocks().empty());
        CHECK(result.completedMinutes() == 0min);
    }

    SECTION("a block still running at the clock is active, not unconfirmed") {
        const DayPlan result = plan(weekday(), DayProgress{}, at(14, 59));
        CHECK(result.at(0).state == BlockState::Active);
        CHECK(result.unconfirmedBlocks().empty());
    }

    SECTION("flexible blocks are never unconfirmed") {
        DayProgress progress;
        progress.at(0).confirmed = true;
        const DayPlan result = plan(weekday(), progress, at(22, 30));
        CHECK(result.at(1).state == BlockState::DoesNotFit);
        CHECK(result.unconfirmedBlocks().empty());
    }
}

TEST_CASE("completed minutes only count blocks that are done", "[planner]") {
    DayProgress progress;
    progress.at(0).confirmed = true;
    progress.at(1).actualStart = timeOfDay(15, 0);
    progress.at(1).actualEnd = timeOfDay(15, 45);
    progress.at(2).actualStart = timeOfDay(15, 45);

    const DayPlan result = plan(weekday(), progress, at(16, 0));

    CHECK(result.at(1).state == BlockState::Done);
    CHECK(result.at(2).state == BlockState::Active);
    CHECK(result.completedMinutes() == 390min + 45min);
}

TEST_CASE("finished blocks keep their actual times and the chain continues from them", "[planner]") {
    DayProgress progress;
    progress.at(0).actualStart = timeOfDay(8, 30);
    progress.at(0).actualEnd = timeOfDay(14, 40);

    const DayPlan result = plan(weekday(), progress, at(14, 40));

    expectBlock(result, 0, timeOfDay(8, 30), timeOfDay(14, 40), BlockState::Done);
    expectBlock(result, 1, timeOfDay(14, 40), timeOfDay(15, 40), BlockState::Upcoming);
}
