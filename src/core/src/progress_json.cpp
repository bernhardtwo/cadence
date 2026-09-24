#include <cadence/core/progress_json.hpp>

#include <cstddef>
#include <nlohmann/json.hpp>
#include <string>

namespace cadence::core {

namespace {

using Json = nlohmann::ordered_json;

[[noreturn]] void fail(const std::string& location, const std::string& message) {
    throw ProgressError(location + ": " + message);
}

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

Minutes asMinutes(const Json& value, const std::string& location) {
    if (!value.is_number_integer()) {
        fail(location, "expected an integer number of minutes");
    }
    return Minutes{value.get<int>()};
}

std::optional<Minutes> optionalMinutes(const Json& object, const char* key, const std::string& location) {
    const Json* member = optionalMember(object, key, location);
    if (member == nullptr) {
        return std::nullopt;
    }
    return asMinutes(*member, location + "." + key);
}

bool optionalBool(const Json& object, const char* key, const std::string& location, bool fallback) {
    const Json* member = optionalMember(object, key, location);
    if (member == nullptr) {
        return fallback;
    }
    if (!member->is_boolean()) {
        fail(location + "." + key, "expected true or false");
    }
    return member->get<bool>();
}

int minutesValue(Minutes value) noexcept {
    return static_cast<int>(value.count());
}

long long secondsValue(Seconds value) noexcept {
    return static_cast<long long>(value.count());
}

Seconds asSeconds(const Json& value, const std::string& location) {
    if (!value.is_number_integer()) {
        fail(location, "expected an integer number of seconds");
    }
    return Seconds{value.get<long long>()};
}

std::optional<Seconds> optionalSeconds(const Json& object, const char* key, const std::string& location) {
    const Json* member = optionalMember(object, key, location);
    if (member == nullptr) {
        return std::nullopt;
    }
    return asSeconds(*member, location + "." + key);
}

const Json& requireMember(const Json& object, const char* key, const std::string& location) {
    const Json* member = optionalMember(object, key, location);
    if (member == nullptr) {
        fail(location, std::string("missing \"") + key + "\"");
    }
    return *member;
}

const char* phaseName(PhaseKind kind) noexcept {
    switch (kind) {
    case PhaseKind::Focus:
        return "focus";
    case PhaseKind::ShortBreak:
        return "shortBreak";
    case PhaseKind::LongBreak:
        return "longBreak";
    }
    return "focus";
}

PhaseKind parsePhase(const Json& value, const std::string& location) {
    if (!value.is_string()) {
        fail(location, "expected a string");
    }
    const std::string text = value.get<std::string>();
    if (text == "focus") {
        return PhaseKind::Focus;
    }
    if (text == "shortBreak") {
        return PhaseKind::ShortBreak;
    }
    if (text == "longBreak") {
        return PhaseKind::LongBreak;
    }
    fail(location, "unknown phase \"" + text + "\", expected focus, shortBreak or longBreak");
}

const char* answerName(PromptAnswer answer) noexcept {
    switch (answer) {
    case PromptAnswer::Pending:
        return "pending";
    case PromptAnswer::Logged:
        return "logged";
    case PromptAnswer::Skipped:
        return "skipped";
    }
    return "pending";
}

PromptAnswer parseAnswer(const Json& value, const std::string& location) {
    if (!value.is_string()) {
        fail(location, "expected a string");
    }
    const std::string text = value.get<std::string>();
    if (text == "pending") {
        return PromptAnswer::Pending;
    }
    if (text == "logged") {
        return PromptAnswer::Logged;
    }
    if (text == "skipped") {
        return PromptAnswer::Skipped;
    }
    fail(location, "unknown answer \"" + text + "\", expected pending, logged or skipped");
}

Json toJson(const PhaseRecord& phase) {
    Json out;
    out["phase"] = phaseName(phase.kind);
    out["index"] = phase.index;
    out["start"] = secondsValue(phase.start);
    if (phase.end) {
        out["end"] = secondsValue(*phase.end);
    }
    if (phase.skipped) {
        out["skipped"] = true;
    }
    if (!phase.pauses.empty()) {
        Json pauses = Json::array();
        for (const PhasePause& pause : phase.pauses) {
            Json item;
            item["start"] = secondsValue(pause.start);
            if (pause.end) {
                item["end"] = secondsValue(*pause.end);
            }
            pauses.push_back(std::move(item));
        }
        out["pauses"] = std::move(pauses);
    }
    return out;
}

PhaseRecord parsePhaseRecord(const Json& value, const std::string& location) {
    PhaseRecord record;
    record.kind = parsePhase(requireMember(value, "phase", location), location + ".phase");
    const Json& index = requireMember(value, "index", location);
    if (!index.is_number_integer()) {
        fail(location + ".index", "expected an integer");
    }
    record.index = index.get<int>();
    record.start = asSeconds(requireMember(value, "start", location), location + ".start");
    record.end = optionalSeconds(value, "end", location);
    record.skipped = optionalBool(value, "skipped", location, false);
    if (const Json* pauses = optionalMember(value, "pauses", location)) {
        const std::string pausesLocation = location + ".pauses";
        if (!pauses->is_array()) {
            fail(pausesLocation, "expected an array");
        }
        for (std::size_t i = 0; i < pauses->size(); ++i) {
            const std::string pauseLocation = pausesLocation + "[" + std::to_string(i) + "]";
            const Json& item = (*pauses)[i];
            record.pauses.push_back(
                PhasePause{asSeconds(requireMember(item, "start", pauseLocation), pauseLocation + ".start"),
                           optionalSeconds(item, "end", pauseLocation)});
        }
    }
    return record;
}

Json toJson(const PromptRecord& prompt) {
    Json out;
    out["set"] = prompt.setIndex;
    out["shown"] = secondsValue(prompt.shown);
    out["answer"] = answerName(prompt.answer);
    return out;
}

PromptRecord parsePromptRecord(const Json& value, const std::string& location) {
    PromptRecord record;
    const Json& set = requireMember(value, "set", location);
    if (!set.is_number_integer()) {
        fail(location + ".set", "expected an integer");
    }
    record.setIndex = set.get<int>();
    record.shown = asSeconds(requireMember(value, "shown", location), location + ".shown");
    if (const Json* answer = optionalMember(value, "answer", location)) {
        record.answer = parseAnswer(*answer, location + ".answer");
    }
    return record;
}

Json toJson(const BlockProgress& progress) {
    Json out = Json::object();
    if (progress.actualStart) {
        out["actualStart"] = minutesValue(*progress.actualStart);
    }
    if (progress.actualEnd) {
        out["actualEnd"] = minutesValue(*progress.actualEnd);
    }
    if (!progress.pauses.empty()) {
        Json pauses = Json::array();
        for (const PauseInterval& pause : progress.pauses) {
            Json item;
            item["start"] = minutesValue(pause.start);
            if (pause.end) {
                item["end"] = minutesValue(*pause.end);
            }
            pauses.push_back(std::move(item));
        }
        out["pauses"] = std::move(pauses);
    }
    if (progress.skipped) {
        out["skipped"] = true;
    }
    if (progress.postponed) {
        out["postponed"] = true;
    }
    if (progress.extended != Minutes{0}) {
        out["extended"] = minutesValue(progress.extended);
    }
    if (progress.confirmed) {
        out["confirmed"] = *progress.confirmed;
    }
    if (!progress.phases.empty()) {
        Json phases = Json::array();
        for (const PhaseRecord& phase : progress.phases) {
            phases.push_back(toJson(phase));
        }
        out["phases"] = std::move(phases);
    }
    if (!progress.prompts.empty()) {
        Json prompts = Json::array();
        for (const PromptRecord& prompt : progress.prompts) {
            prompts.push_back(toJson(prompt));
        }
        out["prompts"] = std::move(prompts);
    }
    return out;
}

BlockProgress parseBlock(const Json& value, const std::string& location) {
    BlockProgress progress;
    progress.actualStart = optionalMinutes(value, "actualStart", location);
    progress.actualEnd = optionalMinutes(value, "actualEnd", location);
    if (const Json* pauses = optionalMember(value, "pauses", location)) {
        const std::string pausesLocation = location + ".pauses";
        if (!pauses->is_array()) {
            fail(pausesLocation, "expected an array");
        }
        for (std::size_t i = 0; i < pauses->size(); ++i) {
            const std::string pauseLocation = pausesLocation + "[" + std::to_string(i) + "]";
            const Json& item = (*pauses)[i];
            const Json* start = optionalMember(item, "start", pauseLocation);
            if (start == nullptr) {
                fail(pauseLocation, "missing \"start\"");
            }
            progress.pauses.push_back(PauseInterval{asMinutes(*start, pauseLocation + ".start"),
                                                    optionalMinutes(item, "end", pauseLocation)});
        }
    }
    progress.skipped = optionalBool(value, "skipped", location, false);
    progress.postponed = optionalBool(value, "postponed", location, false);
    progress.extended = optionalMinutes(value, "extended", location).value_or(Minutes{0});
    if (const Json* confirmed = optionalMember(value, "confirmed", location)) {
        if (!confirmed->is_boolean()) {
            fail(location + ".confirmed", "expected true or false");
        }
        progress.confirmed = confirmed->get<bool>();
    }
    // Files written before phases were recorded simply have none.
    if (const Json* phases = optionalMember(value, "phases", location)) {
        const std::string phasesLocation = location + ".phases";
        if (!phases->is_array()) {
            fail(phasesLocation, "expected an array");
        }
        for (std::size_t i = 0; i < phases->size(); ++i) {
            progress.phases.push_back(
                parsePhaseRecord((*phases)[i], phasesLocation + "[" + std::to_string(i) + "]"));
        }
    }
    if (const Json* prompts = optionalMember(value, "prompts", location)) {
        const std::string promptsLocation = location + ".prompts";
        if (!prompts->is_array()) {
            fail(promptsLocation, "expected an array");
        }
        for (std::size_t i = 0; i < prompts->size(); ++i) {
            progress.prompts.push_back(
                parsePromptRecord((*prompts)[i], promptsLocation + "[" + std::to_string(i) + "]"));
        }
    }
    return progress;
}

} // namespace

std::string serializeDayProgress(const DayProgress& progress) {
    Json root;
    root["version"] = progressSchemaVersion;
    if (progress.actualDayStart) {
        root["actualDayStart"] = minutesValue(*progress.actualDayStart);
    }
    // Block indices become object keys so a sparse map stays sparse in the file.
    Json blocks = Json::object();
    for (const auto& [index, block] : progress.blocks) {
        blocks[std::to_string(index)] = toJson(block);
    }
    root["blocks"] = std::move(blocks);
    Json pushups = Json::array();
    for (const PushupSet& set : progress.pushups) {
        Json item;
        item["block"] = set.blockIndex;
        item["at"] = minutesValue(set.at);
        item["reps"] = set.reps;
        pushups.push_back(std::move(item));
    }
    root["pushups"] = std::move(pushups);
    return root.dump(2) + "\n";
}

DayProgress parseDayProgress(std::string_view json) {
    Json root;
    try {
        root = Json::parse(json);
    } catch (const Json::parse_error& error) {
        throw ProgressError(std::string("progress is not valid JSON: ") + error.what());
    }
    if (!root.is_object()) {
        fail("$", "expected an object");
    }
    const Json* version = optionalMember(root, "version", "$");
    if (version == nullptr || !version->is_number_integer()) {
        fail("version", "missing or not an integer");
    }
    if (version->get<int>() != progressSchemaVersion) {
        fail("version", "unsupported progress version " + std::to_string(version->get<int>()) +
                            ", expected " + std::to_string(progressSchemaVersion));
    }

    DayProgress progress;
    progress.actualDayStart = optionalMinutes(root, "actualDayStart", "$");

    if (const Json* blocks = optionalMember(root, "blocks", "$")) {
        if (!blocks->is_object()) {
            fail("blocks", "expected an object keyed by block index");
        }
        for (const auto& [key, value] : blocks->items()) {
            std::size_t consumed = 0;
            unsigned long long index = 0;
            try {
                index = std::stoull(key, &consumed);
            } catch (const std::exception&) {
                consumed = 0;
            }
            if (consumed != key.size()) {
                fail("blocks", "block key \"" + key + "\" is not an index");
            }
            progress.blocks[static_cast<std::size_t>(index)] = parseBlock(value, "blocks." + key);
        }
    }

    if (const Json* pushups = optionalMember(root, "pushups", "$")) {
        if (!pushups->is_array()) {
            fail("pushups", "expected an array");
        }
        for (std::size_t i = 0; i < pushups->size(); ++i) {
            const std::string location = "pushups[" + std::to_string(i) + "]";
            const Json& item = (*pushups)[i];
            const Json* block = optionalMember(item, "block", location);
            const Json* at = optionalMember(item, "at", location);
            const Json* reps = optionalMember(item, "reps", location);
            if (block == nullptr || !block->is_number_unsigned() || at == nullptr || reps == nullptr ||
                !reps->is_number_integer()) {
                fail(location, "expected block, at and reps");
            }
            progress.pushups.push_back(
                PushupSet{block->get<std::size_t>(), asMinutes(*at, location + ".at"), reps->get<int>()});
        }
    }
    return progress;
}

} // namespace cadence::core
