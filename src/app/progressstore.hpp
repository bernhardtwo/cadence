#pragma once

#include <cadence/core/planner.hpp>
#include <cadence/core/time.hpp>

#include <QString>

#include <optional>

// Persistence of one DayProgress per calendar date. Files today, a database later.
class ProgressStore {
public:
    virtual ~ProgressStore() = default;

    // Empty when nothing was recorded for that date. A corrupt record also reads as empty and is
    // reported through lastError().
    virtual std::optional<cadence::core::DayProgress> load(cadence::core::Date date) = 0;
    virtual bool save(cadence::core::Date date, const cadence::core::DayProgress& progress) = 0;
    virtual QString lastError() const = 0;
};
