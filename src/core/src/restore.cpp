#include <cadence/core/restore.hpp>

#include <cadence/core/phase.hpp>

#include <algorithm>

namespace cadence::core {

namespace {

// Ends the pause still open, then forgets the run: the block is no longer in progress but keeps
// its phases and prompts.
void endRun(BlockProgress& progress, Minutes minute, Seconds second) noexcept {
    if (!progress.pauses.empty() && !progress.pauses.back().end) {
        progress.pauses.back().end = std::max(minute, progress.pauses.back().start);
    }
    closeOpenPhase(progress.phases, second);
    progress.actualStart.reset();
    progress.pauses.clear();
}

Seconds secondOfDay(Instant now) noexcept {
    return std::chrono::duration_cast<Seconds>(now - std::chrono::floor<std::chrono::days>(now));
}

} // namespace

void skipBlock(BlockProgress& progress, Minutes minute, Seconds second) noexcept {
    if (!progress.pauses.empty() && !progress.pauses.back().end) {
        progress.pauses.back().end = std::max(minute, progress.pauses.back().start);
    }
    closeOpenPhase(progress.phases, second);
    progress.skipped = true;
    progress.skips.push_back(SkipInterval{minute, std::nullopt});
}

std::optional<RestoreRefusal> restoreRefusal(const DayTemplate& day, const DayProgress& progress,
                                             std::size_t index, TimePoint now) {
    if (index >= day.blocks.size()) {
        return RestoreRefusal::NotSkipped;
    }
    const BlockProgress* prog = progress.find(index);
    if (prog == nullptr) {
        return RestoreRefusal::NotSkipped;
    }
    if (!prog->skipped) {
        return prog->confirmed == false ? std::optional(RestoreRefusal::WindowClosed)
                                        : std::optional(RestoreRefusal::NotSkipped);
    }
    const Minutes minute = now.minuteOfDay;
    if (minute >= day.dayCutoff) {
        return RestoreRefusal::WindowClosed;
    }
    const BlockTemplate& block = day.blocks[index];
    if (block.kind == BlockKind::Anchored) {
        const Minutes start = block.start.value_or(day.dayStart);
        if (minute >= nominalEnd(start, block, *prog)) {
            return RestoreRefusal::WindowClosed;
        }
    }
    return std::nullopt;
}

std::optional<RestoreResult> restoreBlock(const DayTemplate& day, DayProgress& progress, std::size_t index,
                                          Instant now) {
    const TimePoint point = toTimePoint(now);
    if (restoreRefusal(day, progress, index, point)) {
        return std::nullopt;
    }
    const Minutes minute = point.minuteOfDay;
    const Seconds second = secondOfDay(now);
    const BlockTemplate& block = day.blocks[index];
    BlockProgress& prog = progress.at(index);

    prog.skipped = false;
    if (!prog.skips.empty() && !prog.skips.back().restoredAt) {
        prog.skips.back().restoredAt = minute;
    }

    RestoreResult result;
    if (block.kind != BlockKind::Anchored) {
        // Back in the queue as if never started; a run before the skip lives on in the phases.
        endRun(prog, minute, second);
        prog.postponed = false;
        prog.pomodoroCount.reset();
        return result;
    }

    const Minutes start = block.start.value_or(day.dayStart);
    if (minute < start) {
        return result;
    }
    // The window is open again, so a flexible block that reflowed into it gives it back.
    for (auto& [other, otherProgress] : progress.blocks) {
        if (other == index || day.blocks.size() <= other || day.blocks[other].kind != BlockKind::Flexible) {
            continue;
        }
        if (otherProgress.actualStart && !otherProgress.actualEnd && !otherProgress.skipped) {
            endRun(otherProgress, minute, second);
            result.displaced = other;
        }
    }
    if (!prog.actualStart) {
        prog.actualStart = minute;
    }
    prog.pomodoroCount.reset();
    if (block.pomodoro) {
        const int count = derivePomodoroCount(*block.pomodoro, nominalEnd(start, block, prog) - minute);
        if (count > 0) {
            prog.pomodoroCount = count;
            result.pomodoroCount = count;
        }
    }
    return result;
}

} // namespace cadence::core
