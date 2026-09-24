#include <cadence/core/alarm.hpp>

#include <algorithm>
#include <utility>

namespace cadence::core {

namespace {

bool isTimeline(AlarmKind kind) noexcept {
    return kind != AlarmKind::DayNoLongerFits && kind != AlarmKind::UnconfirmedPending;
}

} // namespace

AlarmScheduler::AlarmScheduler(AlarmPolicy policy) noexcept : policy_(policy) {}

std::vector<Alarm> AlarmScheduler::evaluate(const DayTemplate& day, const DayPlan& plan, const DayProgress& progress,
                                            const PomodoroEvents& pomodoroEvents, Instant now) {
    const Instant last = last_.value_or(now);
    const bool collapse = now - last > policy_.collapseAfter;
    const auto date = toTimePoint(now).date;
    const auto instantOf = [date](Minutes minuteOfDay) { return std::chrono::local_days{date} + minuteOfDay; };
    const auto due = [&](Instant at) { return at > last && at <= now; };

    kinds_.clear();
    for (const BlockTemplate& block : day.blocks) {
        kinds_.push_back(block.kind);
    }

    std::vector<Alarm> timeline;
    std::vector<Alarm> state;

    for (const PlannedBlock& planned : plan.blocks) {
        const std::size_t i = planned.templateIndex;
        if (i >= day.blocks.size()) {
            continue;
        }
        const BlockKind kind = day.blocks[i].kind;
        const BlockProgress* prog = progress.find(i);
        const bool started = prog && prog->actualStart.has_value();

        if (kind == BlockKind::Soft) {
            if (planned.state == BlockState::Upcoming && !started && !fired_.contains({AlarmKind::SoftReminder, i}) &&
                due(instantOf(planned.start))) {
                timeline.push_back(Alarm{AlarmKind::SoftReminder, i, instantOf(planned.start)});
            }
            continue;
        }

        const bool pending = planned.state == BlockState::Upcoming || planned.state == BlockState::Active;
        if (started) {
            snoozeUntil_.erase(i);
        } else if (pending) {
            if (!fired_.contains({AlarmKind::BlockStart, i}) && due(instantOf(planned.start))) {
                timeline.push_back(Alarm{AlarmKind::BlockStart, i, instantOf(planned.start)});
            }
            if (const auto snoozed = snoozeUntil_.find(i); snoozed != snoozeUntil_.end() && due(snoozed->second)) {
                timeline.push_back(Alarm{AlarmKind::BlockStart, i, snoozed->second});
                snoozeUntil_.erase(snoozed);
            }
        }

        if (planned.state == BlockState::Active && !fired_.contains({AlarmKind::BlockEndingSoon, i})) {
            const Instant at = instantOf(planned.end) - policy_.endingSoonLead;
            if (at > instantOf(planned.start) && due(at)) {
                timeline.push_back(Alarm{AlarmKind::BlockEndingSoon, i, at});
            }
        }
    }

    for (const PomodoroEvent& event : pomodoroEvents) {
        if (const auto* phase = std::get_if<PhaseStarted>(&event)) {
            if (phase->phase == PomodoroState::ShortBreak || phase->phase == PomodoroState::LongBreak) {
                timeline.push_back(Alarm{AlarmKind::PomodoroFocusEnd, std::nullopt, now});
            } else if (phase->phase == PomodoroState::Focus && phase->index > 0) {
                timeline.push_back(Alarm{AlarmKind::BreakEnd, std::nullopt, now});
            }
        } else if (std::holds_alternative<SessionCompleted>(event)) {
            timeline.push_back(Alarm{AlarmKind::PomodoroFocusEnd, std::nullopt, now});
        }
    }

    // Everything that was due counts as fired, whether or not it survives the collapse.
    for (const Alarm& alarm : timeline) {
        if (alarm.blockIndex && isTimeline(alarm.kind)) {
            fired_.insert({alarm.kind, *alarm.blockIndex});
        }
    }
    if (collapse && timeline.size() > 1) {
        // Ties go to the later entry, so a pomodoro alarm at now wins over a block alarm at now.
        Alarm kept = timeline.front();
        for (const Alarm& alarm : timeline) {
            if (alarm.at >= kept.at) {
                kept = alarm;
            }
        }
        timeline.clear();
        timeline.push_back(kept);
    }

    const bool doesNotFit = plan.doesNotFit();
    if (doesNotFit && !didNotFit_) {
        state.push_back(Alarm{AlarmKind::DayNoLongerFits, std::nullopt, now});
    }
    didNotFit_ = doesNotFit;

    for (const std::size_t i : plan.unconfirmedBlocks()) {
        if (fired_.insert({AlarmKind::UnconfirmedPending, i}).second) {
            state.push_back(Alarm{AlarmKind::UnconfirmedPending, i, now});
        }
    }

    last_ = now;
    timeline.insert(timeline.end(), state.begin(), state.end());
    return timeline;
}

bool AlarmScheduler::snooze(std::size_t blockIndex, Instant now) {
    if (!canSnooze(blockIndex)) {
        return false;
    }
    snoozes_[blockIndex] += 1;
    snoozeUntil_[blockIndex] = now + policy_.snoozeLength;
    return true;
}

bool AlarmScheduler::canSnooze(std::size_t blockIndex) const noexcept {
    if (blockIndex >= kinds_.size() || kinds_[blockIndex] == BlockKind::Soft) {
        return false;
    }
    return snoozesUsed(blockIndex) < policy_.maxSnoozes;
}

int AlarmScheduler::snoozesUsed(std::size_t blockIndex) const noexcept {
    const auto it = snoozes_.find(blockIndex);
    return it == snoozes_.end() ? 0 : it->second;
}

void AlarmScheduler::dismiss(std::size_t blockIndex) {
    snoozeUntil_.erase(blockIndex);
}

void AlarmScheduler::rearm(std::size_t blockIndex) {
    std::erase_if(fired_, [blockIndex](const Key& key) { return key.block == blockIndex; });
    snoozes_.erase(blockIndex);
    snoozeUntil_.erase(blockIndex);
}

void AlarmScheduler::reset() {
    last_.reset();
    fired_.clear();
    snoozes_.clear();
    snoozeUntil_.clear();
    didNotFit_ = false;
}

} // namespace cadence::core
