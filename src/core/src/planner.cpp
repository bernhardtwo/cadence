#include <cadence/core/planner.hpp>

#include <algorithm>

namespace cadence::core {

namespace {

struct Interval {
    Minutes start;
    Minutes end;
};

const BlockProgress& progressFor(const DayProgress& progress, std::size_t index) noexcept {
    static const BlockProgress none{};
    const BlockProgress* found = progress.find(index);
    return found ? *found : none;
}

// A block the user skipped, or reported as not having happened, is not part of the chain.
bool leavesChain(const BlockProgress& progress) noexcept {
    return progress.skipped || progress.confirmed == false;
}

Minutes pausedTotal(const BlockProgress& progress, Minutes now) noexcept {
    Minutes total{0};
    for (const PauseInterval& pause : progress.pauses) {
        const Minutes end = pause.end.value_or(std::max(now, pause.start));
        total += std::max(Minutes{0}, end - pause.start);
    }
    return total;
}

// A finished block ends where it actually ended. A running block ends no earlier than now, so the
// blocks after it reflow while it runs late. Pauses and extensions grow the nominal length.
Minutes endOf(Minutes start, const BlockTemplate& block, const BlockProgress& progress, Minutes now) noexcept {
    if (progress.actualEnd) {
        return *progress.actualEnd;
    }
    Minutes end = start + resolvedDuration(block).value_or(Minutes{0}) + progress.extended + pausedTotal(progress, now);
    if (progress.actualStart) {
        end = std::max(end, now);
    }
    return end;
}

BlockState stateOf(const BlockTemplate& block, const BlockProgress& progress, const Interval& placed, Minutes now,
                   Minutes cutoff) noexcept {
    if (leavesChain(progress)) {
        return BlockState::Skipped;
    }
    if (progress.actualEnd) {
        return BlockState::Done;
    }
    if (progress.actualStart) {
        return BlockState::Active;
    }
    if (progress.confirmed == true) {
        return BlockState::Done;
    }
    // Anchored blocks follow the clock; the other kinds only become active when the user starts them.
    if (block.kind == BlockKind::Anchored) {
        if (placed.end <= now) {
            return BlockState::Unconfirmed;
        }
        if (placed.start <= now) {
            return BlockState::Active;
        }
    }
    return placed.end > cutoff ? BlockState::DoesNotFit : BlockState::Upcoming;
}

} // namespace

const BlockProgress* DayProgress::find(std::size_t templateIndex) const noexcept {
    const auto it = blocks.find(templateIndex);
    return it == blocks.end() ? nullptr : &it->second;
}

bool DayPlan::doesNotFit() const noexcept {
    return std::any_of(blocks.begin(), blocks.end(),
                       [](const PlannedBlock& block) { return block.state == BlockState::DoesNotFit; });
}

std::vector<std::size_t> DayPlan::unconfirmedBlocks() const {
    std::vector<std::size_t> indices;
    for (const PlannedBlock& block : blocks) {
        if (block.state == BlockState::Unconfirmed) {
            indices.push_back(block.templateIndex);
        }
    }
    return indices;
}

std::vector<std::size_t> DayPlan::overrunsCutoff() const {
    std::vector<std::size_t> indices;
    for (const PlannedBlock& block : blocks) {
        if (block.overrunsCutoff) {
            indices.push_back(block.templateIndex);
        }
    }
    return indices;
}

Minutes DayPlan::completedMinutes() const noexcept {
    Minutes total{0};
    for (const PlannedBlock& block : blocks) {
        if (block.state == BlockState::Done) {
            total += std::max(Minutes{0}, block.end - block.start);
        }
    }
    return total;
}

DayPlan plan(const DayTemplate& day, const DayProgress& progress, TimePoint nowPoint) {
    const Minutes now = nowPoint.minuteOfDay;
    const std::size_t count = day.blocks.size();

    std::vector<Interval> placed(count, Interval{day.dayStart, day.dayStart});
    std::vector<Interval> anchored;

    for (std::size_t i = 0; i < count; ++i) {
        const BlockTemplate& block = day.blocks[i];
        const BlockProgress& prog = progressFor(progress, i);
        if (block.kind != BlockKind::Anchored || leavesChain(prog)) {
            continue;
        }
        const Minutes start = block.start.value_or(day.dayStart);
        placed[i] = Interval{start, endOf(start, block, prog, now)};
        anchored.push_back(placed[i]);
    }
    std::sort(anchored.begin(), anchored.end(),
              [](const Interval& a, const Interval& b) { return a.start < b.start; });

    // The chain is every non-soft block in template order, with postponed blocks moved to the end.
    std::vector<std::size_t> chain;
    std::vector<std::size_t> postponed;
    for (std::size_t i = 0; i < count; ++i) {
        const BlockTemplate& block = day.blocks[i];
        const BlockProgress& prog = progressFor(progress, i);
        if (block.kind == BlockKind::Soft || leavesChain(prog)) {
            continue;
        }
        if (block.kind == BlockKind::Flexible && prog.postponed) {
            postponed.push_back(i);
        } else {
            chain.push_back(i);
        }
    }
    chain.insert(chain.end(), postponed.begin(), postponed.end());

    Minutes cursor = std::max(day.dayStart, progress.actualDayStart.value_or(day.dayStart));

    for (const std::size_t i : chain) {
        const BlockTemplate& block = day.blocks[i];
        const BlockProgress& prog = progressFor(progress, i);
        if (block.kind == BlockKind::Anchored) {
            cursor = std::max(cursor, placed[i].end);
            continue;
        }

        // An unstarted block cannot begin in the past, so the chain reflows around now.
        Minutes start = prog.actualStart ? *prog.actualStart : (prog.actualEnd ? cursor : std::max(cursor, now));
        Minutes end = endOf(start, block, prog, now);

        if (!prog.actualStart && !prog.actualEnd) {
            bool moved = true;
            while (moved) {
                moved = false;
                for (const Interval& fixed : anchored) {
                    if (start < fixed.end && end > fixed.start) {
                        start = fixed.end;
                        end = endOf(start, block, prog, now);
                        moved = true;
                    }
                }
            }
        }

        placed[i] = Interval{start, end};
        cursor = std::max(cursor, end);
    }

    for (std::size_t i = 0; i < count; ++i) {
        const BlockTemplate& block = day.blocks[i];
        const BlockProgress& prog = progressFor(progress, i);
        if (leavesChain(prog)) {
            const Minutes at = block.start.value_or(cursor);
            placed[i] = Interval{at, at};
            continue;
        }
        if (block.kind != BlockKind::Soft) {
            continue;
        }
        const Minutes earliest = block.start.value_or(day.dayStart);
        const Minutes start = prog.actualStart ? *prog.actualStart : std::max({earliest, cursor, now});
        placed[i] = Interval{start, endOf(start, block, prog, now)};
    }

    DayPlan result;
    result.blocks.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const BlockProgress& prog = progressFor(progress, i);
        const BlockState state = stateOf(day.blocks[i], prog, placed[i], now, day.dayCutoff);
        const bool overruns = state == BlockState::Active && placed[i].end > day.dayCutoff;
        result.blocks.push_back(PlannedBlock{i, placed[i].start, placed[i].end, state, overruns});
    }
    return result;
}

} // namespace cadence::core
