#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace cadence::core {

inline constexpr int settingsSchemaVersion = 1;

enum class SoundMode {
    // A chime for the moments that need the user now, a soft note for the rest.
    Default,
    // The soft note for everything.
    Soft,
    Silent,
};

struct AppSettings {
    bool startMinimized = false;
    SoundMode sound = SoundMode::Default;
    int maxSnoozes = 2;
    bool warnDayNoLongerFits = true;
    int defaultReps = 10;

    friend bool operator==(const AppSettings&, const AppSettings&) = default;
};

class SettingsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::string serializeAppSettings(const AppSettings& settings);

// Missing keys keep their defaults so a file written by an older build still loads. Throws
// SettingsError when the text is not JSON or a present key has the wrong type or range.
AppSettings parseAppSettings(std::string_view json);

} // namespace cadence::core
