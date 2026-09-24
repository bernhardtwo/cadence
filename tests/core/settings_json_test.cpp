#include <cadence/core/settings_json.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace cadence::core;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("settings survive a serialize and parse round trip", "[settings]") {
    AppSettings original;
    original.startMinimized = true;
    original.sound = SoundMode::Silent;
    original.maxSnoozes = 5;
    original.warnDayNoLongerFits = false;
    original.defaultReps = 15;
    original.language = "fr";
    original.showQuotes = false;
    original.showQuoteOriginals = true;

    const std::string text = serializeAppSettings(original);
    CHECK_THAT(text, ContainsSubstring("\"version\": 1"));
    CHECK_THAT(text, ContainsSubstring("\"sound\": \"silent\""));
    CHECK_THAT(text, ContainsSubstring("\"language\": \"fr\""));
    CHECK_THAT(text, ContainsSubstring("\"showOriginals\": true"));
    CHECK(parseAppSettings(text) == original);
}

TEST_CASE("language and quotes default to system, shown and no originals", "[settings]") {
    const AppSettings parsed = parseAppSettings(R"({"version": 1})");
    CHECK(parsed.language == "system");
    CHECK(parsed.showQuotes);
    CHECK_FALSE(parsed.showQuoteOriginals);
    CHECK_FALSE(parseAppSettings(R"({"quotes": {"show": false}})").showQuotes);
    CHECK_THROWS_WITH(parseAppSettings(R"({"language": "de"})"), ContainsSubstring("language"));
    CHECK_THROWS_WITH(parseAppSettings(R"({"quotes": {"show": "yes"}})"), ContainsSubstring("show"));
}

TEST_CASE("missing keys keep their defaults", "[settings]") {
    const AppSettings parsed = parseAppSettings(R"({"version": 1, "maxSnoozes": 0})");
    AppSettings expected;
    expected.maxSnoozes = 0;
    CHECK(parsed == expected);
    CHECK(parseAppSettings("{}") == AppSettings{});
}

TEST_CASE("settings parse errors name the offending key", "[settings]") {
    CHECK_THROWS_WITH(parseAppSettings("nope"), ContainsSubstring("not valid JSON"));
    CHECK_THROWS_WITH(parseAppSettings(R"({"sound": "loud"})"), ContainsSubstring("sound"));
    CHECK_THROWS_WITH(parseAppSettings(R"({"maxSnoozes": 99})"), ContainsSubstring("maxSnoozes"));
    CHECK_THROWS_WITH(parseAppSettings(R"({"defaultReps": 0})"), ContainsSubstring("defaultReps"));
    CHECK_THROWS_WITH(parseAppSettings(R"({"startMinimized": "yes"})"), ContainsSubstring("startMinimized"));
}

TEST_CASE("spotify settings round trip and stay optional", "[settings]") {
    AppSettings original;
    // A fake client id: tests never carry a real one.
    original.spotifyClientId = "fake-client-id-0000000000000000";
    original.spotifyAutoplay = true;
    original.activityPlaylists["work"] = PlaylistChoice{"spotify:playlist:playlist-1", "Deep focus"};
    original.activityPlaylists["french"] = PlaylistChoice{"spotify:playlist:playlist-2", "French with music"};

    const std::string text = serializeAppSettings(original);
    CHECK_THAT(text, ContainsSubstring("\"clientId\": \"fake-client-id-0000000000000000\""));
    CHECK_THAT(text, ContainsSubstring("\"autoplay\": true"));
    CHECK(parseAppSettings(text) == original);

    // A file without the section keeps the defaults; a choice without a uri is dropped.
    CHECK(parseAppSettings(R"({"version": 1})").activityPlaylists.empty());
    const AppSettings partial = parseAppSettings(R"({"spotify": {"playlists": {"work": {"name": "x"}}}})");
    CHECK(partial.activityPlaylists.empty());
    CHECK_THROWS_WITH(parseAppSettings(R"({"spotify": {"clientId": 5}})"),
                      ContainsSubstring("spotify.clientId"));
}
