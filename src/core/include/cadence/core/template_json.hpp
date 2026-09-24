#pragma once

#include <cadence/core/activity.hpp>
#include <cadence/core/block.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cadence::core {

inline constexpr int templateSchemaVersion = 1;

struct TemplateDocument {
    std::vector<Activity> activities;
    WeekTemplate week;

    friend bool operator==(const TemplateDocument&, const TemplateDocument&) = default;
};

class TemplateError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Stable identity of a validation problem, so the app can word it in the user's language. The
// message stays English and is what the command line and the logs print.
enum class IssueCode {
    UnknownActivity, // args: activity id
    StartRequired,   // args: block kind
    StartNotAllowed,
    StartOutOfRange,
    DurationNotPositive,
    FocusNotPositive,
    BreakNegative,
    LongBreakEveryNegative,
    CountNotPositive,
    DurationMissing,
    ZeroResolvedDuration,
    CrossesMidnight, // args: start, duration in minutes
    DayStartOutOfRange,
    DayCutoffOutOfRange,
    CutoffBeforeStart,
    AnchoredOverlap, // args: other block index, its start, its end
    ActivityIdEmpty,
    DuplicateActivity, // args: activity id
    BadColor,
};

struct ValidationIssue {
    // JSON path of the offending element, e.g. "week.mon.blocks[2]".
    std::string location;
    std::string message;
    IssueCode code = IssueCode::UnknownActivity;
    std::vector<std::string> args;

    friend bool operator==(const ValidationIssue&, const ValidationIssue&) = default;
};

// Semantic checks on a well formed document: unknown activity ids, missing or zero durations,
// blocks crossing midnight, overlapping anchored blocks, malformed colors.
std::vector<ValidationIssue> validate(const TemplateDocument& document);

// Throws TemplateError when the text is not JSON or does not match the schema.
TemplateDocument parseTemplateDocument(std::string_view json);

std::string serializeTemplateDocument(const TemplateDocument& document);

// parseTemplateDocument followed by validate; throws TemplateError listing every issue found.
TemplateDocument loadTemplateDocument(std::string_view json);

} // namespace cadence::core
