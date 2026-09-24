#include <cadence/spotify/api.hpp>

#include <algorithm>
#include <charconv>
#include <nlohmann/json.hpp>
#include <sstream>

namespace cadence::spotify {

namespace {

using Json = nlohmann::json;

Json parse(std::string_view json) {
    try {
        return Json::parse(json);
    } catch (const Json::parse_error& error) {
        throw ParseError(std::string("response is not valid JSON: ") + error.what());
    }
}

std::string stringOr(const Json& object, const char* key, std::string fallback = {}) {
    if (!object.is_object()) {
        return fallback;
    }
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        return fallback;
    }
    return it->get<std::string>();
}

int intOr(const Json& object, const char* key, int fallback = 0) {
    if (!object.is_object()) {
        return fallback;
    }
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number()) {
        return fallback;
    }
    return it->get<int>();
}

bool boolOr(const Json& object, const char* key, bool fallback = false) {
    if (!object.is_object()) {
        return fallback;
    }
    const auto it = object.find(key);
    if (it == object.end() || !it->is_boolean()) {
        return fallback;
    }
    return it->get<bool>();
}

// The smallest image that is at least 300 pixels wide, else the largest one. Spotify lists
// images largest first, with sizes that are sometimes null.
std::string pickImage(const Json& images) {
    if (!images.is_array() || images.empty()) {
        return {};
    }
    std::string best;
    int bestWidth = 0;
    for (const Json& image : images) {
        const std::string url = stringOr(image, "url");
        if (url.empty()) {
            continue;
        }
        const int width = intOr(image, "width", 0);
        if (best.empty() || (width >= 300 && (bestWidth < 300 || width < bestWidth)) ||
            (width < 300 && bestWidth < 300 && width > bestWidth)) {
            best = url;
            bestWidth = width;
        }
    }
    return best;
}

Device parseDevice(const Json& value) {
    Device device;
    device.id = stringOr(value, "id");
    device.name = stringOr(value, "name");
    device.type = stringOr(value, "type");
    device.isActive = boolOr(value, "is_active");
    device.volumePercent = intOr(value, "volume_percent");
    return device;
}

Track parseTrack(const Json& item) {
    Track track;
    track.uri = stringOr(item, "uri");
    track.name = stringOr(item, "name");
    track.durationMs = intOr(item, "duration_ms");
    if (const auto artists = item.find("artists"); artists != item.end() && artists->is_array()) {
        std::string joined;
        for (const Json& artist : *artists) {
            const std::string name = stringOr(artist, "name");
            if (name.empty()) {
                continue;
            }
            if (!joined.empty()) {
                joined += ", ";
            }
            joined += name;
        }
        track.artists = joined;
    }
    if (const auto album = item.find("album"); album != item.end() && album->is_object()) {
        track.albumName = stringOr(*album, "name");
        if (const auto images = album->find("images"); images != album->end()) {
            track.imageUrl = pickImage(*images);
        }
    }
    return track;
}

} // namespace

const std::vector<std::string>& requiredScopes() {
    static const std::vector<std::string> scopes = {
        "user-read-private",           "user-read-playback-state", "user-modify-playback-state",
        "user-read-currently-playing", "playlist-read-private",    "playlist-read-collaborative",
    };
    return scopes;
}

Profile parseProfile(std::string_view json) {
    const Json root = parse(json);
    if (!root.is_object()) {
        throw ParseError("profile: expected an object");
    }
    Profile profile;
    profile.id = stringOr(root, "id");
    profile.displayName = stringOr(root, "display_name");
    const std::string product = stringOr(root, "product");
    if (!product.empty()) {
        profile.product = product;
    }
    return profile;
}

std::vector<Device> parseDevices(std::string_view json) {
    const Json root = parse(json);
    std::vector<Device> devices;
    if (!root.is_object()) {
        throw ParseError("devices: expected an object");
    }
    if (const auto list = root.find("devices"); list != root.end() && list->is_array()) {
        for (const Json& value : *list) {
            devices.push_back(parseDevice(value));
        }
    }
    return devices;
}

