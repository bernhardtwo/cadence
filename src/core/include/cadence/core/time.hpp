#pragma once

#include <chrono>
#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace cadence::core {

using Minutes = std::chrono::minutes;
using Seconds = std::chrono::seconds;
using Date = std::chrono::year_month_day;
using Weekday = std::chrono::weekday;

inline constexpr Minutes minutesPerDay{24 * 60};

// Minutes elapsed since local midnight. Valid values lie in [0, minutesPerDay).
constexpr Minutes timeOfDay(int hours, int minutes) noexcept {
    return Minutes{hours * 60 + minutes};
}

constexpr bool isValidTimeOfDay(Minutes value) noexcept {
    return value >= Minutes{0} && value < minutesPerDay;
}

// Accepts exactly "HH:MM" with a 24 hour clock.
std::optional<Minutes> parseTimeOfDay(std::string_view text);

std::string formatTimeOfDay(Minutes value);

struct TimePoint {
    Date date;
    Minutes minuteOfDay;

    friend constexpr auto operator<=>(const TimePoint&, const TimePoint&) = default;
};

// Wall clock seconds in local time. Core never converts between time zones; the caller
// that owns the real clock does.
using Instant = std::chrono::local_seconds;

constexpr Instant toInstant(const TimePoint& point) noexcept {
    return std::chrono::local_days{point.date} + point.minuteOfDay;
}

constexpr TimePoint toTimePoint(Instant instant) noexcept {
    const auto day = std::chrono::floor<std::chrono::days>(instant);
    return TimePoint{Date{day}, std::chrono::floor<Minutes>(instant - day)};
}

} // namespace cadence::core
