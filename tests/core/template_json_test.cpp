#include <cadence/core/template_json.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <sstream>
#include <string>

using namespace cadence::core;
using namespace std::chrono_literals;
using Catch::Matchers::ContainsSubstring;

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string defaultTemplateText() {
    return readFile(std::string(CADENCE_RESOURCES_DIR) + "/templates/default.json");
}

// Two activities "a" and "b", one Monday with the given blocks.
std::string documentWith(const std::string& blocks, const std::string& dayCutoff = "22:00") {
    return R"({"version": 1,
        "activities": [
            {"id": "a", "name": "A", "color": "#112233"},
            {"id": "b", "name": "B", "color": "#445566"}
        ],
        "week": {"mon": {"dayStart": "08:00", "dayCutoff": ")" +
           dayCutoff + R"(", "blocks": [)" + blocks + "]}}}";
}

std::vector<ValidationIssue> issuesOf(const std::string& json) {
    return validate(parseTemplateDocument(json));
}

} // namespace

TEST_CASE("the shipped default template describes the weekday from the spec", "[json]") {
    const TemplateDocument document = loadTemplateDocument(defaultTemplateText());

    REQUIRE(document.activities.size() == 5);
    CHECK(document.activities[0].id == "work");
    CHECK(document.activities[4].id == "reading");

    CHECK(document.week.isFreeDay(std::chrono::Saturday));
    CHECK(document.week.isFreeDay(std::chrono::Sunday));
    for (const auto weekday : {std::chrono::Monday, std::chrono::Tuesday, std::chrono::Wednesday,
                               std::chrono::Thursday, std::chrono::Friday}) {
        REQUIRE_FALSE(document.week.isFreeDay(weekday));
        CHECK(*document.week.day(weekday) == *document.week.day(std::chrono::Monday));
    }

    const DayTemplate& day = *document.week.day(std::chrono::Monday);
    CHECK(day.dayStart == timeOfDay(8, 30));
    CHECK(day.dayCutoff == timeOfDay(23, 0));
    REQUIRE(day.blocks.size() == 5);

    const BlockTemplate& work = day.blocks[0];
    CHECK(work.activityId == "work");
    CHECK(work.kind == BlockKind::Anchored);
    CHECK(work.start == timeOfDay(8, 30));
    CHECK(work.durationMinutes == 390min);
    REQUIRE(work.pomodoro.has_value());
    CHECK(work.pomodoro->focus == 25min);
    CHECK(work.pomodoro->shortBreak == 5min);
    CHECK(work.pomodoro->longBreak == 15min);
    CHECK(work.pomodoro->longBreakEvery == 4);
    CHECK_FALSE(work.pomodoro->count.has_value());
    CHECK(resolvedPomodoroCount(work) == 12);
    CHECK(pomodoroDuration(*work.pomodoro, 12) == 375min);
    CHECK(work.pushupsOnBreak);

    const BlockTemplate& lunch = day.blocks[1];
    CHECK(lunch.kind == BlockKind::Flexible);
    CHECK(lunch.durationMinutes == 60min);
    CHECK_FALSE(lunch.pomodoro.has_value());
    CHECK_FALSE(lunch.pushupsOnBreak);

    const BlockTemplate& french = day.blocks[2];
    CHECK(french.kind == BlockKind::Flexible);
    CHECK(resolvedPomodoroCount(french) == 2);
    CHECK(french.pushupsOnBreak);

    const BlockTemplate& logic = day.blocks[3];
    CHECK(resolvedPomodoroCount(logic) == 1);
    CHECK(logic.pushupsOnBreak);

    const BlockTemplate& reading = day.blocks[4];
    CHECK(reading.kind == BlockKind::Soft);
    CHECK(reading.start == timeOfDay(21, 0));
    CHECK(reading.durationMinutes == 30min);
}

TEST_CASE("a template survives a serialize and parse round trip", "[json]") {
    const TemplateDocument original = loadTemplateDocument(defaultTemplateText());

    const std::string text = serializeTemplateDocument(original);
    const TemplateDocument reloaded = loadTemplateDocument(text);

    CHECK(reloaded == original);
    CHECK(serializeTemplateDocument(reloaded) == text);
}

TEST_CASE("serialization writes the schema version and omits free days", "[json]") {
    TemplateDocument document;
    document.activities.push_back({"a", "A", "#000000"});
    DayTemplate sunday;
    sunday.dayStart = timeOfDay(10, 0);
    BlockTemplate block;
    block.activityId = "a";
    block.kind = BlockKind::Soft;
    block.start = timeOfDay(18, 0);
    block.durationMinutes = 45min;
    sunday.blocks.push_back(block);
    document.week.day(std::chrono::Sunday) = sunday;

    const std::string text = serializeTemplateDocument(document);

    CHECK_THAT(text, ContainsSubstring("\"version\": 1"));
    CHECK_THAT(text, ContainsSubstring("\"sun\""));
    CHECK_THAT(text, !ContainsSubstring("\"mon\""));
    CHECK_THAT(text, ContainsSubstring("\"start\": \"18:00\""));
    CHECK(loadTemplateDocument(text) == document);
}

