#include <cadence/core/language.hpp>

#include <algorithm>
#include <cctype>

namespace cadence::core {

namespace {

std::string primaryTag(std::string_view tag) {
    const std::size_t end = tag.find_first_of("-_");
    std::string primary(tag.substr(0, end));
    std::transform(primary.begin(), primary.end(), primary.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return primary;
}

bool isShipped(std::string_view code) {
    return std::find(shippedLanguages.begin(), shippedLanguages.end(), code) != shippedLanguages.end();
}

} // namespace

std::string resolveLanguage(std::string_view setting, const std::vector<std::string>& systemPreferences) {
    if (isShipped(setting)) {
        return std::string(setting);
    }
    for (const std::string& preference : systemPreferences) {
        const std::string primary = primaryTag(preference);
        if (isShipped(primary)) {
            return primary;
        }
    }
    return "en";
}

} // namespace cadence::core
