#include "appinfo.hpp"

#include <cadence/core/version.hpp>

AppInfo::AppInfo(QObject* parent) : QObject(parent) {}

QString AppInfo::version() const {
    const auto text = cadence::core::versionString();
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}
