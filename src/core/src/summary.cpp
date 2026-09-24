#include <cadence/core/summary.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <string>

namespace cadence::core {

namespace {

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

} // namespace

std::optional<ActivityId> summaryActivity(const DayTemplate& day, const std::vector<Activity>& activities) {
    std::map<ActivityId, Minutes> totals;
    for (const BlockTemplate& block : day.blocks) {
        totals[block.activityId] += resolvedDuration(block).value_or(Minutes{0});
    }
    if (totals.empty()) {
        return std::nullopt;
    }
    for (const auto& [id, total] : totals) {
        const auto named = std::find_if(activities.begin(), activities.end(),
                                        [&id](const Activity& activity) { return activity.id == id; });
        const std::string name = named == activities.end() ? id : named->name;
        if (lower(name) == "work" || lower(id) == "work") {
            return id;
        }
    }
    const auto longest = std::max_element(totals.begin(), totals.end(),
                                          [](const auto& a, const auto& b) { return a.second < b.second; });
    return longest->first;
}

ActivitySummary summarizeActivity(const DayTemplate& day, const DayPlan& plan, const DayProgress& progress,
                                  const ActivityId& activity, Minutes now) {
    ActivitySummary summary;
    for (std::size_t i = 0; i < day.blocks.size(); ++i) {
        const BlockTemplate& block = day.blocks[i];
        if (block.activityId != activity) {
            continue;
        }
        summary.target += resolvedDuration(block).value_or(Minutes{0});
        if (i >= plan.blocks.size()) {
            continue;
        }
        const PlannedBlock& planned = plan.at(i);
        if (planned.state == BlockState::Done) {
            summary.done += std::max(Minutes{0}, planned.end - planned.start);
        } else if (planned.state == BlockState::Active) {
            Minutes elapsed = std::clamp(now, planned.start, planned.end) - planned.start;
            if (const BlockProgress* prog = progress.find(i)) {
                for (const PauseInterval& pause : prog->pauses) {
                    elapsed -= std::max(Minutes{0}, pause.end.value_or(now) - pause.start);
                }
            }
            summary.done += std::max(Minutes{0}, elapsed);
        }
    }
    return summary;
}

} // namespace cadence::core
