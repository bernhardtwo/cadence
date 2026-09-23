#pragma once

#include <cadence/core/time.hpp>

namespace cadence::core {

// Core logic never reads the system clock. The application supplies an implementation and
// tests drive a FakeClock.
class IClock {
public:
    virtual ~IClock() = default;

    virtual Instant now() const = 0;
};

class FakeClock final : public IClock {
public:
    explicit FakeClock(Instant start) noexcept : now_(start) {}
    explicit FakeClock(const TimePoint& start) noexcept : FakeClock(toInstant(start)) {}

    Instant now() const override { return now_; }

    void set(Instant value) noexcept { now_ = value; }
    void set(const TimePoint& value) noexcept { now_ = toInstant(value); }
    void advance(Seconds delta) noexcept { now_ += delta; }

private:
    Instant now_;
};

} // namespace cadence::core
