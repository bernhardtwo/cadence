#pragma once

#include <QString>

#include <memory>

namespace cadence::platform {

class Autostart {
public:
    virtual ~Autostart() = default;

    virtual bool isEnabled() const = 0;
    // Returns false when the registration could not be written or removed.
    virtual bool setEnabled(bool enabled) = 0;
    // Where the registration lives, for diagnostics: a registry key or a file path.
    virtual QString location() const = 0;
};

// The implementation for the platform this binary was built for.
std::unique_ptr<Autostart> createAutostart(const QString& executablePath);

} // namespace cadence::platform
