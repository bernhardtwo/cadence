#pragma once

#include <cadence/core/block.hpp>
#include <cadence/core/phase.hpp>
#include <cadence/core/time.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <vector>

namespace cadence::core {

enum class BlockState {
    Done,
    Active,
    Upcoming,
    Skipped,
    DoesNotFit,
    // An anchored block whose time has passed without the user touching it. The app asks whether it
    // happened; until then it counts toward nothing.
    Unconfirmed,
};

struct PauseInterval {
    Minutes start;
    // Empty while the pause is still running.
    std::optional<Minutes> end;

    friend bool operator==(const PauseInterval&, const PauseInterval&) = default;
};

struct SkipInterval {
    Minutes at;
    // Empty while the block is still skipped.
    std::optional<Minutes> restoredAt;

    friend bool operator==(const SkipInterval&, const SkipInterval&) = default;
};

struct BlockProgress {
    std::optional<Minutes> actualStart;
    std::optional<Minutes> actualEnd;
    std::vector<PauseInterval> pauses;
    bool skipped = false;
    bool postponed = false;
    Minutes extended{0};
    // Resolves an Unconfirmed block: true means it happened (Done), false means it did not (Skipped).
    std::optional<bool> confirmed;
    // The pomodoro session of the block, phase by phase, and the push-up prompts it showed.
    std::vector<PhaseRecord> phases;
    std::vector<PromptRecord> prompts;
    // Every skip of the block and, once restored, when. `skipped` says whether the last one is open.
    std::vector<SkipInterval> skips;
    // The count a restore started the pomodoro session with, so a restart rebuilds the same plan.
    std::optional<int> pomodoroCount;

    friend bool operator==(const BlockProgress&, const BlockProgress&) = default;
};

// Minutes of [start, end) that fell between a skip and its restore. Time skipped is never worked.
Minutes skippedWithin(const BlockProgress& progress, Minutes start, Minutes end) noexcept;

// Where a block that begins at `start` ends by its own length: duration, extensions and finished
// pauses. Unlike the plan, it never stretches to the clock.
Minutes nominalEnd(Minutes start, const BlockTemplate& block, const BlockProgress& progress) noexcept;

struct PushupSet {
    std::size_t blockIndex;
    Minutes at;
    int reps;

    friend bool operator==(const PushupSet&, const PushupSet&) = default;
};

struct DayProgress {
    // When the user starts the day later than the template's dayStart.
    std::optional<Minutes> actualDayStart;
    // Keyed by index into DayTemplate::blocks. Blocks without an entry have no progress.
    std::map<std::size_t, BlockProgress> blocks;
    std::vector<PushupSet> pushups;

    BlockProgress& at(std::size_t templateIndex) { return blocks[templateIndex]; }
    const BlockProgress* find(std::size_t templateIndex) const noexcept;

    friend bool operator==(const DayProgress&, const DayProgress&) = default;
};

struct PlannedBlock {
    std::size_t templateIndex;
    Minutes start;
    Minutes end;
    BlockState state;
    // An Active block that will end after the day cutoff. It keeps its state; the app warns instead.
    bool overrunsCutoff = false;

    friend bool operator==(const PlannedBlock&, const PlannedBlock&) = default;
};

struct DayPlan {
    // Same order as DayTemplate::blocks.
    std::vector<PlannedBlock> blocks;

    bool doesNotFit() const noexcept;
    // Template indices of blocks waiting for the user to say whether they happened.
    std::vector<std::size_t> unconfirmedBlocks() const;
    // Template indices of Active blocks that will end after the cutoff. Independent of doesNotFit().
    std::vector<std::size_t> overrunsCutoff() const;
    // Sum of the planned length of Done blocks less the time they spent skipped, the basis for any
    // statistics.
    Minutes completedMinutes(const DayProgress& progress) const noexcept;
    const PlannedBlock& at(std::size_t templateIndex) const { return blocks.at(templateIndex); }
};

DayPlan plan(const DayTemplate& day, const DayProgress& progress, TimePoint now);

} // namespace cadence::core
