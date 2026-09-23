#pragma once

#include <cadence/core/activity.hpp>
#include <cadence/core/time.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace cadence::core {

enum class BlockKind {
    // Fixed start and end taken from the template. The start never moves.
    Anchored,
    // Has a duration and is placed in sequence after the previous non-soft block.
    Flexible,
    // Reminder only, with an earliest start. Never pushes other blocks.
    Soft,
};

struct PomodoroPlan {
    Minutes focus{25};
    Minutes shortBreak{5};
    Minutes longBreak{15};
    int longBreakEvery = 4;
    // Left empty to derive the count from the block duration.
    std::optional<int> count;

    friend bool operator==(const PomodoroPlan&, const PomodoroPlan&) = default;
};

struct BlockTemplate {
    ActivityId activityId;
    BlockKind kind = BlockKind::Flexible;
    // Start for anchored blocks, earliest start for soft blocks, unused for flexible blocks.
    std::optional<Minutes> start;
    std::optional<Minutes> durationMinutes;
    std::optional<PomodoroPlan> pomodoro;
    bool pushupsOnBreak = false;
    // Whether the start alarm takes over the screen or stays a sound and a notification.
    bool fullscreenAlarm = true;

    friend bool operator==(const BlockTemplate&, const BlockTemplate&) = default;
};

// Wall time of `count` sessions: every focus plus the breaks between them. The break after session
// k (1-based) is long when k is a multiple of longBreakEvery; there is no break after the last one.
Minutes pomodoroDuration(const PomodoroPlan& plan, int count) noexcept;

// The largest count whose pomodoroDuration fits in the duration.
int derivePomodoroCount(const PomodoroPlan& plan, Minutes duration) noexcept;

// The explicit duration, else the length of the pomodoro plan when it has an explicit count.
std::optional<Minutes> resolvedDuration(const BlockTemplate& block) noexcept;

// The explicit count, else the count derived from the duration. Empty when the block has no plan.
std::optional<int> resolvedPomodoroCount(const BlockTemplate& block) noexcept;

struct DayTemplate {
    Minutes dayStart{timeOfDay(8, 0)};
    Minutes dayCutoff{timeOfDay(23, 0)};
    std::vector<BlockTemplate> blocks;

    friend bool operator==(const DayTemplate&, const DayTemplate&) = default;
};

struct WeekTemplate {
    // Indexed Monday first. A day without a template is a free day.
    std::array<std::optional<DayTemplate>, 7> days;

    static constexpr std::size_t indexOf(Weekday weekday) noexcept { return weekday.iso_encoding() - 1; }

    const std::optional<DayTemplate>& day(Weekday weekday) const noexcept { return days[indexOf(weekday)]; }
    std::optional<DayTemplate>& day(Weekday weekday) noexcept { return days[indexOf(weekday)]; }
    bool isFreeDay(Weekday weekday) const noexcept { return !day(weekday).has_value(); }

    friend bool operator==(const WeekTemplate&, const WeekTemplate&) = default;
};

} // namespace cadence::core
