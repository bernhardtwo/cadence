#pragma once

#include <cadence/core/time.hpp>

#include <optional>
#include <vector>

namespace cadence::core {

// What happened inside a block's pomodoro session, as recorded in the day's progress. Times are
// seconds since local midnight so a restart can rebuild the session with the right remaining time.
enum class PhaseKind {
    Focus,
    ShortBreak,
    LongBreak,
};

struct PhasePause {
    Seconds start;
    // Empty while the pause is still running.
    std::optional<Seconds> end;

    friend bool operator==(const PhasePause&, const PhasePause&) = default;
};

struct PhaseRecord {
    PhaseKind kind;
    // Index of the focus session; a break carries the index of the session it follows.
    int index;
    Seconds start;
    // Empty while the phase is still running.
    std::optional<Seconds> end;
    // Ended by the user rather than by the clock.
    bool skipped = false;
    std::vector<PhasePause> pauses;

    friend bool operator==(const PhaseRecord&, const PhaseRecord&) = default;
};

enum class PromptAnswer {
    Pending,
    Logged,
    Skipped,
};

// A push-up prompt shown at the start of a break and what the user did with it.
struct PromptRecord {
    int setIndex;
    Seconds shown;
    PromptAnswer answer = PromptAnswer::Pending;

    friend bool operator==(const PromptRecord&, const PromptRecord&) = default;
};

// Ends the last phase, and a pause still open inside it, at the given time. Used when the block
// is finished or skipped by hand so no record is ever left open. Returns whether anything closed.
bool closeOpenPhase(std::vector<PhaseRecord>& phases, Seconds at) noexcept;

} // namespace cadence::core
