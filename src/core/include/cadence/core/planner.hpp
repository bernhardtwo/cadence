#pragma once

#include <cadence/core/block.hpp>
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

struct BlockProgress {
    std::optional<Minutes> actualStart;
    std::optional<Minutes> actualEnd;
    std::vector<PauseInterval> pauses;
    bool skipped = false;
    bool postponed = false;
    Minutes extended{0};
    // Resolves an Unconfirmed block: true means it happened (Done), false means it did not (Skipped).
    std::optional<bool> confirmed;

    friend bool operator==(const BlockProgress&, const BlockProgress&) = default;
};

struct DayProgress {
    // When the user starts the day later than the template's dayStart.
    std::optional<Minutes> actualDayStart;
    // Keyed by index into DayTemplate::blocks. Blocks without an entry have no progress.
    std::map<std::size_t, BlockProgress> blocks;

    BlockProgress& at(std::size_t templateIndex) { return blocks[templateIndex]; }
    const BlockProgress* find(std::size_t templateIndex) const noexcept;
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
    // Sum of the planned length of Done blocks, the basis for any statistics.
    Minutes completedMinutes() const noexcept;
    const PlannedBlock& at(std::size_t templateIndex) const { return blocks.at(templateIndex); }
};

DayPlan plan(const DayTemplate& day, const DayProgress& progress, TimePoint now);

} // namespace cadence::core
