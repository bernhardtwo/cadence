#pragma once

#include <cadence/core/block.hpp>
#include <cadence/core/planner.hpp>
#include <cadence/core/pomodoro.hpp>
#include <cadence/core/time.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace cadence::core {

enum class AlarmKind {
    BlockStart,
    BlockEndingSoon,
    // The single reminder of a soft block. It cannot be snoozed.
    SoftReminder,
    PomodoroFocusEnd,
    BreakEnd,
    DayNoLongerFits,
    UnconfirmedPending,
};

struct Alarm {
    AlarmKind kind;
    std::optional<std::size_t> blockIndex;
    Instant at;

    friend bool operator==(const Alarm&, const Alarm&) = default;
};

struct AlarmPolicy {
    Minutes endingSoonLead{5};
    Minutes snoozeLength{5};
    int maxSnoozes = 2;
    // A gap between evaluations longer than this is treated as a wake from sleep: the alarms that
    // fell into the gap collapse into the most recent one.
    Seconds collapseAfter{120};
};

// Turns the plan and the pomodoro machine into due alarms. Everything is decided by comparing wall
// clock instants with the previous evaluation, so a stalled timer or a sleeping machine cannot make
// it drift; it only makes the next evaluation catch up.
class AlarmScheduler {
public:
    explicit AlarmScheduler(AlarmPolicy policy = {}) noexcept;

    std::vector<Alarm> evaluate(const DayTemplate& day, const DayPlan& plan, const DayProgress& progress,
                                const PomodoroEvents& pomodoroEvents, Instant now);

    // Re-fires BlockStart after the snooze length. False for soft blocks and once the limit is reached.
    bool snooze(std::size_t blockIndex, Instant now);
    bool canSnooze(std::size_t blockIndex) const noexcept;
    int snoozesUsed(std::size_t blockIndex) const noexcept;
    // Cancels a pending snooze, for when the user acted on the block another way.
    void dismiss(std::size_t blockIndex);
    // Forgets everything fired for one block, so a restored block alarms again as if it had never
    // been skipped. Instants already past still cannot fire: only what falls due after the last
    // evaluation does.
    void rearm(std::size_t blockIndex);

    const AlarmPolicy& policy() const noexcept { return policy_; }
    // Takes effect on the next evaluation; snoozes already used keep counting against the new limit.
    void setPolicy(AlarmPolicy policy) noexcept { policy_ = policy; }
    std::optional<Instant> lastEvaluation() const noexcept { return last_; }

    // Forgets everything fired so far; used when the day changes.
    void reset();

private:
    struct Key {
        AlarmKind kind;
        std::size_t block;

        friend auto operator<=>(const Key&, const Key&) = default;
    };

    AlarmPolicy policy_;
    std::optional<Instant> last_;
    std::set<Key> fired_;
    std::map<std::size_t, int> snoozes_;
    std::map<std::size_t, Instant> snoozeUntil_;
    std::vector<BlockKind> kinds_;
    bool didNotFit_ = false;
};

} // namespace cadence::core
