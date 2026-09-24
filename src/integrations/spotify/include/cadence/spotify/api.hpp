#pragma once

#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Qt free view of the Spotify Web API: the shapes Cadence needs, parsed from response bodies, and
// the rules for turning a status code into an outcome. Everything here is checked against
// recorded fixtures.
namespace cadence::spotify {

inline constexpr int redirectPort = 43821;
inline constexpr std::string_view redirectUri = "http://127.0.0.1:43821/callback";

// The scopes Cadence asks for. A connection granted with fewer needs a re-consent.
const std::vector<std::string>& requiredScopes();

struct Profile {
    std::string id;
    std::string displayName;
    // Empty when the profile scope is missing; "premium" when playback control is allowed.
    std::optional<std::string> product;

    friend bool operator==(const Profile&, const Profile&) = default;
};

struct Device {
    std::string id;
    std::string name;
    // "Computer", "Smartphone", "Speaker" and so on, as Spotify names them.
    std::string type;
    bool isActive = false;
    int volumePercent = 0;

    friend bool operator==(const Device&, const Device&) = default;
};

struct Track {
    std::string uri;
    std::string name;
    // All artist names joined with ", ".
    std::string artists;
    std::string albumName;
    std::string imageUrl;
    int durationMs = 0;

    friend bool operator==(const Track&, const Track&) = default;
};

struct PlayerState {
    // False for the 204 "nothing playing anywhere" answer.
    bool active = false;
    bool isPlaying = false;
    int progressMs = 0;
    std::optional<Device> device;
    std::optional<Track> track;
    std::string contextUri;

    friend bool operator==(const PlayerState&, const PlayerState&) = default;
};

struct Playlist {
    std::string id;
    std::string uri;
    std::string name;
    std::string ownerName;
    std::string imageUrl;

    friend bool operator==(const Playlist&, const Playlist&) = default;
};

struct PlaylistPage {
    std::vector<Playlist> items;
    int total = 0;
    int offset = 0;
    int limit = 0;
    bool hasNext = false;

    friend bool operator==(const PlaylistPage&, const PlaylistPage&) = default;
};

struct ApiError {
    int status = 0;
    std::string message;
    std::string reason;

    friend bool operator==(const ApiError&, const ApiError&) = default;
};

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

Profile parseProfile(std::string_view json);
std::vector<Device> parseDevices(std::string_view json);
// An empty body is the inactive state.
PlayerState parsePlayerState(std::string_view json);
PlaylistPage parsePlaylistPage(std::string_view json);
// Empty when the body carries no error object.
std::optional<ApiError> parseError(std::string_view json);

enum class Command {
    Read,
    Play,
    Pause,
    Next,
    Previous,
    Volume,
    Transfer,
};

enum class Outcome {
    Ok,
    // 404 NO_ACTIVE_DEVICE: the target device is not ready; one retry with the device id.
    NoActiveDevice,
    // 403 on pause while nothing plays: not an error.
    NothingToPause,
    PremiumRequired,
    Unauthorized,
    RateLimited,
    Forbidden,
    NotFound,
    ServerError,
    Other,
};

struct Classification {
    Outcome outcome = Outcome::Other;
    // From the Retry-After header for RateLimited, else zero.
    int retryAfterSeconds = 0;
    std::string message;

    friend bool operator==(const Classification&, const Classification&) = default;
};

// Any 2xx is success whatever the documentation promised. The body and Retry-After header refine
// the failures.
Classification classify(Command command, int status, std::string_view body,
                        std::optional<std::string_view> retryAfter = std::nullopt);

std::set<std::string> parseScopes(std::string_view spaceSeparated);
std::vector<std::string> missingScopes(const std::set<std::string>& granted);

} // namespace cadence::spotify
