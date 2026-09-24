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

TEST_CASE("phase and prompt records survive a round trip", "[progress]") {
    DayProgress progress;
    BlockProgress& block = progress.at(3);
    block.actualStart = timeOfDay(16, 52);
    PhaseRecord focus{PhaseKind::Focus, 0, Seconds{60720}, Seconds{62220}};
    focus.pauses.push_back(PhasePause{Seconds{60900}, Seconds{60960}});
    block.phases.push_back(focus);
    PhaseRecord shortBreak{PhaseKind::ShortBreak, 0, Seconds{62220}};
    shortBreak.skipped = true;
    block.phases.push_back(shortBreak);
    block.prompts.push_back(PromptRecord{0, Seconds{62221}, PromptAnswer::Logged});
    block.prompts.push_back(PromptRecord{1, Seconds{64000}});

    const std::string text = serializeDayProgress(progress);
    CHECK_THAT(text, ContainsSubstring("\"phases\""));
    CHECK_THAT(text, ContainsSubstring("\"shortBreak\""));
    CHECK_THAT(text, ContainsSubstring("\"answer\": \"logged\""));
    CHECK(parseDayProgress(text) == progress);
}

TEST_CASE("a progress file written before phases were recorded still loads", "[progress]") {
    const std::string json = R"({
      "version": 1,
      "actualDayStart": 515,
      "blocks": {
        "0": { "actualStart": 515, "actualEnd": 885 },
        "1": { "skipped": true },
        "3": { "actualStart": 1012 }
      },
      "pushups": []
    })";
    const DayProgress progress = parseDayProgress(json);
    REQUIRE(progress.find(3) != nullptr);
    CHECK(progress.find(3)->phases.empty());
    CHECK(progress.find(3)->prompts.empty());
    CHECK(progress.find(0)->actualEnd == timeOfDay(14, 45));
    CHECK(serializeDayProgress(progress).find("phases") == std::string::npos);
}

TEST_CASE("phase records report their problems by location", "[progress]") {
    CHECK_THROWS_WITH(parseDayProgress(R"({"version": 1, "blocks": {"0": {"phases": [{"phase": "nap", "index": 0, "start": 1}]}}})"),
                      ContainsSubstring("blocks.0.phases[0].phase"));
    CHECK_THROWS_WITH(parseDayProgress(R"({"version": 1, "blocks": {"0": {"prompts": [{"set": 0}]}}})"),
                      ContainsSubstring("blocks.0.prompts[0]"));
}
