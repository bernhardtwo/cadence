#include <cadence/core/progress_json.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace cadence::core;
using namespace std::chrono_literals;
using Catch::Matchers::ContainsSubstring;

namespace {

DayProgress sampleProgress() {
    DayProgress progress;
    progress.actualDayStart = timeOfDay(9, 5);
    progress.at(0).actualStart = timeOfDay(9, 5);
    progress.at(0).actualEnd = timeOfDay(14, 40);
    progress.at(0).pauses.push_back(PauseInterval{timeOfDay(10, 0), timeOfDay(10, 10)});
    progress.at(0).extended = 15min;
    progress.at(1).skipped = true;
    progress.at(2).postponed = true;
    progress.at(2).pauses.push_back(PauseInterval{timeOfDay(16, 0), std::nullopt});
    progress.at(3).confirmed = false;
    progress.pushups.push_back(PushupSet{0, timeOfDay(9, 30), 10});
    progress.pushups.push_back(PushupSet{0, timeOfDay(10, 0), 12});
    return progress;
}

} // namespace

TEST_CASE("day progress survives a serialize and parse round trip", "[progress]") {
    const DayProgress original = sampleProgress();

    const std::string text = serializeDayProgress(original);
    const DayProgress reloaded = parseDayProgress(text);

    CHECK(reloaded == original);
    CHECK(serializeDayProgress(reloaded) == text);
    CHECK_THAT(text, ContainsSubstring("\"version\": 1"));
}

TEST_CASE("empty progress serializes to a minimal document", "[progress]") {
    const std::string text = serializeDayProgress(DayProgress{});
    CHECK_THAT(text, !ContainsSubstring("actualDayStart"));
    CHECK(parseDayProgress(text) == DayProgress{});
}

TEST_CASE("only the fields that carry information are written", "[progress]") {
    DayProgress progress;
    progress.at(4).skipped = true;
    const std::string text = serializeDayProgress(progress);
    CHECK_THAT(text, ContainsSubstring("\"4\""));
    CHECK_THAT(text, ContainsSubstring("\"skipped\": true"));
    CHECK_THAT(text, !ContainsSubstring("postponed"));
    CHECK_THAT(text, !ContainsSubstring("extended"));
    CHECK_THAT(text, !ContainsSubstring("confirmed"));
}

TEST_CASE("progress parse errors name the offending element", "[progress]") {
    CHECK_THROWS_WITH(parseDayProgress("nope"), ContainsSubstring("not valid JSON"));
    CHECK_THROWS_WITH(parseDayProgress("[]"), ContainsSubstring("expected an object"));
    CHECK_THROWS_WITH(parseDayProgress(R"({"blocks": {}})"), ContainsSubstring("version"));
    CHECK_THROWS_WITH(parseDayProgress(R"({"version": 7})"), ContainsSubstring("unsupported progress version 7"));
    CHECK_THROWS_WITH(parseDayProgress(R"({"version": 1, "blocks": {"x": {}}})"),
                      ContainsSubstring("block key \"x\" is not an index"));
    CHECK_THROWS_WITH(parseDayProgress(R"({"version": 1, "blocks": {"0": {"actualStart": "09:00"}}})"),
                      ContainsSubstring("blocks.0.actualStart: expected an integer number of minutes"));
    CHECK_THROWS_WITH(parseDayProgress(R"({"version": 1, "blocks": {"0": {"pauses": [{"end": 5}]}}})"),
                      ContainsSubstring("blocks.0.pauses[0]: missing \"start\""));
    CHECK_THROWS_WITH(parseDayProgress(R"({"version": 1, "pushups": [{"block": 0}]})"),
                      ContainsSubstring("pushups[0]: expected block, at and reps"));
}
