#include <cadence/core/block.hpp>

namespace cadence::core {

Minutes pomodoroDuration(const PomodoroPlan& plan, int count) noexcept {
    if (count <= 0) {
        return Minutes{0};
    }
    Minutes total = plan.focus * count;
    for (int session = 1; session < count; ++session) {
        const bool longBreak = plan.longBreakEvery > 0 && session % plan.longBreakEvery == 0;
        total += longBreak ? plan.longBreak : plan.shortBreak;
    }
    return total;
}

int derivePomodoroCount(const PomodoroPlan& plan, Minutes duration) noexcept {
    if (plan.focus <= Minutes{0} || duration <= Minutes{0}) {
        return 0;
    }
    int count = 0;
    while (pomodoroDuration(plan, count + 1) <= duration) {
        ++count;
    }
    return count;
}

std::optional<Minutes> resolvedDuration(const BlockTemplate& block) noexcept {
    if (block.durationMinutes) {
        return block.durationMinutes;
    }
    if (block.pomodoro && block.pomodoro->count) {
        return pomodoroDuration(*block.pomodoro, *block.pomodoro->count);
    }
    return std::nullopt;
}

std::optional<int> resolvedPomodoroCount(const BlockTemplate& block) noexcept {
    if (!block.pomodoro) {
        return std::nullopt;
    }
    if (block.pomodoro->count) {
        return block.pomodoro->count;
    }
    if (block.durationMinutes) {
        return derivePomodoroCount(*block.pomodoro, *block.durationMinutes);
    }
    return std::nullopt;
}

} // namespace cadence::core