TEST_CASE("the weekdays shorthand fills Monday to Friday", "[json]") {
    const std::string json = R"({"version": 1,
        "activities": [{"id": "a", "name": "A", "color": "#112233"}],
        "week": {"weekdays": {"dayStart": "09:00", "blocks": [
            {"activity": "a", "kind": "flexible", "duration": 30}]}}})";

    const TemplateDocument document = loadTemplateDocument(json);

    for (const auto weekday : {std::chrono::Monday, std::chrono::Tuesday, std::chrono::Wednesday,
                               std::chrono::Thursday, std::chrono::Friday}) {
        REQUIRE_FALSE(document.week.isFreeDay(weekday));
        CHECK(document.week.day(weekday)->dayStart == timeOfDay(9, 0));
        CHECK(document.week.day(weekday)->blocks.size() == 1);
    }
    CHECK(document.week.isFreeDay(std::chrono::Saturday));
    CHECK(document.week.isFreeDay(std::chrono::Sunday));
}

TEST_CASE("explicit days override the weekdays shorthand whatever their order", "[json]") {
    const std::string json = R"({"version": 1,
        "activities": [{"id": "a", "name": "A", "color": "#112233"}],
        "week": {
            "wed": {"dayStart": "10:00", "blocks": []},
            "weekdays": {"dayStart": "09:00", "blocks": [
                {"activity": "a", "kind": "flexible", "duration": 30}]},
            "fri": null,
            "sat": {"dayStart": "11:00", "blocks": []}
        }})";

    const TemplateDocument document = loadTemplateDocument(json);

    CHECK(document.week.day(std::chrono::Monday)->dayStart == timeOfDay(9, 0));
    CHECK(document.week.day(std::chrono::Tuesday)->dayStart == timeOfDay(9, 0));
    CHECK(document.week.day(std::chrono::Thursday)->dayStart == timeOfDay(9, 0));
    REQUIRE_FALSE(document.week.isFreeDay(std::chrono::Wednesday));
    CHECK(document.week.day(std::chrono::Wednesday)->dayStart == timeOfDay(10, 0));
    CHECK(document.week.day(std::chrono::Wednesday)->blocks.empty());
    CHECK(document.week.isFreeDay(std::chrono::Friday));
    CHECK(document.week.day(std::chrono::Saturday)->dayStart == timeOfDay(11, 0));
    CHECK(document.week.isFreeDay(std::chrono::Sunday));
}

TEST_CASE("serialization emits the shorthand only when Monday to Friday are identical", "[json]") {
    const TemplateDocument document = loadTemplateDocument(defaultTemplateText());

    SECTION("identical weekdays collapse into weekdays") {
        const std::string text = serializeTemplateDocument(document);
        CHECK_THAT(text, ContainsSubstring("\"weekdays\""));
        CHECK_THAT(text, !ContainsSubstring("\"mon\""));
        CHECK_THAT(text, !ContainsSubstring("\"fri\""));
        CHECK(loadTemplateDocument(text) == document);
    }

    SECTION("a differing weekday forces explicit keys") {
        TemplateDocument modified = document;
        modified.week.day(std::chrono::Wednesday)->dayStart = timeOfDay(9, 0);
        const std::string text = serializeTemplateDocument(modified);
        CHECK_THAT(text, !ContainsSubstring("\"weekdays\""));
        CHECK_THAT(text, ContainsSubstring("\"mon\""));
        CHECK_THAT(text, ContainsSubstring("\"wed\""));
        CHECK(loadTemplateDocument(text) == modified);
    }

    SECTION("a free weekday forces explicit keys") {
        TemplateDocument modified = document;
        modified.week.day(std::chrono::Friday).reset();
        const std::string text = serializeTemplateDocument(modified);
        CHECK_THAT(text, !ContainsSubstring("\"weekdays\""));
        CHECK_THAT(text, !ContainsSubstring("\"fri\""));
        CHECK(loadTemplateDocument(text) == modified);
    }
}

TEST_CASE("validation reports an issue in shared weekdays once", "[json][validation]") {
    const std::string json = R"({"version": 1,
        "activities": [{"id": "a", "name": "A", "color": "#112233"}],
        "week": {"weekdays": {"dayStart": "09:00", "blocks": [
            {"activity": "nope", "kind": "flexible", "duration": 30}]}}})";

    const auto issues = validate(parseTemplateDocument(json));
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].location == "week.mon.blocks[0]");
}

