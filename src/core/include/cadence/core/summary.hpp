#pragma once

#include <cadence/core/activity.hpp>
#include <cadence/core/block.hpp>
#include <cadence/core/planner.hpp>
#include <cadence/core/time.hpp>

#include <optional>
#include <vector>

namespace cadence::core {

struct ActivitySummary {
    // Finished blocks by their planned length, the running one by its elapsed time less pauses.
    Minutes done{0};
    // Template length of every block of the activity. Finishing early or late never moves it.
    Minutes target{0};

    friend bool operator==(const ActivitySummary&, const ActivitySummary&) = default;
};

// The activity the day summary follows: the one called "work" when the template has it, else the
// one with the most template time today. Empty on a day without blocks.
std::optional<ActivityId> summaryActivity(const DayTemplate& day, const std::vector<Activity>& activities);

ActivitySummary summarizeActivity(const DayTemplate& day, const DayPlan& plan, const DayProgress& progress,
                                  const ActivityId& activity, Minutes now);

} // namespace cadence::core
