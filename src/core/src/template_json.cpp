#include <cadence/core/template_json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <utility>

namespace cadence::core {

namespace {

using Json = nlohmann::ordered_json;

constexpr std::array<const char*, 7> weekdayKeys{"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

[[noreturn]] void fail(const std::string& location, const std::string& message) {
    throw TemplateError(location + ": " + message);
}

std::string join(const std::string& location, std::string_view key) {
    return location + "." + std::string(key);
}

std::string indexed(const std::string& location, std::size_t index) {
    return location + "[" + std::to_string(index) + "]";
}

// Both lookups take the location by value: GCC's -Wdangling-reference flags a reference returning
// function that receives a temporary through a reference parameter.
const Json* optionalMember(const Json& object, const char* key, std::string_view location) {
    if (!object.is_object()) {
        fail(std::string(location), "expected an object");
    }
    const auto it = object.find(key);
    if (it == object.end() || it->is_null()) {
        return nullptr;
    }
    return &*it;
}

const Json& requireMember(const Json& object, const char* key, std::string_view location) {
    const Json* member = optionalMember(object, key, location);
    if (member == nullptr) {
        fail(std::string(location), std::string("missing \"") + key + "\"");
    }
    return *member;
}

std::string asString(const Json& value, const std::string& location) {
    if (!value.is_string()) {
        fail(location, "expected a string");
    }
    return value.get<std::string>();
}

int asInt(const Json& value, const std::string& location) {
    if (!value.is_number_integer()) {
        fail(location, "expected an integer");
    }
    return value.get<int>();
}

bool asBool(const Json& value, const std::string& location) {
    if (!value.is_boolean()) {
        fail(location, "expected true or false");
    }
    return value.get<bool>();
}

Minutes asTime(const Json& value, const std::string& location) {
    const std::string text = asString(value, location);
    const std::optional<Minutes> parsed = parseTimeOfDay(text);
    if (!parsed) {
        fail(location, "expected a time formatted as HH:MM, got \"" + text + "\"");
    }
    return *parsed;
}

std::optional<Minutes> optionalTime(const Json& object, const char* key, const std::string& location) {
    const Json* member = optionalMember(object, key, location);
    if (member == nullptr) {
        return std::nullopt;
    }
    return asTime(*member, join(location, key));
}

std::optional<int> optionalInt(const Json& object, const char* key, const std::string& location) {
    const Json* member = optionalMember(object, key, location);
    if (member == nullptr) {
        return std::nullopt;
    }
    return asInt(*member, join(location, key));
}

BlockKind parseKind(const Json& value, const std::string& location) {
    const std::string text = asString(value, location);
    if (text == "anchored") {
        return BlockKind::Anchored;
    }
    if (text == "flexible") {
        return BlockKind::Flexible;
    }
    if (text == "soft") {
        return BlockKind::Soft;
    }
    fail(location, "unknown block kind \"" + text + "\", expected anchored, flexible or soft");
}

const char* kindName(BlockKind kind) noexcept {
    switch (kind) {
    case BlockKind::Anchored:
        return "anchored";
    case BlockKind::Flexible:
        return "flexible";
    case BlockKind::Soft:
        return "soft";
    }
    return "flexible";
}

PomodoroPlan parsePomodoro(const Json& value, const std::string& location) {
    if (!value.is_object()) {
        fail(location, "expected an object");
    }
    PomodoroPlan plan;
    if (const auto focus = optionalInt(value, "focus", location)) {
        plan.focus = Minutes{*focus};
    }
    if (const auto shortBreak = optionalInt(value, "shortBreak", location)) {
        plan.shortBreak = Minutes{*shortBreak};
    }
    if (const auto longBreak = optionalInt(value, "longBreak", location)) {
        plan.longBreak = Minutes{*longBreak};
    }
    if (const auto every = optionalInt(value, "longBreakEvery", location)) {
        plan.longBreakEvery = *every;
    }
    plan.count = optionalInt(value, "count", location);
    return plan;
}

BlockTemplate parseBlock(const Json& value, const std::string& location) {
    if (!value.is_object()) {
        fail(location, "expected an object");
    }
    BlockTemplate block;
    block.activityId = asString(requireMember(value, "activity", location), join(location, "activity"));
    block.kind = parseKind(requireMember(value, "kind", location), join(location, "kind"));
    block.start = optionalTime(value, "start", location);
    if (const auto duration = optionalInt(value, "duration", location)) {
        block.durationMinutes = Minutes{*duration};
    }
    if (const Json* pomodoro = optionalMember(value, "pomodoro", location)) {
        block.pomodoro = parsePomodoro(*pomodoro, join(location, "pomodoro"));
    }
    if (const Json* pushups = optionalMember(value, "pushupsOnBreak", location)) {
        block.pushupsOnBreak = asBool(*pushups, join(location, "pushupsOnBreak"));
    }
    if (const Json* fullscreen = optionalMember(value, "fullscreenAlarm", location)) {
        block.fullscreenAlarm = asBool(*fullscreen, join(location, "fullscreenAlarm"));
    }
    return block;
}

DayTemplate parseDay(const Json& value, const std::string& location) {
    if (!value.is_object()) {
        fail(location, "expected an object");
    }
    DayTemplate day;
    day.dayStart = asTime(requireMember(value, "dayStart", location), join(location, "dayStart"));
    if (const auto cutoff = optionalTime(value, "dayCutoff", location)) {
        day.dayCutoff = *cutoff;
    }
    const Json& blocks = requireMember(value, "blocks", location);
    const std::string blocksLocation = join(location, "blocks");
    if (!blocks.is_array()) {
        fail(blocksLocation, "expected an array");
    }
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        day.blocks.push_back(parseBlock(blocks[i], indexed(blocksLocation, i)));
    }
    return day;
}

Activity parseActivity(const Json& value, const std::string& location) {
    if (!value.is_object()) {
        fail(location, "expected an object");
    }
    Activity activity;
    activity.id = asString(requireMember(value, "id", location), join(location, "id"));
    activity.name = asString(requireMember(value, "name", location), join(location, "name"));
    activity.color = asString(requireMember(value, "color", location), join(location, "color"));
    return activity;
}

Json toJson(const PomodoroPlan& plan) {
    Json out;
    out["focus"] = static_cast<int>(plan.focus.count());
    out["shortBreak"] = static_cast<int>(plan.shortBreak.count());
    out["longBreak"] = static_cast<int>(plan.longBreak.count());
    out["longBreakEvery"] = plan.longBreakEvery;
    if (plan.count) {
        out["count"] = *plan.count;
    }
    return out;
}

Json toJson(const BlockTemplate& block) {
    Json out;
    out["activity"] = block.activityId;
    out["kind"] = kindName(block.kind);
    if (block.start) {
        out["start"] = formatTimeOfDay(*block.start);
    }
    if (block.durationMinutes) {
        out["duration"] = static_cast<int>(block.durationMinutes->count());
    }
    if (block.pomodoro) {
        out["pomodoro"] = toJson(*block.pomodoro);
    }
    out["pushupsOnBreak"] = block.pushupsOnBreak;
    if (!block.fullscreenAlarm) {
        out["fullscreenAlarm"] = false;
    }
    return out;
}

Json toJson(const DayTemplate& day) {
    Json out;
    out["dayStart"] = formatTimeOfDay(day.dayStart);
    out["dayCutoff"] = formatTimeOfDay(day.dayCutoff);
    Json blocks = Json::array();
    for (const BlockTemplate& block : day.blocks) {
        blocks.push_back(toJson(block));
    }
    out["blocks"] = std::move(blocks);
    return out;
}

bool isHexColor(const std::string& text) noexcept {
    if (text.size() != 7 || text[0] != '#') {
        return false;
    }
    return std::all_of(text.begin() + 1, text.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

void validateBlock(const BlockTemplate& block, const std::set<ActivityId>& known, const std::string& location,
                   std::vector<ValidationIssue>& issues) {
    if (!known.contains(block.activityId)) {
        issues.push_back({location,
                          "unknown activity id \"" + block.activityId + "\"",
                          IssueCode::UnknownActivity,
                          {block.activityId}});
    }

    const bool needsStart = block.kind != BlockKind::Flexible;
    if (needsStart && !block.start) {
        issues.push_back({location,
                          std::string(kindName(block.kind)) + " blocks need a start time",
                          IssueCode::StartRequired,
                          {std::string(kindName(block.kind))}});
    }
    if (!needsStart && block.start) {
        issues.push_back({location, "flexible blocks are placed in sequence and do not take a start time",
                          IssueCode::StartNotAllowed, {}});
    }
    if (block.start && !isValidTimeOfDay(*block.start)) {
        issues.push_back(
            {location, "start must lie between 00:00 and 23:59", IssueCode::StartOutOfRange, {}});
    }

    if (block.durationMinutes && *block.durationMinutes <= Minutes{0}) {
        issues.push_back(
            {location, "duration must be greater than zero", IssueCode::DurationNotPositive, {}});
    }
    if (block.pomodoro) {
        const PomodoroPlan& plan = *block.pomodoro;
        if (plan.focus <= Minutes{0}) {
            issues.push_back(
                {location, "pomodoro focus must be greater than zero", IssueCode::FocusNotPositive, {}});
        }
        if (plan.shortBreak < Minutes{0} || plan.longBreak < Minutes{0}) {
            issues.push_back({location, "pomodoro breaks cannot be negative", IssueCode::BreakNegative, {}});
        }
        if (plan.longBreakEvery < 0) {
            issues.push_back(
                {location, "longBreakEvery cannot be negative", IssueCode::LongBreakEveryNegative, {}});
        }
        if (plan.count && *plan.count <= 0) {
            issues.push_back(
                {location, "pomodoro count must be greater than zero", IssueCode::CountNotPositive, {}});
        }
    }

    const std::optional<Minutes> duration = resolvedDuration(block);
    if (!duration) {
        issues.push_back({location, "a block needs a duration or a pomodoro plan with a count",
                          IssueCode::DurationMissing, {}});
    } else if (*duration <= Minutes{0}) {
        if (!block.durationMinutes) {
            issues.push_back({location,
                              "the pomodoro plan resolves to a zero duration",
                              IssueCode::ZeroResolvedDuration,
                              {}});
        }
    } else if (block.start && isValidTimeOfDay(*block.start) && *block.start + *duration > minutesPerDay) {
        const std::string start = formatTimeOfDay(*block.start);
        const std::string minutes = std::to_string(duration->count());
        issues.push_back({location,
                          "block crosses midnight (" + start + " plus " + minutes + " minutes)",
                          IssueCode::CrossesMidnight,
                          {start, minutes}});
    }
}

void validateDay(const DayTemplate& day, const std::set<ActivityId>& known, const std::string& location,
                 std::vector<ValidationIssue>& issues) {
    if (!isValidTimeOfDay(day.dayStart)) {
        issues.push_back({join(location, "dayStart"),
                          "must lie between 00:00 and 23:59",
                          IssueCode::DayStartOutOfRange,
                          {}});
    }
    if (!isValidTimeOfDay(day.dayCutoff)) {
        issues.push_back({join(location, "dayCutoff"), "must lie between 00:00 and 23:59",
                          IssueCode::DayCutoffOutOfRange, {}});
    }
    if (day.dayCutoff <= day.dayStart) {
        issues.push_back(
            {join(location, "dayCutoff"), "must be later than dayStart", IssueCode::CutoffBeforeStart, {}});
    }

    const std::string blocksLocation = join(location, "blocks");
    struct Placed {
        std::size_t index;
        Minutes start;
        Minutes end;
    };
    std::vector<Placed> anchored;
    for (std::size_t i = 0; i < day.blocks.size(); ++i) {
        const BlockTemplate& block = day.blocks[i];
        validateBlock(block, known, indexed(blocksLocation, i), issues);
        const std::optional<Minutes> duration = resolvedDuration(block);
        if (block.kind == BlockKind::Anchored && block.start && duration && *duration > Minutes{0}) {
            anchored.push_back({i, *block.start, *block.start + *duration});
        }
    }

    std::sort(anchored.begin(), anchored.end(), [](const Placed& a, const Placed& b) {
        return a.start < b.start || (a.start == b.start && a.index < b.index);
    });
    for (std::size_t i = 1; i < anchored.size(); ++i) {
        const Placed& previous = anchored[i - 1];
        const Placed& current = anchored[i];
        if (current.start < previous.end) {
            const std::string other = std::to_string(previous.index);
            const std::string start = formatTimeOfDay(previous.start);
            const std::string end = formatTimeOfDay(previous.end);
            issues.push_back(
                {indexed(blocksLocation, current.index),
                 "anchored block overlaps anchored block " + other + " (" + start + " to " + end + ")",
                 IssueCode::AnchoredOverlap,
                 {other, start, end}});
        }
    }
}

} // namespace

std::vector<ValidationIssue> validate(const TemplateDocument& document) {
    std::vector<ValidationIssue> issues;
    std::set<ActivityId> known;

    for (std::size_t i = 0; i < document.activities.size(); ++i) {
        const Activity& activity = document.activities[i];
        const std::string location = indexed("activities", i);
        if (activity.id.empty()) {
            issues.push_back({location, "activity id cannot be empty", IssueCode::ActivityIdEmpty, {}});
        } else if (!known.insert(activity.id).second) {
            issues.push_back({location,
                              "duplicate activity id \"" + activity.id + "\"",
                              IssueCode::DuplicateActivity,
                              {activity.id}});
        }
        if (!isHexColor(activity.color)) {
            issues.push_back(
                {join(location, "color"), "expected a color formatted as #RRGGBB", IssueCode::BadColor, {}});
        }
    }

    // Identical days, typically expanded from the weekdays shorthand, are reported once under the
    // first of them.
    const auto& days = document.week.days;
    for (std::size_t i = 0; i < weekdayKeys.size(); ++i) {
        if (!days[i]) {
            continue;
        }
        const bool duplicate = std::any_of(days.begin(), days.begin() + static_cast<std::ptrdiff_t>(i),
                                           [&days, i](const auto& day) { return day && *day == *days[i]; });
        if (!duplicate) {
            validateDay(*days[i], known, join("week", weekdayKeys[i]), issues);
        }
    }
    return issues;
}

TemplateDocument parseTemplateDocument(std::string_view json) {
    Json root;
    try {
        root = Json::parse(json);
    } catch (const Json::parse_error& error) {
        throw TemplateError(std::string("template is not valid JSON: ") + error.what());
    }
    if (!root.is_object()) {
        fail("$", "expected an object");
    }

    const int version = asInt(requireMember(root, "version", "$"), "version");
    if (version != templateSchemaVersion) {
        fail("version", "unsupported template version " + std::to_string(version) + ", expected " +
                            std::to_string(templateSchemaVersion));
    }

    TemplateDocument document;

    const Json& activities = requireMember(root, "activities", "$");
    if (!activities.is_array()) {
        fail("activities", "expected an array");
    }
    for (std::size_t i = 0; i < activities.size(); ++i) {
        document.activities.push_back(parseActivity(activities[i], indexed("activities", i)));
    }

    const Json& week = requireMember(root, "week", "$");
    if (!week.is_object()) {
        fail("week", "expected an object");
    }
    // The weekdays shorthand fills Monday to Friday first, whatever its position in the object.
    // Explicit day keys then override it, and an explicit null makes that day free again.
    if (const Json* weekdays = optionalMember(week, "weekdays", "week")) {
        const DayTemplate shared = parseDay(*weekdays, "week.weekdays");
        for (std::size_t index = 0; index < 5; ++index) {
            document.week.days[index] = shared;
        }
    }
    for (const auto& [key, value] : week.items()) {
        if (key == "weekdays") {
            continue;
        }
        const auto found = std::find(weekdayKeys.begin(), weekdayKeys.end(), key);
        if (found == weekdayKeys.end()) {
            fail("week",
                 "unknown weekday \"" + key + "\", expected weekdays, mon, tue, wed, thu, fri, sat or sun");
        }
        const auto index = static_cast<std::size_t>(found - weekdayKeys.begin());
        if (value.is_null()) {
            document.week.days[index].reset();
            continue;
        }
        document.week.days[index] = parseDay(value, join("week", key));
    }
    return document;
}

std::string serializeTemplateDocument(const TemplateDocument& document) {
    Json root;
    root["version"] = templateSchemaVersion;

    Json activities = Json::array();
    for (const Activity& activity : document.activities) {
        Json item;
        item["id"] = activity.id;
        item["name"] = activity.name;
        item["color"] = activity.color;
        activities.push_back(std::move(item));
    }
    root["activities"] = std::move(activities);

    const auto& days = document.week.days;
    const bool sameWeekdays =
        days[0].has_value() && std::all_of(days.begin() + 1, days.begin() + 5, [&days](const auto& day) {
            return day.has_value() && *day == *days[0];
        });
    Json week = Json::object();
    if (sameWeekdays) {
        week["weekdays"] = toJson(*days[0]);
    }
    for (std::size_t i = 0; i < weekdayKeys.size(); ++i) {
        if (sameWeekdays && i < 5) {
            continue;
        }
        if (days[i]) {
            week[weekdayKeys[i]] = toJson(*days[i]);
        }
    }
    root["week"] = std::move(week);

    return root.dump(2) + "\n";
}

TemplateDocument loadTemplateDocument(std::string_view json) {
    TemplateDocument document = parseTemplateDocument(json);
    const std::vector<ValidationIssue> issues = validate(document);
    if (!issues.empty()) {
        std::string message = "template has " + std::to_string(issues.size()) +
                              (issues.size() == 1 ? " problem" : " problems") + ":";
        for (const ValidationIssue& issue : issues) {
            message += "\n  " + issue.location + ": " + issue.message;
        }
        throw TemplateError(message);
    }
    return document;
}

} // namespace cadence::core
