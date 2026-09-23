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

    const std::string text = serializeAppSettings(original);
    CHECK_THAT(text, ContainsSubstring("\"version\": 1"));
    CHECK_THAT(text, ContainsSubstring("\"sound\": \"silent\""));
    CHECK(parseAppSettings(text) == original);
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
