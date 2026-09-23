#include <cadence/core/time.hpp>

#include <cctype>

namespace cadence::core {

namespace {

bool isDigit(char c) noexcept {
    return c >= '0' && c <= '9';
}

int digitPair(std::string_view text) noexcept {
    return (text[0] - '0') * 10 + (text[1] - '0');
}

} // namespace

std::optional<Minutes> parseTimeOfDay(std::string_view text) {
    if (text.size() != 5 || text[2] != ':') {
        return std::nullopt;
    }
    if (!isDigit(text[0]) || !isDigit(text[1]) || !isDigit(text[3]) || !isDigit(text[4])) {
        return std::nullopt;
    }
    const int hours = digitPair(text.substr(0, 2));
    const int minutes = digitPair(text.substr(3, 2));
    if (hours > 23 || minutes > 59) {
        return std::nullopt;
    }
    return timeOfDay(hours, minutes);
}

std::string formatTimeOfDay(Minutes value) {
    const auto total = value.count();
    const auto hours = total / 60;
    const auto minutes = total % 60;
    std::string out;
    out.reserve(5);
    out += static_cast<char>('0' + hours / 10);
    out += static_cast<char>('0' + hours % 10);
    out += ':';
    out += static_cast<char>('0' + minutes / 10);
    out += static_cast<char>('0' + minutes % 10);
    return out;
}

} // namespace cadence::core
