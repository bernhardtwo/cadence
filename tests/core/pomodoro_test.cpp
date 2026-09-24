#include <cadence/core/clock.hpp>
#include <cadence/core/pomodoro.hpp>

#include <catch2/catch_test_macros.hpp>

#include <variant>
#include <vector>

using namespace cadence::core;
using namespace std::chrono_literals;

namespace {

constexpr Date anyDate{std::chrono::year{2026}, std::chrono::September, std::chrono::day{22}};

Instant at(int hours, int minutes) {
    return toInstant(TimePoint{anyDate, timeOfDay(hours, minutes)});
}

template <typename Event> bool has(const PomodoroEvents& events, const Event& expected) {
    for (const PomodoroEvent& event : events) {
        if (const Event* actual = std::get_if<Event>(&event); actual && *actual == expected) {
            return true;
        }
    }
    return false;
}

bool hasPhase(const PomodoroEvents& events, PomodoroState phase, int index) {
    for (const PomodoroEvent& event : events) {
        if (const auto* started = std::get_if<PhaseStarted>(&event);
            started && started->phase == phase && started->index == index) {
            return true;
        }
    }
    return false;
}

template <typename Event> bool hasAny(const PomodoroEvents& events) {
    for (const PomodoroEvent& event : events) {
        if (std::holds_alternative<Event>(event)) {
            return true;
        }
    }
    return false;
}

// Runs the session to completion, ticking exactly at each phase end, and returns the phase sequence.
std::vector<PomodoroState> phaseSequence(PomodoroSession& session, FakeClock& clock) {
    std::vector<PomodoroState> phases;
    session.start(clock.now());
    phases.push_back(session.state());
    while (session.state() != PomodoroState::Completed) {
        clock.advance(session.remaining(clock.now()));
        session.tick(clock.now());
        phases.push_back(session.state());
    }
    return phases;
}

} // namespace

TEST_CASE("pomodoro duration counts focus time and the breaks between sessions", "[pomodoro]") {
    const PomodoroPlan plan;
    CHECK(pomodoroDuration(plan, 0) == 0min);
    CHECK(pomodoroDuration(plan, 1) == 25min);
    CHECK(pomodoroDuration(plan, 2) == 55min);
    CHECK(pomodoroDuration(plan, 4) == 115min);
    CHECK(pomodoroDuration(plan, 5) == 155min);
    CHECK(pomodoroDuration(plan, 12) == 375min);
    CHECK(pomodoroDuration(plan, 13) == 415min);

    PomodoroPlan noLongBreaks;
    noLongBreaks.longBreakEvery = 0;
    CHECK(pomodoroDuration(noLongBreaks, 5) == 145min);
}

TEST_CASE("pomodoro count is the largest number of sessions that fits the duration", "[pomodoro]") {
    const PomodoroPlan plan;
    CHECK(derivePomodoroCount(plan, 390min) == 12);
    CHECK(derivePomodoroCount(plan, 375min) == 12);
    CHECK(derivePomodoroCount(plan, 374min) == 11);
    CHECK(derivePomodoroCount(plan, 59min) == 2);
    CHECK(derivePomodoroCount(plan, 29min) == 1);
    CHECK(derivePomodoroCount(plan, 24min) == 0);

    BlockTemplate work;
    work.pomodoro = plan;
    work.durationMinutes = 390min;
    CHECK(resolvedPomodoroCount(work) == 12);

    BlockTemplate french;
    french.pomodoro = plan;
    french.pomodoro->count = 2;
    CHECK(resolvedPomodoroCount(french) == 2);
    CHECK(resolvedDuration(french) == 55min);

    BlockTemplate logic;
    logic.pomodoro = plan;
    logic.pomodoro->count = 1;
    CHECK(resolvedDuration(logic) == 25min);

    BlockTemplate plain;
    plain.durationMinutes = 45min;
    CHECK_FALSE(resolvedPomodoroCount(plain).has_value());
    CHECK_FALSE(PomodoroSession::fromTemplate(plain).has_value());
}

