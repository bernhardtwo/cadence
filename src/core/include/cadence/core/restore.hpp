#pragma once

#include <cadence/core/block.hpp>
#include <cadence/core/planner.hpp>
#include <cadence/core/time.hpp>

#include <cstddef>
#include <optional>

namespace cadence::core {

// Skipping a block by hand and taking the skip back. Both only touch the day's progress; the plan,
// the alarms and the pomodoro session re-derive from it.

// Records the skip at `minute`, closing the pause and the pomodoro phase still open at `second` so
// no record is left open. Phase records stay: the time worked before the skip is never lost.
void skipBlock(BlockProgress& progress, Minutes minute, Seconds second) noexcept;

enum class RestoreRefusal {
    NotSkipped,
    // The block's time is over, or the day is: an anchored block past its end, any block past the
    // day cutoff, and an anchored block the user said did not happen.
    WindowClosed,
};

// Why the block cannot be restored at `now`, or empty when it can.
std::optional<RestoreRefusal> restoreRefusal(const DayTemplate& day, const DayProgress& progress,
                                             std::size_t index, TimePoint now);

struct RestoreResult {
    // A flexible block that was running inside the restored anchored block's window and went back
    // to Upcoming to give it up. Its phase records stay.
    std::optional<std::size_t> displaced;
    // The pomodoro session to start now for a restored anchored block: the sessions that fit the
    // time left, long breaks included. Empty when the block has no plan or nothing fits.
    std::optional<int> pomodoroCount;
};

// Takes the skip back. An anchored block continues at the clock with the time left until its end;
// a flexible block returns to the queue in template order; a soft block waits for its reminder
// again. Empty, with nothing changed, when restoreRefusal is not empty.
std::optional<RestoreResult> restoreBlock(const DayTemplate& day, DayProgress& progress, std::size_t index,
                                          Instant now);

} // namespace cadence::core
