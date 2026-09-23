#pragma once

#include <cadence/core/planner.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace cadence::core {

inline constexpr int progressSchemaVersion = 1;

class ProgressError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::string serializeDayProgress(const DayProgress& progress);

// Throws ProgressError when the text is not JSON or does not match the schema.
DayProgress parseDayProgress(std::string_view json);

} // namespace cadence::core
