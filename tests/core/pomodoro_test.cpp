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
    CHECK(has(events, SessionCompleted{}));
    CHECK_FALSE(hasAny<PushupPrompt>(events));
}

TEST_CASE("entering a break prompts for push-ups when the block asks for them", "[pomodoro]") {
    SECTION("with push-ups") {
        PomodoroSession session(PomodoroPlan{}, 3, true);
        session.start(at(9, 0));

        const PomodoroEvents first = session.tick(at(9, 25));
        CHECK(session.state() == PomodoroState::ShortBreak);
        CHECK(has(first, PhaseStarted{PomodoroState::ShortBreak, 0}));
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
    CHECK(has(events, PhaseStarted{PomodoroState::ShortBreak, 0}));
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
    CHECK(has(events, PhaseStarted{PomodoroState::ShortBreak, 0}));
    CHECK(has(events, PushupPrompt{0}));
    CHECK(has(events, PhaseStarted{PomodoroState::Focus, 1}));
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
