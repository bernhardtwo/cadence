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

struct ValidationIssue {
    // JSON path of the offending element, e.g. "week.mon.blocks[2]".
    std::string location;
    std::string message;

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
