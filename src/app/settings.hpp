#pragma once

#include <cadence/core/settings_json.hpp>
#include <cadence/platform/autostart.hpp>

#include <QObject>
#include <QQmlEngine>
#include <QString>

// The user's preferences: settings.json under the config directory plus the launch at login
// registration, which lives with the platform and is read live. Every setter persists at once.
class Settings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool launchAtLogin READ launchAtLogin WRITE setLaunchAtLogin NOTIFY changed)
    Q_PROPERTY(QString autostartLocation READ autostartLocation CONSTANT)
    Q_PROPERTY(bool startMinimized READ startMinimized WRITE setStartMinimized NOTIFY changed)
    Q_PROPERTY(QString sound READ sound WRITE setSound NOTIFY changed)
    Q_PROPERTY(int maxSnoozes READ maxSnoozes WRITE setMaxSnoozes NOTIFY changed)
    Q_PROPERTY(bool warnDayNoLongerFits READ warnDayNoLongerFits WRITE setWarnDayNoLongerFits NOTIFY changed)
    Q_PROPERTY(int defaultReps READ defaultReps WRITE setDefaultReps NOTIFY changed)
    Q_PROPERTY(QString spotifyClientId READ spotifyClientId WRITE setSpotifyClientId NOTIFY changed)
    Q_PROPERTY(bool spotifyAutoplay READ spotifyAutoplay WRITE setSpotifyAutoplay NOTIFY changed)
    Q_PROPERTY(QString path READ path CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    Settings(QString path, cadence::platform::Autostart& autostart, QObject* parent = nullptr);
    ~Settings() override;

    static Settings* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static void setInstance(Settings* instance);

    const cadence::core::AppSettings& values() const { return values_; }

    bool launchAtLogin() const { return autostart_.isEnabled(); }
    QString autostartLocation() const { return autostart_.location(); }
    bool startMinimized() const { return values_.startMinimized; }
    QString sound() const;
    cadence::core::SoundMode soundMode() const { return values_.sound; }
    int maxSnoozes() const { return values_.maxSnoozes; }
    bool warnDayNoLongerFits() const { return values_.warnDayNoLongerFits; }
    int defaultReps() const { return values_.defaultReps; }
    QString path() const { return path_; }
    QString error() const { return error_; }

    void setLaunchAtLogin(bool enabled);
    void setStartMinimized(bool minimized);
    void setSound(const QString& mode);
    void setMaxSnoozes(int limit);
    void setWarnDayNoLongerFits(bool warn);
    void setDefaultReps(int reps);

    QString spotifyClientId() const { return QString::fromStdString(values_.spotifyClientId); }
    void setSpotifyClientId(const QString& clientId);
    bool spotifyAutoplay() const { return values_.spotifyAutoplay; }
    void setSpotifyAutoplay(bool autoplay);
    Q_INVOKABLE QString playlistUriFor(const QString& activityId) const;
    Q_INVOKABLE QString playlistNameFor(const QString& activityId) const;
    Q_INVOKABLE void setPlaylistFor(const QString& activityId, const QString& uri, const QString& name);
    Q_INVOKABLE void clearPlaylistFor(const QString& activityId);

    // For changes made elsewhere, such as the tray toggling launch at login.
    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    void load();
    void save();

    QString path_;
    cadence::platform::Autostart& autostart_;
    cadence::core::AppSettings values_;
    QString error_;
};