TEST_CASE("parse errors name the offending element", "[json]") {
    CHECK_THROWS_WITH(parseTemplateDocument("{not json"), ContainsSubstring("not valid JSON"));
    CHECK_THROWS_WITH(parseTemplateDocument("[]"), ContainsSubstring("expected an object"));
    CHECK_THROWS_WITH(parseTemplateDocument(R"({"activities": [], "week": {}})"),
                      ContainsSubstring("missing \"version\""));
    CHECK_THROWS_WITH(parseTemplateDocument(R"({"version": 2, "activities": [], "week": {}})"),
                      ContainsSubstring("unsupported template version 2"));
    CHECK_THROWS_WITH(parseTemplateDocument(R"({"version": 1, "week": {}})"),
                      ContainsSubstring("missing \"activities\""));
    CHECK_THROWS_WITH(parseTemplateDocument(R"({"version": 1, "activities": [], "week": {"monday": null}})"),
                      ContainsSubstring("unknown weekday \"monday\""));
    CHECK_THROWS_WITH(
        parseTemplateDocument(R"({"version": 1, "activities": [{"id": "a", "name": "A"}], "week": {}})"),
        ContainsSubstring("activities[0]: missing \"color\""));
    CHECK_THROWS_WITH(
        parseTemplateDocument(documentWith(R"({"activity": "a", "kind": "fixed", "duration": 10})")),
        ContainsSubstring("week.mon.blocks[0].kind: unknown block kind \"fixed\""));
    CHECK_THROWS_WITH(
        parseTemplateDocument(
            documentWith(R"({"activity": "a", "kind": "anchored", "start": "8:00", "duration": 10})")),
        ContainsSubstring("week.mon.blocks[0].start: expected a time formatted as HH:MM, got \"8:00\""));
    CHECK_THROWS_WITH(
        parseTemplateDocument(documentWith(R"({"activity": "a", "kind": "flexible", "duration": "10"})")),
        ContainsSubstring("week.mon.blocks[0].duration: expected an integer"));
    CHECK_THROWS_WITH(parseTemplateDocument(documentWith(
                          R"({"activity": "a", "kind": "flexible", "duration": 10, "pushupsOnBreak": 1})")),
                      ContainsSubstring("pushupsOnBreak: expected true or false"));
}

TEST_CASE("validation reports unknown activity ids", "[json][validation]") {
    const auto issues = issuesOf(documentWith(R"({"activity": "nope", "kind": "flexible", "duration": 10})"));
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].location == "week.mon.blocks[0]");
    CHECK_THAT(issues[0].message, ContainsSubstring("unknown activity id \"nope\""));
    CHECK(issues[0].code == IssueCode::UnknownActivity);
    CHECK(issues[0].args == std::vector<std::string>{"nope"});
}

TEST_CASE("validation reports duplicate and malformed activities", "[json][validation]") {
    const std::string json = R"({"version": 1,
        "activities": [
            {"id": "a", "name": "A", "color": "#112233"},
            {"id": "a", "name": "Again", "color": "red"},
            {"id": "", "name": "Empty", "color": "#FFFFFF"}
        ],
        "week": {}})";
    const auto issues = issuesOf(json);
    REQUIRE(issues.size() == 3);
    CHECK_THAT(issues[0].message, ContainsSubstring("duplicate activity id \"a\""));
    CHECK(issues[1].location == "activities[1].color");
    CHECK_THAT(issues[1].message, ContainsSubstring("#RRGGBB"));
    CHECK_THAT(issues[2].message, ContainsSubstring("cannot be empty"));
}

TEST_CASE("validation reports zero and missing durations", "[json][validation]") {
    SECTION("zero duration") {
        const auto issues = issuesOf(documentWith(R"({"activity": "a", "kind": "flexible", "duration": 0})"));
        REQUIRE(issues.size() == 1);
        CHECK_THAT(issues[0].message, ContainsSubstring("duration must be greater than zero"));
    }
    SECTION("no duration and no pomodoro count") {
        const auto issues =
            issuesOf(documentWith(R"({"activity": "a", "kind": "flexible", "pomodoro": {"focus": 25}})"));
        REQUIRE(issues.size() == 1);
        CHECK_THAT(issues[0].message, ContainsSubstring("needs a duration or a pomodoro plan with a count"));
    }
    SECTION("zero pomodoro count") {
        const auto issues =
            issuesOf(documentWith(R"({"activity": "a", "kind": "flexible", "pomodoro": {"count": 0}})"));
        REQUIRE_FALSE(issues.empty());
        CHECK_THAT(issues[0].message, ContainsSubstring("pomodoro count must be greater than zero"));
    }
}

