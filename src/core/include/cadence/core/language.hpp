#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace cadence::core {

// The languages the UI ships, as BCP 47 primary tags. English is the source language.
inline constexpr std::array<std::string_view, 3> shippedLanguages{"en", "es", "fr"};

// The language to load for a settings value: a shipped code is taken as is; "system" (or anything
// else) picks the first entry of the OS preference list whose primary tag is shipped, else English.
// Entries look like "es-MX", "fr_CA" or "de"; the match ignores case and region.
std::string resolveLanguage(std::string_view setting, const std::vector<std::string>& systemPreferences);

} // namespace cadence::core
