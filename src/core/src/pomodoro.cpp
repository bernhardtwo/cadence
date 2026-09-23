#include <cadence/core/pomodoro.hpp>

#include <algorithm>

namespace cadence::core {

PomodoroSession::PomodoroSession(PomodoroPlan plan, int count, bool pushupsOnBreak) noexcept
    : plan_(plan), count_(std::max(count, 0)), pushupsOnBreak_(pushupsOnBreak) {}

std::optional<PomodoroSession> PomodoroSession::fromTemplate(const BlockTemplate& block) noexcept {
    const std::optional<int> count = resolvedPomodoroCount(block);
    if (!block.pomodoro || !count || *count <= 0) {
        return std::nullopt;
    }
    return PomodoroSession(*block.pomodoro, *count, block.pushupsOnBreak);
}

bool PomodoroSession::inTimedPhase() const noexcept {
    return state_ == PomodoroState::Focus || state_ == PomodoroState::ShortBreak ||
           state_ == PomodoroState::LongBreak;
}

PomodoroEvents PomodoroSession::start(Instant now) {
    PomodoroEvents events;
    if (state_ != PomodoroState::Idle) {
        return events;
    }
    if (count_ == 0) {
        state_ = PomodoroState::Completed;
        events.push_back(SessionCompleted{});
        return events;
    }
    index_ = 0;
    state_ = PomodoroState::Focus;
    phaseEnd_ = now + plan_.focus;
    events.push_back(PhaseStarted{state_, index_});
    advance(now, events);
    return events;
}

PomodoroEvents PomodoroSession::pause(Instant now) {
    PomodoroEvents events;
    if (!inTimedPhase()) {
        return events;
    }
    advance(now, events);
    if (!inTimedPhase()) {
        return events;
    }
    pausedRemaining_ = std::max(Seconds{0}, phaseEnd_ - now);
    pausedFrom_ = state_;
    state_ = PomodoroState::Paused;
    return events;
}

PomodoroEvents PomodoroSession::resume(Instant now) {
    PomodoroEvents events;
    if (state_ != PomodoroState::Paused) {
        return events;
    }
    state_ = pausedFrom_;
    phaseEnd_ = now + pausedRemaining_;
    advance(now, events);
    return events;
}

PomodoroEvents PomodoroSession::skip(Instant now) {
    PomodoroEvents events;
    if (state_ == PomodoroState::Paused) {
        state_ = pausedFrom_;
    }
    if (!inTimedPhase()) {
        return events;
    }
    finishPhase(now, events);
    advance(now, events);
    return events;
}

PomodoroEvents PomodoroSession::tick(Instant now) {
    PomodoroEvents events;
    advance(now, events);
    return events;
}

Seconds PomodoroSession::remaining(Instant now) const noexcept {
    switch (state_) {
    case PomodoroState::Idle:
        return plan_.focus;
    case PomodoroState::Paused:
        return pausedRemaining_;
    case PomodoroState::Completed:
        return Seconds{0};
    case PomodoroState::Focus:
    case PomodoroState::ShortBreak:
    case PomodoroState::LongBreak:
        return std::max(Seconds{0}, phaseEnd_ - now);
    }
    return Seconds{0};
}

// Phases that elapsed between two ticks are replayed from their scheduled ends, not from now, so the
// timeline stays exact however coarse the ticks are.
void PomodoroSession::advance(Instant now, PomodoroEvents& events) {
    while (inTimedPhase() && now >= phaseEnd_) {
        finishPhase(phaseEnd_, events);
    }
}

void PomodoroSession::finishPhase(Instant at, PomodoroEvents& events) {
    if (state_ == PomodoroState::Focus) {
        if (index_ + 1 >= count_) {
            state_ = PomodoroState::Completed;
            events.push_back(SessionCompleted{});
            return;
        }
        const bool longBreak = plan_.longBreakEvery > 0 && (index_ + 1) % plan_.longBreakEvery == 0;
        state_ = longBreak ? PomodoroState::LongBreak : PomodoroState::ShortBreak;
        phaseEnd_ = at + (longBreak ? plan_.longBreak : plan_.shortBreak);
        events.push_back(PhaseStarted{state_, index_});
        if (pushupsOnBreak_) {
            events.push_back(PushupPrompt{index_});
        }
        return;
    }

    ++index_;
    state_ = PomodoroState::Focus;
    phaseEnd_ = at + plan_.focus;
    events.push_back(PhaseStarted{state_, index_});
}

} // namespace cadence::core
