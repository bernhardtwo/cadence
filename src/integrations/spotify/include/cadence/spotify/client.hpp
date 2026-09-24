#pragma once

#include <cadence/spotify/api.hpp>

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

namespace cadence::spotify {

class SpotifyAuth;

// The Web API calls Cadence makes, each answered through a callback on the main thread. A 401
// gets one token refresh and one retry, a 429 waits for Retry-After and retries, a play that hits
// NO_ACTIVE_DEVICE retries once after a short wait, and a network failure is reported as such
// without ever blocking the caller.
class SpotifyClient : public QObject {
    Q_OBJECT

public:
    struct Response {
        int status = 0;
        QByteArray body;
        Classification classification;
        // True when the request never reached Spotify.
        bool networkFailure = false;
    };

    using Done = std::function<void(const Response&)>;

    explicit SpotifyClient(SpotifyAuth& auth, QObject* parent = nullptr);

    bool online() const { return online_; }

    void fetchProfile(std::function<void(std::optional<Profile>, const Response&)> done);
    void fetchPlayerState(std::function<void(std::optional<PlayerState>, const Response&)> done);
    void fetchDevices(std::function<void(std::vector<Device>, const Response&)> done);
    void fetchPlaylists(int offset, int limit,
                        std::function<void(std::optional<PlaylistPage>, const Response&)> done);

    // contextUri empty resumes whatever is current. deviceId empty targets the active device.
    void play(const QString& contextUri, const QString& deviceId, Done done);
    void pause(Done done);
    void next(Done done);
    void previous(Done done);
    void setVolume(int percent, Done done);
    void transfer(const QString& deviceId, bool startPlaying, Done done);

signals:
    void onlineChanged();
    // Token free, for the event log.
    void logMessage(const QString& line);

private:
    struct Call {
        Command command;
        QByteArray verb;
        QString path;
        QByteArray body;
        Done done;
        int refreshes = 0;
        int rateLimitRetries = 0;
        int deviceRetries = 0;
    };

    void send(Call call);
    void dispatch(Call call);
    void setOnline(bool online);

    SpotifyAuth& auth_;
    QNetworkAccessManager network_;
    bool online_ = true;
};

} // namespace cadence::spotify