TEST_CASE("a long break follows every fourth focus session and never the last one", "[pomodoro]") {
    FakeClock clock(at(9, 0));
    PomodoroSession session(PomodoroPlan{}, 8, false);

    const std::vector<PomodoroState> expected = {
        PomodoroState::Focus,     PomodoroState::ShortBreak, PomodoroState::Focus, PomodoroState::ShortBreak,
        PomodoroState::Focus,     PomodoroState::ShortBreak, PomodoroState::Focus, PomodoroState::LongBreak,
        PomodoroState::Focus,     PomodoroState::ShortBreak, PomodoroState::Focus, PomodoroState::ShortBreak,
        PomodoroState::Focus,     PomodoroState::ShortBreak, PomodoroState::Focus, PomodoroState::Completed,
    };
    CHECK(phaseSequence(session, clock) == expected);
    CHECK(session.currentIndex() == 7);
    CHECK(session.totalCount() == 8);
    CHECK(session.remaining(clock.now()) == 0s);
}

TEST_CASE("exactly four sessions end with completion instead of a long break", "[pomodoro]") {
    FakeClock clock(at(9, 0));
    PomodoroSession session(PomodoroPlan{}, 4, false);

    const std::vector<PomodoroState> phases = phaseSequence(session, clock);
    REQUIRE(phases.size() == 8);
    CHECK(phases[6] == PomodoroState::Focus);
    CHECK(phases[7] == PomodoroState::Completed);
}

TEST_CASE("a single session completes without any break", "[pomodoro]") {
    PomodoroSession session(PomodoroPlan{}, 1, true);
    session.start(at(9, 0));

    const PomodoroEvents events = session.tick(at(9, 25));

    CHECK(session.state() == PomodoroState::Completed);
    CHECK(hasAny<SessionCompleted>(events));
    CHECK_FALSE(hasAny<PushupPrompt>(events));
}

TEST_CASE("entering a break prompts for push-ups when the block asks for them", "[pomodoro]") {
    SECTION("with push-ups") {
        PomodoroSession session(PomodoroPlan{}, 3, true);
        session.start(at(9, 0));

        const PomodoroEvents first = session.tick(at(9, 25));
        CHECK(session.state() == PomodoroState::ShortBreak);
        CHECK(hasPhase(first, PomodoroState::ShortBreak, 0));
        CHECK(has(first, PushupPrompt{0}));

        session.tick(at(9, 30));
        const PomodoroEvents second = session.tick(at(9, 55));
        CHECK(has(second, PushupPrompt{1}));
    }

    SECTION("without push-ups") {
        PomodoroSession session(PomodoroPlan{}, 3, false);
        session.start(at(9, 0));
        const PomodoroEvents events = session.tick(at(9, 25));
        CHECK(session.state() == PomodoroState::ShortBreak);
        CHECK_FALSE(hasAny<PushupPrompt>(events));
    }
}

TEST_CASE("pause and resume keep the remaining time", "[pomodoro]") {
    FakeClock clock(at(9, 0));
    PomodoroSession session(PomodoroPlan{}, 2, false);
    session.start(clock.now());

    clock.advance(10min);
    session.pause(clock.now());
    CHECK(session.state() == PomodoroState::Paused);
    CHECK(session.remaining(clock.now()) == 15min);

    clock.advance(30min);
    CHECK(session.remaining(clock.now()) == 15min);
    CHECK(session.tick(clock.now()).empty());

    session.resume(clock.now());
    CHECK(session.state() == PomodoroState::Focus);
    CHECK(session.remaining(clock.now()) == 15min);

    clock.advance(14min);
    session.tick(clock.now());
    CHECK(session.state() == PomodoroState::Focus);

    clock.advance(1min);
    session.tick(clock.now());
    CHECK(session.state() == PomodoroState::ShortBreak);
}

TEST_CASE("skip ends the current phase immediately", "[pomodoro]") {
    PomodoroSession session(PomodoroPlan{}, 2, false);
    session.start(at(9, 0));

    const PomodoroEvents events = session.skip(at(9, 5));
    CHECK(session.state() == PomodoroState::ShortBreak);
    CHECK(hasPhase(events, PomodoroState::ShortBreak, 0));
    CHECK(session.remaining(at(9, 5)) == 5min);

    session.skip(at(9, 6));
    CHECK(session.state() == PomodoroState::Focus);
    CHECK(session.currentIndex() == 1);

    SECTION("skipping while paused acts on the paused phase") {
        session.pause(at(9, 10));
        session.skip(at(9, 12));
        CHECK(session.state() == PomodoroState::Completed);
    }
}