PlayerState parsePlayerState(std::string_view json) {
    PlayerState state;
    const bool blank = std::all_of(json.begin(), json.end(), [](unsigned char c) { return std::isspace(c); });
    if (blank) {
        return state;
    }
    const Json root = parse(json);
    if (!root.is_object()) {
        throw ParseError("player: expected an object");
    }
    state.active = true;
    state.isPlaying = boolOr(root, "is_playing");
    state.progressMs = intOr(root, "progress_ms");
    if (const auto device = root.find("device"); device != root.end() && device->is_object()) {
        state.device = parseDevice(*device);
    }
    if (const auto item = root.find("item"); item != root.end() && item->is_object()) {
        state.track = parseTrack(*item);
    }
    if (const auto context = root.find("context"); context != root.end() && context->is_object()) {
        state.contextUri = stringOr(*context, "uri");
    }
    return state;
}

PlaylistPage parsePlaylistPage(std::string_view json) {
    const Json root = parse(json);
    if (!root.is_object()) {
        throw ParseError("playlists: expected an object");
    }
    PlaylistPage page;
    page.total = intOr(root, "total");
    page.offset = intOr(root, "offset");
    page.limit = intOr(root, "limit");
    if (const auto next = root.find("next"); next != root.end()) {
        page.hasNext = next->is_string() && !next->get<std::string>().empty();
    }
    if (const auto items = root.find("items"); items != root.end() && items->is_array()) {
        for (const Json& value : *items) {
            // Spotify may pad a page with null entries for playlists it cannot show.
            if (!value.is_object()) {
                continue;
            }
            Playlist playlist;
            playlist.id = stringOr(value, "id");
            playlist.uri = stringOr(value, "uri");
            playlist.name = stringOr(value, "name");
            if (const auto owner = value.find("owner"); owner != value.end()) {
                playlist.ownerName = stringOr(*owner, "display_name");
            }
            if (const auto images = value.find("images"); images != value.end()) {
                playlist.imageUrl = pickImage(*images);
            }
            page.items.push_back(playlist);
        }
    }
    return page;
}

std::optional<ApiError> parseError(std::string_view json) {
    const bool blank = std::all_of(json.begin(), json.end(), [](unsigned char c) { return std::isspace(c); });
    if (blank) {
        return std::nullopt;
    }
    Json root;
    try {
        root = Json::parse(json);
    } catch (const Json::parse_error&) {
        return std::nullopt;
    }
    if (!root.is_object()) {
        return std::nullopt;
    }
    const auto error = root.find("error");
    if (error == root.end() || !error->is_object()) {
        return std::nullopt;
    }
    ApiError result;
    result.status = intOr(*error, "status");
    result.message = stringOr(*error, "message");
    result.reason = stringOr(*error, "reason");
    return result;
}

Classification classify(Command command, int status, std::string_view body,
                        std::optional<std::string_view> retryAfter) {
    Classification result;
    if (status >= 200 && status < 300) {
        result.outcome = Outcome::Ok;
        return result;
    }
    const std::optional<ApiError> error = parseError(body);
    result.message = error ? error->message : std::string();
    const std::string reason = error ? error->reason : std::string();

    if (status == 401) {
        result.outcome = Outcome::Unauthorized;
    } else if (status == 429) {
        result.outcome = Outcome::RateLimited;
        int seconds = 0;
        if (retryAfter) {
            const auto text = *retryAfter;
            std::from_chars(text.data(), text.data() + text.size(), seconds);
        }
        result.retryAfterSeconds = std::max(1, seconds);
    } else if (status == 404 && reason == "NO_ACTIVE_DEVICE") {
        result.outcome = Outcome::NoActiveDevice;
    } else if (status == 403 && reason == "PREMIUM_REQUIRED") {
        result.outcome = Outcome::PremiumRequired;
    } else if (status == 403 && command == Command::Pause) {
        // Spotify answers 403 "Restriction violated" when there is nothing to pause.
        result.outcome = Outcome::NothingToPause;
    } else if (status == 403) {
        result.outcome = Outcome::Forbidden;
    } else if (status == 404) {
        result.outcome = Outcome::NotFound;
    } else if (status >= 500) {
        result.outcome = Outcome::ServerError;
    } else {
        result.outcome = Outcome::Other;
    }
    return result;
}

std::set<std::string> parseScopes(std::string_view spaceSeparated) {
    std::set<std::string> scopes;
    std::istringstream stream{std::string(spaceSeparated)};
    std::string scope;
    while (stream >> scope) {
        scopes.insert(scope);
    }
    return scopes;
}

std::vector<std::string> missingScopes(const std::set<std::string>& granted) {
    std::vector<std::string> missing;
    for (const std::string& scope : requiredScopes()) {
        if (!granted.contains(scope)) {
            missing.push_back(scope);
        }
    }
    return missing;
}

} // namespace cadence::spotify
