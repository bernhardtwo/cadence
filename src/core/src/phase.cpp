#include <cadence/core/phase.hpp>

#include <algorithm>

namespace cadence::core {

bool closeOpenPhase(std::vector<PhaseRecord>& phases, Seconds at) noexcept {
    if (phases.empty() || phases.back().end) {
        return false;
    }
    PhaseRecord& open = phases.back();
    if (!open.pauses.empty() && !open.pauses.back().end) {
        open.pauses.back().end = std::max(at, open.pauses.back().start);
    }
    open.end = std::max(at, open.start);
    return true;
}

} // namespace cadence::core
