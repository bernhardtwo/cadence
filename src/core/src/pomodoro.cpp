#include <cadence/core/pomodoro.hpp>

#include <algorithm>

namespace cadence::core {

PhaseKind phaseKindOf(PomodoroState state) noexcept {
    switch (state) {
    case PomodoroState::ShortBreak:
        return PhaseKind::ShortBreak;
    case PomodoroState::LongBreak:
        return PhaseKind::LongBreak;
    case PomodoroState::Idle:
    case PomodoroState::Focus:
    case PomodoroState::Paused:
    case PomodoroState::Completed:
        break;
    }
    return PhaseKind::Focus;
}

PomodoroState stateOf(PhaseKind kind) noexcept {
    switch (kind) {
    case PhaseKind::Focus:
        return PomodoroState::Focus;
    case PhaseKind::ShortBreak:
        return PomodoroState::ShortBreak;
    case PhaseKind::LongBreak:
        return PomodoroState::LongBreak;
    }
    return PomodoroState::Focus;
}

PomodoroSession::PomodoroSession(PomodoroPlan plan, int count, bool pushupsOnBreak) noexcept
    : plan_(plan), count_(std::max(count, 0)), pushupsOnBreak_(pushupsOnBreak) {}

std::optional<PomodoroSession> PomodoroSession::fromTemplate(const BlockTemplate& block) noexcept {
    const std::optional<int> count = resolvedPomodoroCount(block);
    if (!block.pomodoro || !count || *count <= 0) {
        return std::nullopt;
    }
    return PomodoroSession(*block.pomodoro, *count, block.pushupsOnBreak);
}

std::optional<PomodoroSession> PomodoroSession::restore(const BlockTemplate& block,
                                                        const std::vector<PhaseRecord>& history,
                                                        std::chrono::local_days day) noexcept {
    std::optional<PomodoroSession> session = fromTemplate(block);
    if (!session || history.empty()) {
        return std::nullopt;
    }
    const PhaseRecord& last = history.back();
    const PomodoroState phase = stateOf(last.kind);
    Minutes length = session->plan_.focus;
    if (phase == PomodoroState::ShortBreak) {
        length = session->plan_.shortBreak;
    } else if (phase == PomodoroState::LongBreak) {
        length = session->plan_.longBreak;
    }
    session->index_ = std::clamp(last.index, 0, std::max(0, session->count_ - 1));
    session->state_ = phase;

    if (last.end) {
        // The phase is over; the next tick replays whatever followed from its end.
        session->phaseEnd_ = day + *last.end;
        return session;
    }
    Seconds paused{0};
    for (const PhasePause& pause : last.pauses) {
        if (pause.end) {
            paused += std::max(Seconds{0}, *pause.end - pause.start);
        } else {
            // Still paused: the remaining time froze when the pause began.
            const Seconds elapsed = std::max(Seconds{0}, pause.start - last.start - paused);
            session->pausedFrom_ = phase;
            session->state_ = PomodoroState::Paused;
            session->pausedRemaining_ =
                std::max(Seconds{0}, std::chrono::duration_cast<Seconds>(length) - elapsed);
            return session;
        }
    }
    session->phaseEnd_ = day + last.start + length + paused;
    return session;
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
        events.push_back(SessionCompleted{now});
        return events;
    }
    index_ = 0;
    state_ = PomodoroState::Focus;
    phaseEnd_ = now + plan_.focus;
    events.push_back(PhaseStarted{state_, index_, now});
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

int PomodoroSession::completedSessions() const noexcept {
    switch (activePhase()) {
    case PomodoroState::Idle:
        return 0;
    case PomodoroState::Focus:
        return index_;
    case PomodoroState::ShortBreak:
    case PomodoroState::LongBreak:
        return index_ + 1;
    case PomodoroState::Completed:
        return count_;
    case PomodoroState::Paused:
        break;
    }
    return index_;
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
            events.push_back(SessionCompleted{at});
            return;
        }
        const bool longBreak = plan_.longBreakEvery > 0 && (index_ + 1) % plan_.longBreakEvery == 0;
        state_ = longBreak ? PomodoroState::LongBreak : PomodoroState::ShortBreak;
        phaseEnd_ = at + (longBreak ? plan_.longBreak : plan_.shortBreak);
        events.push_back(PhaseStarted{state_, index_, at});
        if (pushupsOnBreak_) {
            events.push_back(PushupPrompt{index_});
        }
        return;
    }

    ++index_;
    state_ = PomodoroState::Focus;
    phaseEnd_ = at + plan_.focus;
    events.push_back(PhaseStarted{state_, index_, at});
}

} // namespace cadence::core
