#pragma once

#include <cadence/core/block.hpp>
#include <cadence/core/phase.hpp>
#include <cadence/core/time.hpp>

#include <optional>
#include <variant>
#include <vector>

namespace cadence::core {

enum class PomodoroState {
    Idle,
    Focus,
    ShortBreak,
    LongBreak,
    Paused,
    Completed,
};

struct PhaseStarted {
    PomodoroState phase;
    // Index of the focus session the phase belongs to; a break carries the index of the session it follows.
    int index;
    // When the phase began: the scheduled end of the previous one when replayed, else the call's now.
    Instant at{};

    friend bool operator==(const PhaseStarted&, const PhaseStarted&) = default;
};

struct PushupPrompt {
    int setIndex;

    friend bool operator==(const PushupPrompt&, const PushupPrompt&) = default;
};

struct SessionCompleted {
    Instant at{};

    friend bool operator==(const SessionCompleted&, const SessionCompleted&) = default;
};

PhaseKind phaseKindOf(PomodoroState state) noexcept;
PomodoroState stateOf(PhaseKind kind) noexcept;

using PomodoroEvent = std::variant<PhaseStarted, PushupPrompt, SessionCompleted>;
using PomodoroEvents = std::vector<PomodoroEvent>;

// Every operation returns the events it produced so a UI layer can react without subscribing.
class PomodoroSession {
public:
    PomodoroSession(PomodoroPlan plan, int count, bool pushupsOnBreak) noexcept;

    // Empty when the block has no pomodoro plan or its count cannot be resolved.
    static std::optional<PomodoroSession> fromTemplate(const BlockTemplate& block) noexcept;

    // Rebuilds a session from the phases recorded for the block, positioned in the last recorded
    // phase with its pauses applied. A phase that already ended is caught up by the next tick.
    // Empty when the block has no plan or nothing was recorded. `count` replaces the template's
    // count, for a session that was started over the time left after a restore.
    static std::optional<PomodoroSession> restore(const BlockTemplate& block,
                                                  const std::vector<PhaseRecord>& history,
                                                  std::chrono::local_days day,
                                                  std::optional<int> count = std::nullopt) noexcept;

    PomodoroEvents start(Instant now);
    PomodoroEvents pause(Instant now);
    PomodoroEvents resume(Instant now);
    PomodoroEvents skip(Instant now);
    PomodoroEvents tick(Instant now);

    PomodoroState state() const noexcept { return state_; }
    // The timed phase, also while paused, so a UI can show what will resume.
    PomodoroState activePhase() const noexcept {
        return state_ == PomodoroState::Paused ? pausedFrom_ : state_;
    }
    int currentIndex() const noexcept { return index_; }
    int totalCount() const noexcept { return count_; }
    // Focus sessions finished so far; a break counts the session it follows as finished.
    int completedSessions() const noexcept;
    const PomodoroPlan& plan() const noexcept { return plan_; }
    bool pushupsOnBreak() const noexcept { return pushupsOnBreak_; }
    Seconds remaining(Instant now) const noexcept;

private:
    bool inTimedPhase() const noexcept;
    void advance(Instant now, PomodoroEvents& events);
    void finishPhase(Instant at, PomodoroEvents& events);

    PomodoroPlan plan_;
    int count_;
    bool pushupsOnBreak_;
    PomodoroState state_ = PomodoroState::Idle;
    PomodoroState pausedFrom_ = PomodoroState::Idle;
    int index_ = 0;
    Instant phaseEnd_{};
    Seconds pausedRemaining_{0};
};

} // namespace cadence::core
