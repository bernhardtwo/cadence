#pragma once

#include <cadence/core/clock.hpp>

// The only place in the application that reads the wall clock. Local time comes from Qt, which
// resolves the platform time zone, so core never has to.
class SystemClock final : public cadence::core::IClock {
public:
    cadence::core::Instant now() const override;
};