TEST_CASE("a coarse tick replays the phases that elapsed in between", "[pomodoro]") {
    PomodoroSession session(PomodoroPlan{}, 3, true);
    session.start(at(9, 0));

    const PomodoroEvents events = session.tick(at(9, 40));

    CHECK(session.state() == PomodoroState::Focus);
    CHECK(session.currentIndex() == 1);
    CHECK(session.remaining(at(9, 40)) == 15min);
    CHECK(hasPhase(events, PomodoroState::ShortBreak, 0));
    CHECK(has(events, PushupPrompt{0}));
    CHECK(hasPhase(events, PomodoroState::Focus, 1));
}

TEST_CASE("operations outside their state are ignored", "[pomodoro]") {
    PomodoroSession session(PomodoroPlan{}, 2, false);
    CHECK(session.pause(at(9, 0)).empty());
    CHECK(session.resume(at(9, 0)).empty());
    CHECK(session.skip(at(9, 0)).empty());
    CHECK(session.state() == PomodoroState::Idle);
    CHECK(session.remaining(at(9, 0)) == 25min);

    session.start(at(9, 0));
    CHECK(session.start(at(9, 1)).empty());
    CHECK(session.resume(at(9, 1)).empty());
    CHECK(session.state() == PomodoroState::Focus);
}

TEST_CASE("completed sessions count the focus phases that have ended", "[pomodoro]") {
    PomodoroSession session(PomodoroPlan{}, 3, false);
    FakeClock clock(at(9, 0));
    CHECK(session.completedSessions() == 0);

    session.start(clock.now());
    CHECK(session.completedSessions() == 0);

    clock.advance(25min);
    session.tick(clock.now());
    REQUIRE(session.state() == PomodoroState::ShortBreak);
    CHECK(session.completedSessions() == 1);

    session.pause(clock.now());
    CHECK(session.completedSessions() == 1);
    CHECK(session.activePhase() == PomodoroState::ShortBreak);
    session.resume(clock.now());

    clock.advance(5min);
    session.tick(clock.now());
    REQUIRE(session.state() == PomodoroState::Focus);
    CHECK(session.completedSessions() == 1);

    session.pause(clock.now());
    CHECK(session.completedSessions() == 1);
    CHECK(session.activePhase() == PomodoroState::Focus);
    session.resume(clock.now());

    clock.advance(25min + 5min + 25min);
    session.tick(clock.now());
    REQUIRE(session.state() == PomodoroState::Completed);
    CHECK(session.completedSessions() == 3);
}

namespace {

BlockTemplate pomodoroBlock(int count) {
    BlockTemplate block;
    block.activityId = "work";
    block.pomodoro = PomodoroPlan{};
    block.pomodoro->count = count;
    block.pushupsOnBreak = true;
    return block;
}

constexpr std::chrono::local_days today{anyDate};

Seconds sod(int hours, int minutes, int seconds = 0) {
    return Seconds{hours * 3600 + minutes * 60 + seconds};
}

} // namespace

TEST_CASE("events carry the instant the phase began", "[pomodoro]") {
    PomodoroSession session(PomodoroPlan{}, 3, false);
    FakeClock clock(at(9, 0));
    PomodoroEvents events = session.start(clock.now());
    REQUIRE(events.size() == 1);
    CHECK(std::get<PhaseStarted>(events[0]).at == at(9, 0));

    // A coarse tick replays the break at its scheduled instant, not at now.
    clock.advance(40min);
    events = session.tick(clock.now());
    REQUIRE(events.size() == 2);
    CHECK(std::get<PhaseStarted>(events[0]).at == at(9, 25));
    CHECK(std::get<PhaseStarted>(events[1]).at == at(9, 30));
}

