#include <cadence/spotify/api.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <sstream>
#include <string>

using namespace cadence::spotify;
using Catch::Matchers::ContainsSubstring;

namespace {

// Recorded from a Development Mode app in September 2026, with names and ids replaced.
std::string fixture(const char* name) {
    std::ifstream file(std::string(CADENCE_SPOTIFY_FIXTURES_DIR) + "/" + name);
    REQUIRE(file.is_open());
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

} // namespace

TEST_CASE("the profile carries the product when the scope allows it", "[spotify][parse]") {
    const Profile premium = parseProfile(fixture("profile.json"));
    CHECK(premium.id == "user-1");
    CHECK(premium.displayName == "Someone");
    CHECK(premium.product == "premium");

    // Without user-read-private the field is empty and no warning must be derived from it.
    const Profile unknown = parseProfile(fixture("profile-no-product.json"));
    CHECK(unknown.displayName == "Someone");
    CHECK_FALSE(unknown.product.has_value());
}

TEST_CASE("devices list every entry with its activity and volume", "[spotify][parse]") {
    const std::vector<Device> devices = parseDevices(fixture("devices.json"));
    REQUIRE(devices.size() == 2);
    CHECK(devices[0] == Device{"device-computer-1", "MY-PC", "Computer", false, 0});
    CHECK(devices[1] == Device{"device-phone-1", "Phone", "Smartphone", true, 65});
}

TEST_CASE("the player state carries track, artists, cover, device and context", "[spotify][parse]") {
    const PlayerState state = parsePlayerState(fixture("player-state.json"));
    CHECK(state.active);
    CHECK(state.isPlaying);
    CHECK(state.progressMs == 2205);
    REQUIRE(state.device);
    CHECK(state.device->name == "MY-PC");
    CHECK(state.device->volumePercent == 50);
    REQUIRE(state.track);
    CHECK(state.track->name == "Track A");
    CHECK(state.track->artists == "Artist A, Artist B");
    CHECK(state.track->albumName == "Album A");
    CHECK(state.track->durationMs == 180000);
    // The 300 pixel image is preferred over the 640 and 64 ones.
    CHECK(state.track->imageUrl == "https://i.example/album-300.jpg");
    CHECK(state.contextUri == "spotify:playlist:playlist-1");
}

TEST_CASE("an empty player body is the inactive state", "[spotify][parse]") {
    const PlayerState state = parsePlayerState("");
    CHECK_FALSE(state.active);
    CHECK_FALSE(state.track.has_value());
    CHECK(parsePlayerState("  \n") == PlayerState{});
}

TEST_CASE("a playlist page has no track count and pages through next", "[spotify][parse]") {
    const PlaylistPage page = parsePlaylistPage(fixture("playlists-page.json"));
    CHECK(page.total == 30);
    CHECK(page.offset == 0);
    CHECK(page.limit == 2);
    CHECK(page.hasNext);
    REQUIRE(page.items.size() == 2);
    CHECK(page.items[0].name == "French with music");
    CHECK(page.items[0].uri == "spotify:playlist:playlist-1");
    CHECK(page.items[0].ownerName == "Someone");
    CHECK(page.items[0].imageUrl == "https://i.example/playlist-1.jpg");
    CHECK(page.items[1].name == "Cosmic");

    const PlaylistPage last = parsePlaylistPage(fixture("playlists-last-page.json"));
    CHECK_FALSE(last.hasNext);
    // A null entry is skipped rather than parsed as a playlist.
    CHECK(last.items.size() == 1);
}

TEST_CASE("error bodies expose status, message and reason", "[spotify][parse]") {
    const auto error = parseError(fixture("error-no-device.json"));
    REQUIRE(error);
    CHECK(error->status == 404);
    CHECK(error->reason == "NO_ACTIVE_DEVICE");
    CHECK_THAT(error->message, ContainsSubstring("No active device"));
    CHECK_FALSE(parseError("").has_value());
    CHECK_FALSE(parseError("{}").has_value());
    CHECK_FALSE(parseError("not json").has_value());
}

TEST_CASE("any 2xx is a success whatever the documentation promised", "[spotify][classify]") {
    CHECK(classify(Command::Play, 204, "").outcome == Outcome::Ok);
    CHECK(classify(Command::Play, 200, "").outcome == Outcome::Ok);
    CHECK(classify(Command::Next, 200, "").outcome == Outcome::Ok);
    CHECK(classify(Command::Read, 200, "{}").outcome == Outcome::Ok);
}

TEST_CASE("no active device after a transfer asks for one retry", "[spotify][classify]") {
    const Classification result = classify(Command::Play, 404, fixture("error-no-device.json"));
    CHECK(result.outcome == Outcome::NoActiveDevice);
    // Any other 404 stays a not found.
    CHECK(classify(Command::Read, 404, fixture("error-not-found.json")).outcome == Outcome::NotFound);
}

TEST_CASE("pausing while nothing plays is a no-op, not an error", "[spotify][classify]") {
    const Classification pause = classify(Command::Pause, 403, fixture("error-restriction.json"));
    CHECK(pause.outcome == Outcome::NothingToPause);
    // The same body on another command is a real restriction.
    CHECK(classify(Command::Next, 403, fixture("error-restriction.json")).outcome == Outcome::Forbidden);
}

TEST_CASE("premium required is its own outcome", "[spotify][classify]") {
    const Classification result = classify(Command::Play, 403, fixture("error-premium.json"));
    CHECK(result.outcome == Outcome::PremiumRequired);
    CHECK_THAT(result.message, ContainsSubstring("Premium"));
}

TEST_CASE("401 asks for a refresh and 429 carries the retry delay", "[spotify][classify]") {
    CHECK(classify(Command::Read, 401, fixture("error-unauthorized.json")).outcome == Outcome::Unauthorized);
    const Classification limited = classify(Command::Read, 429, fixture("error-rate-limit.json"), "7");
    CHECK(limited.outcome == Outcome::RateLimited);
    CHECK(limited.retryAfterSeconds == 7);
    // A missing or unparsable header still waits at least a second.
    CHECK(classify(Command::Read, 429, "", std::nullopt).retryAfterSeconds == 1);
    CHECK(classify(Command::Read, 429, "", "soon").retryAfterSeconds == 1);
    CHECK(classify(Command::Read, 503, "").outcome == Outcome::ServerError);
}

TEST_CASE("a token granted without the profile scope needs a re-consent", "[spotify][scopes]") {
    const std::set<std::string> granted =
        parseScopes("user-read-playback-state user-modify-playback-state user-read-currently-playing "
                    "playlist-read-private playlist-read-collaborative");
    CHECK(missingScopes(granted) == std::vector<std::string>{"user-read-private"});

    std::set<std::string> full = granted;
    full.insert("user-read-private");
    CHECK(missingScopes(full).empty());
    CHECK(parseScopes("  ").empty());
}