TEST_CASE("validation reports blocks crossing midnight", "[json][validation]") {
    const auto issues =
        issuesOf(documentWith(R"({"activity": "a", "kind": "anchored", "start": "23:30", "duration": 60})"));
    REQUIRE(issues.size() == 1);
    CHECK_THAT(issues[0].message, ContainsSubstring("crosses midnight"));
    CHECK_THAT(issues[0].message, ContainsSubstring("23:30"));
    CHECK(issues[0].code == IssueCode::CrossesMidnight);
    CHECK(issues[0].args == std::vector<std::string>{"23:30", "60"});
}

TEST_CASE("validation reports overlapping anchored blocks", "[json][validation]") {
    const auto issues = issuesOf(documentWith(R"(
        {"activity": "a", "kind": "anchored", "start": "09:00", "duration": 120},
        {"activity": "b", "kind": "anchored", "start": "10:00", "duration": 30})"));
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].location == "week.mon.blocks[1]");
    CHECK_THAT(issues[0].message, ContainsSubstring("overlaps anchored block 0 (09:00 to 11:00)"));
    CHECK(issues[0].code == IssueCode::AnchoredOverlap);
    CHECK(issues[0].args == std::vector<std::string>{"0", "09:00", "11:00"});

    const auto adjacent = issuesOf(documentWith(R"(
        {"activity": "a", "kind": "anchored", "start": "09:00", "duration": 60},
        {"activity": "b", "kind": "anchored", "start": "10:00", "duration": 30})"));
    CHECK(adjacent.empty());
}

TEST_CASE("validation reports start times on the wrong block kinds", "[json][validation]") {
    SECTION("anchored without start") {
        const auto issues =
            issuesOf(documentWith(R"({"activity": "a", "kind": "anchored", "duration": 30})"));
        REQUIRE(issues.size() == 1);
        CHECK_THAT(issues[0].message, ContainsSubstring("anchored blocks need a start time"));
    }
    SECTION("soft without start") {
        const auto issues = issuesOf(documentWith(R"({"activity": "a", "kind": "soft", "duration": 30})"));
        REQUIRE(issues.size() == 1);
        CHECK_THAT(issues[0].message, ContainsSubstring("soft blocks need a start time"));
    }
    SECTION("flexible with start") {
        const auto issues = issuesOf(
            documentWith(R"({"activity": "a", "kind": "flexible", "start": "09:00", "duration": 30})"));
        REQUIRE(issues.size() == 1);
        CHECK_THAT(issues[0].message, ContainsSubstring("do not take a start time"));
    }
}

TEST_CASE("validation reports a cutoff that is not after the day start", "[json][validation]") {
    const auto issues =
        issuesOf(documentWith(R"({"activity": "a", "kind": "flexible", "duration": 30})", "08:00"));
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].location == "week.mon.dayCutoff");
    CHECK_THAT(issues[0].message, ContainsSubstring("later than dayStart"));
}

TEST_CASE("loading a template with problems throws and lists all of them", "[json][validation]") {
    const std::string json = documentWith(R"(
        {"activity": "nope", "kind": "flexible", "duration": 0},
        {"activity": "a", "kind": "anchored", "start": "23:00", "duration": 120})");

    CHECK_THROWS_WITH(loadTemplateDocument(json), ContainsSubstring("template has 3 problems"));
    CHECK_THROWS_WITH(loadTemplateDocument(json),
                      ContainsSubstring("week.mon.blocks[0]: unknown activity id"));
    CHECK_THROWS_WITH(loadTemplateDocument(json),
                      ContainsSubstring("week.mon.blocks[1]: block crosses midnight"));
}

TEST_CASE("the full screen alarm flag defaults to true and is written only when off", "[json]") {
    const std::string json = R"({"version": 1,
        "activities": [{"id": "a", "name": "A", "color": "#112233"}],
        "week": {"mon": {"dayStart": "09:00", "blocks": [
            {"activity": "a", "kind": "flexible", "duration": 30},
            {"activity": "a", "kind": "flexible", "duration": 30, "fullscreenAlarm": false}]}}})";

    const TemplateDocument document = loadTemplateDocument(json);
    const DayTemplate& monday = *document.week.day(std::chrono::Monday);
    REQUIRE(monday.blocks.size() == 2);
    CHECK(monday.blocks[0].fullscreenAlarm);
    CHECK_FALSE(monday.blocks[1].fullscreenAlarm);

    const std::string text = serializeTemplateDocument(document);
    CHECK_THAT(text, ContainsSubstring("\"fullscreenAlarm\": false"));
    CHECK(loadTemplateDocument(text) == document);
}
