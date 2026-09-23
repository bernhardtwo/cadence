#include <cadence/core/version.hpp>

namespace cadence::core {

Version version() noexcept {
    return Version{CADENCE_VERSION_MAJOR, CADENCE_VERSION_MINOR, CADENCE_VERSION_PATCH};
}

std::string_view versionString() noexcept {
    return CADENCE_VERSION_STRING;
}

} // namespace cadence::core
