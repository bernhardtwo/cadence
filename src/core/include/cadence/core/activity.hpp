#pragma once

#include <string>

namespace cadence::core {

using ActivityId = std::string;

struct Activity {
    ActivityId id;
    std::string name;
    // "#RRGGBB"; core stays free of any UI color type.
    std::string color;

    friend bool operator==(const Activity&, const Activity&) = default;
};

} // namespace cadence::core