TEST_CASE("restore rebuilds a session in the middle of a focus phase", "[pomodoro][restore]") {
    const std::vector<PhaseRecord> history = {
        PhaseRecord{PhaseKind::Focus, 0, sod(9, 0), sod(9, 25)},
        PhaseRecord{PhaseKind::ShortBreak, 0, sod(9, 25), sod(9, 30)},
        PhaseRecord{PhaseKind::Focus, 1, sod(9, 30)},
    };
    auto session = PomodoroSession::restore(pomodoroBlock(4), history, today);
    REQUIRE(session);
    CHECK(session->state() == PomodoroState::Focus);
    CHECK(session->currentIndex() == 1);
    CHECK(session->completedSessions() == 1);
    CHECK(session->remaining(at(9, 40)) == 15min);
    CHECK(session->tick(at(9, 40)).empty());
}

TEST_CASE("restore rebuilds a session in the middle of a break", "[pomodoro][restore]") {
    const std::vector<PhaseRecord> history = {
        PhaseRecord{PhaseKind::Focus, 0, sod(9, 0), sod(9, 25)},
        PhaseRecord{PhaseKind::ShortBreak, 0, sod(9, 25)},
    };
    auto session = PomodoroSession::restore(pomodoroBlock(4), history, today);
    REQUIRE(session);
    CHECK(session->state() == PomodoroState::ShortBreak);
    CHECK(session->completedSessions() == 1);
    CHECK(session->remaining(at(9, 27)) == 3min);

    // The break ends while the app was down: the next tick moves on to the second focus.
    const PomodoroEvents events = session->tick(at(9, 31));
    REQUIRE(events.size() == 1);
    CHECK(std::get<PhaseStarted>(events[0]) == PhaseStarted{PomodoroState::Focus, 1, at(9, 30)});
    CHECK(session->remaining(at(9, 31)) == 24min);
}

TEST_CASE("restore keeps a paused phase paused with its frozen remaining time", "[pomodoro][restore]") {
    PhaseRecord focus{PhaseKind::Focus, 2, sod(11, 0)};
    focus.pauses.push_back(PhasePause{sod(11, 5), sod(11, 7)});
    focus.pauses.push_back(PhasePause{sod(11, 10), std::nullopt});
    auto session = PomodoroSession::restore(pomodoroBlock(4), {focus}, today);
    REQUIRE(session);
    CHECK(session->state() == PomodoroState::Paused);
    CHECK(session->activePhase() == PomodoroState::Focus);
    CHECK(session->currentIndex() == 2);
    // Eight minutes of focus elapsed before the open pause: 11:00 to 11:10 less the two minute pause.
    CHECK(session->remaining(at(12, 0)) == 17min);
    CHECK(session->tick(at(12, 0)).empty());

    session->resume(at(12, 0));
    CHECK(session->state() == PomodoroState::Focus);
    CHECK(session->remaining(at(12, 0)) == 17min);
}

TEST_CASE("restore accounts for finished pauses of a running phase", "[pomodoro][restore]") {
    PhaseRecord focus{PhaseKind::Focus, 0, sod(9, 0)};
    focus.pauses.push_back(PhasePause{sod(9, 5), sod(9, 15)});
    auto session = PomodoroSession::restore(pomodoroBlock(2), {focus}, today);
    REQUIRE(session);
    CHECK(session->state() == PomodoroState::Focus);
    CHECK(session->remaining(at(9, 20)) == 15min);
}

TEST_CASE("restore has nothing to rebuild without recorded phases or a plan", "[pomodoro][restore]") {
    CHECK_FALSE(PomodoroSession::restore(pomodoroBlock(2), {}, today).has_value());
    BlockTemplate plain;
    plain.activityId = "lunch";
    plain.durationMinutes = 60min;
    CHECK_FALSE(PomodoroSession::restore(plain, {PhaseRecord{PhaseKind::Focus, 0, sod(9, 0)}}, today).has_value());
}

TEST_CASE("restore after a completed last phase replays the rest on the next tick", "[pomodoro][restore]") {
    const std::vector<PhaseRecord> history = {
        PhaseRecord{PhaseKind::Focus, 1, sod(9, 30), sod(9, 55)},
    };
    auto session = PomodoroSession::restore(pomodoroBlock(2), history, today);
    REQUIRE(session);
    const PomodoroEvents events = session->tick(at(10, 0));
    REQUIRE(events.size() == 1);
    CHECK(std::get<SessionCompleted>(events[0]).at == at(9, 55));
    CHECK(session->state() == PomodoroState::Completed);
}
