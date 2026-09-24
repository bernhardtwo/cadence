#pragma once

#include <map>
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

// A playlist the user picked for an activity; the name is only for display.
struct PlaylistChoice {
    std::string uri;
    std::string name;

    friend bool operator==(const PlaylistChoice&, const PlaylistChoice&) = default;
};

struct AppSettings {
    bool startMinimized = false;
    SoundMode sound = SoundMode::Default;
    int maxSnoozes = 2;
    bool warnDayNoLongerFits = true;
    int defaultReps = 10;
    // UI language: "system" follows the OS, otherwise one of the shipped codes (en, es, fr).
    std::string language = "system";
    bool showQuotes = true;
    // The Latin or Greek line under a quote, where the surface has room for it.
    bool showQuoteOriginals = false;
    // The user's own Spotify app. It is an identifier, not a secret; tokens never come here.
    std::string spotifyClientId;
    // Start the activity's playlist when its block starts.
    bool spotifyAutoplay = false;
    // Keyed by activity id.
    std::map<std::string, PlaylistChoice> activityPlaylists;

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
