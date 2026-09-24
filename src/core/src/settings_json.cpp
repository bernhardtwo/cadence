#include <cadence/core/settings_json.hpp>

#include <nlohmann/json.hpp>
#include <string>

namespace cadence::core {

namespace {

using Json = nlohmann::ordered_json;

[[noreturn]] void fail(const std::string& key, const std::string& message) {
    throw SettingsError(key + ": " + message);
}

const char* soundName(SoundMode mode) noexcept {
    switch (mode) {
    case SoundMode::Default:
        return "default";
    case SoundMode::Soft:
        return "soft";
    case SoundMode::Silent:
        return "silent";
    }
    return "default";
}

SoundMode parseSound(const std::string& text) {
    if (text == "default") {
        return SoundMode::Default;
    }
    if (text == "soft") {
        return SoundMode::Soft;
    }
    if (text == "silent") {
        return SoundMode::Silent;
    }
    fail("sound", "unknown sound mode \"" + text + "\", expected default, soft or silent");
}

bool readBool(const Json& root, const char* key, bool fallback) {
    const auto it = root.find(key);
    if (it == root.end() || it->is_null()) {
        return fallback;
    }
    if (!it->is_boolean()) {
        fail(key, "expected true or false");
    }
    return it->get<bool>();
}

int readInt(const Json& root, const char* key, int fallback, int minimum, int maximum) {
    const auto it = root.find(key);
    if (it == root.end() || it->is_null()) {
        return fallback;
    }
    if (!it->is_number_integer()) {
        fail(key, "expected an integer");
    }
    const int value = it->get<int>();
    if (value < minimum || value > maximum) {
        fail(key, "must lie between " + std::to_string(minimum) + " and " + std::to_string(maximum));
    }
    return value;
}

} // namespace

std::string serializeAppSettings(const AppSettings& settings) {
    Json root;
    root["version"] = settingsSchemaVersion;
    root["startMinimized"] = settings.startMinimized;
    root["sound"] = soundName(settings.sound);
    root["maxSnoozes"] = settings.maxSnoozes;
    root["warnDayNoLongerFits"] = settings.warnDayNoLongerFits;
    root["defaultReps"] = settings.defaultReps;
    Json spotify;
    spotify["clientId"] = settings.spotifyClientId;
    spotify["autoplay"] = settings.spotifyAutoplay;
    Json playlists = Json::object();
    for (const auto& [activity, choice] : settings.activityPlaylists) {
        Json item;
        item["uri"] = choice.uri;
        item["name"] = choice.name;
        playlists[activity] = std::move(item);
    }
    spotify["playlists"] = std::move(playlists);
    root["spotify"] = std::move(spotify);
    return root.dump(2) + "\n";
}

AppSettings parseAppSettings(std::string_view json) {
    Json root;
    try {
        root = Json::parse(json);
    } catch (const Json::parse_error& error) {
        throw SettingsError(std::string("settings are not valid JSON: ") + error.what());
    }
    if (!root.is_object()) {
        fail("$", "expected an object");
    }
    const int version = readInt(root, "version", settingsSchemaVersion, 1, settingsSchemaVersion);
    if (version != settingsSchemaVersion) {
        fail("version", "unsupported settings version " + std::to_string(version));
    }

    AppSettings settings;
    settings.startMinimized = readBool(root, "startMinimized", settings.startMinimized);
    if (const auto it = root.find("sound"); it != root.end() && !it->is_null()) {
        if (!it->is_string()) {
            fail("sound", "expected a string");
        }
        settings.sound = parseSound(it->get<std::string>());
    }
    settings.maxSnoozes = readInt(root, "maxSnoozes", settings.maxSnoozes, 0, 20);
    settings.warnDayNoLongerFits = readBool(root, "warnDayNoLongerFits", settings.warnDayNoLongerFits);
    settings.defaultReps = readInt(root, "defaultReps", settings.defaultReps, 1, 500);
    if (const auto spotify = root.find("spotify"); spotify != root.end() && !spotify->is_null()) {
        if (!spotify->is_object()) {
            fail("spotify", "expected an object");
        }
        if (const auto id = spotify->find("clientId"); id != spotify->end() && !id->is_null()) {
            if (!id->is_string()) {
                fail("spotify.clientId", "expected a string");
            }
            settings.spotifyClientId = id->get<std::string>();
        }
        settings.spotifyAutoplay = readBool(*spotify, "autoplay", settings.spotifyAutoplay);
        if (const auto lists = spotify->find("playlists"); lists != spotify->end() && !lists->is_null()) {
            if (!lists->is_object()) {
                fail("spotify.playlists", "expected an object keyed by activity");
            }
            for (const auto& [activity, value] : lists->items()) {
                if (!value.is_object()) {
                    fail("spotify.playlists." + activity, "expected an object");
                }
                PlaylistChoice choice;
                if (const auto uri = value.find("uri"); uri != value.end() && uri->is_string()) {
                    choice.uri = uri->get<std::string>();
                }
                if (const auto name = value.find("name"); name != value.end() && name->is_string()) {
                    choice.name = name->get<std::string>();
                }
                if (!choice.uri.empty()) {
                    settings.activityPlaylists[activity] = choice;
                }
            }
        }
    }
    return settings;
}

} // namespace cadence::core
