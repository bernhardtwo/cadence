#include <cadence/core/block.hpp>

namespace cadence::core {

int derivePomodoroCount(const PomodoroPlan& plan, Minutes duration) noexcept {
    const Minutes cycle = plan.focus + plan.shortBreak;
    if (cycle <= Minutes{0} || duration <= Minutes{0}) {
        return 0;
    }
    return static_cast<int>(duration / cycle);
}

std::optional<Minutes> resolvedDuration(const BlockTemplate& block) noexcept {
    if (block.durationMinutes) {
        return block.durationMinutes;
    }
    if (block.pomodoro && block.pomodoro->count) {
        return (block.pomodoro->focus + block.pomodoro->shortBreak) * *block.pomodoro->count;
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
