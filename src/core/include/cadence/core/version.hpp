#pragma once

#include <string_view>

namespace cadence::core {

struct Version {
    int major;
    int minor;
    int patch;

    friend constexpr bool operator==(const Version&, const Version&) = default;
};

Version version() noexcept;

std::string_view versionString() noexcept;

} // namespace cadence::core
