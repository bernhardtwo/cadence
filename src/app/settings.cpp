#include "settings.hpp"

#include <cadence/core/language.hpp>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QtLogging>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;
using namespace cadence::core;

namespace {

Settings* s_instance = nullptr;

} // namespace

Settings::Settings(QString path, cadence::platform::Autostart& autostart, QObject* parent)
    : QObject(parent), path_(std::move(path)), autostart_(autostart) {
    load();
}

Settings::~Settings() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

Settings* Settings::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine) {
    Q_UNUSED(qmlEngine);
    Q_ASSERT(s_instance != nullptr);
    Q_ASSERT(jsEngine->thread() == s_instance->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void Settings::setInstance(Settings* instance) {
    s_instance = instance;
}

void Settings::load() {
    error_.clear();
    if (!QFileInfo::exists(path_)) {
        return;
    }
    QFile file(path_);
    if (!file.open(QIODevice::ReadOnly)) {
        error_ = u"cannot read %1: %2"_s.arg(path_, file.errorString());
        qWarning("%s", qPrintable(error_));
        return;
    }
    try {
        values_ = parseAppSettings(file.readAll().toStdString());
    } catch (const SettingsError& error) {
        // Defaults stand in for a broken file; the screen shows why.
        error_ = u"%1: %2"_s.arg(path_, QString::fromUtf8(error.what()));
        qWarning("%s", qPrintable(error_));
    }
}

void Settings::save() {
    error_.clear();
    if (!QDir().mkpath(QFileInfo(path_).absolutePath())) {
        error_ = u"cannot create %1"_s.arg(QFileInfo(path_).absolutePath());
    } else {
        QSaveFile file(path_);
        if (!file.open(QIODevice::WriteOnly)) {
            error_ = u"cannot write %1: %2"_s.arg(path_, file.errorString());
        } else {
            file.write(QByteArray::fromStdString(serializeAppSettings(values_)));
            if (!file.commit()) {
                error_ = u"cannot write %1: %2"_s.arg(path_, file.errorString());
            }
        }
    }
    if (!error_.isEmpty()) {
        qWarning("%s", qPrintable(error_));
    }
    emit changed();
}

QString Settings::sound() const {
    switch (values_.sound) {
    case SoundMode::Default:
        return u"Default"_s;
    case SoundMode::Soft:
        return u"Soft"_s;
    case SoundMode::Silent:
        return u"Silent"_s;
    }
    return u"Default"_s;
}

void Settings::setLaunchAtLogin(bool enabled) {
    if (enabled == autostart_.isEnabled()) {
        return;
    }
    error_.clear();
    if (!autostart_.setEnabled(enabled)) {
        error_ = u"Could not update launch at login: %1"_s.arg(autostart_.location());
        qWarning("%s", qPrintable(error_));
    }
    emit changed();
}

void Settings::setStartMinimized(bool minimized) {
    if (values_.startMinimized == minimized) {
        return;
    }
    values_.startMinimized = minimized;
    save();
}

void Settings::setSound(const QString& mode) {
    const QString lower = mode.toLower();
    SoundMode parsed = SoundMode::Default;
    if (lower == u"soft"_s) {
        parsed = SoundMode::Soft;
    } else if (lower == u"silent"_s) {
        parsed = SoundMode::Silent;
    } else if (lower != u"default"_s) {
        return;
    }
    if (values_.sound == parsed) {
        return;
    }
    values_.sound = parsed;
    save();
}

void Settings::setMaxSnoozes(int limit) {
    const int clamped = std::clamp(limit, 0, 20);
    if (values_.maxSnoozes == clamped) {
        emit changed();
        return;
    }
    values_.maxSnoozes = clamped;
    save();
}

void Settings::setWarnDayNoLongerFits(bool warn) {
    if (values_.warnDayNoLongerFits == warn) {
        return;
    }
    values_.warnDayNoLongerFits = warn;
    save();
}

void Settings::setDefaultReps(int reps) {
    const int clamped = std::clamp(reps, 1, 500);
    if (values_.defaultReps == clamped) {
        emit changed();
        return;
    }
    values_.defaultReps = clamped;
    save();
}

void Settings::setLanguage(const QString& language) {
    const std::string code = language.toStdString();
    const bool shipped =
        std::find(shippedLanguages.begin(), shippedLanguages.end(), code) != shippedLanguages.end();
    if (!shipped && code != "system") {
        return;
    }
    if (values_.language == code) {
        return;
    }
    values_.language = code;
    save();
}

void Settings::setShowQuotes(bool show) {
    if (values_.showQuotes == show) {
        return;
    }
    values_.showQuotes = show;
    save();
}

void Settings::setShowQuoteOriginals(bool show) {
    if (values_.showQuoteOriginals == show) {
        return;
    }
    values_.showQuoteOriginals = show;
    save();
}

void Settings::setSpotifyClientId(const QString& clientId) {
    const std::string trimmed = clientId.trimmed().toStdString();
    if (values_.spotifyClientId == trimmed) {
        return;
    }
    values_.spotifyClientId = trimmed;
    save();
}

void Settings::setSpotifyAutoplay(bool autoplay) {
    if (values_.spotifyAutoplay == autoplay) {
        return;
    }
    values_.spotifyAutoplay = autoplay;
    save();
}

QString Settings::playlistUriFor(const QString& activityId) const {
    const auto it = values_.activityPlaylists.find(activityId.toStdString());
    return it == values_.activityPlaylists.end() ? QString() : QString::fromStdString(it->second.uri);
}

QString Settings::playlistNameFor(const QString& activityId) const {
    const auto it = values_.activityPlaylists.find(activityId.toStdString());
    return it == values_.activityPlaylists.end() ? QString() : QString::fromStdString(it->second.name);
}

void Settings::setPlaylistFor(const QString& activityId, const QString& uri, const QString& name) {
    if (activityId.isEmpty() || uri.isEmpty()) {
        return;
    }
    values_.activityPlaylists[activityId.toStdString()] =
        PlaylistChoice{uri.toStdString(), name.toStdString()};
    save();
}

void Settings::clearPlaylistFor(const QString& activityId) {
    if (values_.activityPlaylists.erase(activityId.toStdString()) > 0) {
        save();
    }
}

void Settings::refresh() {
    emit changed();
}
