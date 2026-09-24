#pragma once

#include <cadence/spotify/api.hpp>

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <optional>
#include <vector>

class Settings;

namespace cadence::spotify {
class SpotifyAuth;
class SpotifyClient;
} // namespace cadence::spotify

// What the screens see of Spotify: connection status, the current track, this computer's device,
// the playlist library and the commands. Polls the player every three seconds only while a screen
// that shows it says so.
class SpotifyPlayer : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Spotify)
    QML_SINGLETON

    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool connecting READ connecting NOTIFY changed)
    Q_PROPERTY(bool online READ online NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(QString displayName READ displayName NOTIFY changed)
    Q_PROPERTY(bool premium READ premium NOTIFY changed)
    Q_PROPERTY(bool premiumWarning READ premiumWarning NOTIFY changed)
    Q_PROPERTY(bool needsReconsent READ needsReconsent NOTIFY changed)
    Q_PROPERTY(QString redirectUri READ redirectUri CONSTANT)
    Q_PROPERTY(int redirectPort READ redirectPort CONSTANT)

    Q_PROPERTY(bool hasActiveDevice READ hasActiveDevice NOTIFY changed)
    Q_PROPERTY(bool canTransferHere READ canTransferHere NOTIFY changed)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY changed)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY changed)
    Q_PROPERTY(QString trackName READ trackName NOTIFY changed)
    Q_PROPERTY(QString artists READ artists NOTIFY changed)
    Q_PROPERTY(QString imageUrl READ imageUrl NOTIFY changed)
    Q_PROPERTY(int progressMs READ progressMs NOTIFY changed)
    Q_PROPERTY(int durationMs READ durationMs NOTIFY changed)
    Q_PROPERTY(QString contextUri READ contextUri NOTIFY changed)

    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY playlistsChanged)
    Q_PROPERTY(bool playlistsHasMore READ playlistsHasMore NOTIFY playlistsChanged)
    Q_PROPERTY(bool playlistsLoading READ playlistsLoading NOTIFY playlistsChanged)
    Q_PROPERTY(bool pollingEnabled READ pollingEnabled WRITE setPollingEnabled NOTIFY changed)

public:
    SpotifyPlayer(Settings& settings, cadence::spotify::SpotifyAuth& auth,
                  cadence::spotify::SpotifyClient& client, QObject* parent = nullptr);
    ~SpotifyPlayer() override;

    static SpotifyPlayer* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static void setInstance(SpotifyPlayer* instance);

    // At startup: hands the client id to the auth layer and restores a stored login.
    void restore();

    bool configured() const;
    bool connected() const;
    bool connecting() const;
    bool online() const;
    QString statusText() const;
    QString errorText() const;
    QString displayName() const { return displayName_; }
    bool premium() const { return product_ == u"premium"; }
    // Only when the product is known and not premium.
    bool premiumWarning() const { return !product_.isEmpty() && product_ != u"premium"; }
    bool needsReconsent() const;
    QString redirectUri() const;
    int redirectPort() const { return cadence::spotify::redirectPort; }

    bool hasActiveDevice() const { return state_.active && state_.device.has_value(); }
    bool canTransferHere() const { return !hasActiveDevice() && !thisDeviceId_.isEmpty(); }
    QString deviceName() const;
    bool isPlaying() const { return state_.active && state_.isPlaying; }
    QString trackName() const;
    QString artists() const;
    QString imageUrl() const;
    int progressMs() const { return state_.progressMs; }
    int durationMs() const { return state_.track ? state_.track->durationMs : 0; }
    QString contextUri() const { return QString::fromStdString(state_.contextUri); }

    QVariantList playlists() const { return playlists_; }
    bool playlistsHasMore() const { return playlistsHasMore_; }
    bool playlistsLoading() const { return playlistsLoading_; }
    bool pollingEnabled() const { return pollingEnabled_; }
    void setPollingEnabled(bool enabled);

    Q_INVOKABLE void connectAccount();
    Q_INVOKABLE void disconnectAccount();
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void playPlaylist(const QString& uri);
    Q_INVOKABLE void transferHere();
    Q_INVOKABLE void loadPlaylists(bool reset);
    Q_INVOKABLE void refreshNow();
    Q_INVOKABLE void copyToClipboard(const QString& text);

public slots:
    void onBlockStarted(int blockIndex, const QString& activityId);

signals:
    void changed();
    void playlistsChanged();
    void logMessage(const QString& line);

private:
    void onAuthStateChanged();
    void fetchProfile();
    void poll();
    void applyPollingState();
    void handleCommandResult(const QString& what, const cadence::spotify::Classification& result,
                             bool networkFailure);
    void pickThisDevice(const std::vector<cadence::spotify::Device>& devices);
    void setError(const QString& text);

    Settings& settings_;
    cadence::spotify::SpotifyAuth& auth_;
    cadence::spotify::SpotifyClient& client_;
    QTimer pollTimer_;
    bool pollingEnabled_ = false;
    QString displayName_;
    QString product_;
    QString error_;
    cadence::spotify::PlayerState state_;
    QString thisDeviceId_;
    QString thisDeviceName_;
    QVariantList playlists_;
    int playlistsOffset_ = 0;
    bool playlistsHasMore_ = false;
    bool playlistsLoading_ = false;
    int polls_ = 0;
};
