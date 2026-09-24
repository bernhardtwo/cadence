#include <cadence/spotify/auth.hpp>
#include <cadence/spotify/client.hpp>

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <utility>

using namespace Qt::StringLiterals;

namespace cadence::spotify {

namespace {

const QString apiBase = u"https://api.spotify.com/v1"_s;
constexpr int deviceRetryDelayMs = 1500;
constexpr int maxRateLimitRetries = 2;

template <typename T>
std::optional<T> parseOr(const SpotifyClient::Response& response, T (*parser)(std::string_view)) {
    if (response.classification.outcome != Outcome::Ok) {
        return std::nullopt;
    }
    try {
        return parser(
            std::string_view(response.body.constData(), static_cast<std::size_t>(response.body.size())));
    } catch (const ParseError&) {
        return std::nullopt;
    }
}

} // namespace

SpotifyClient::SpotifyClient(SpotifyAuth& auth, QObject* parent) : QObject(parent), auth_(auth) {}

void SpotifyClient::setOnline(bool online) {
    if (online_ == online) {
        return;
    }
    online_ = online;
    emit onlineChanged();
    emit logMessage(online ? u"spotify reachable again"_s : u"spotify unreachable"_s);
}

void SpotifyClient::send(Call call) {
    auth_.ensureFreshToken([this, call = std::move(call)](bool ok) mutable {
        if (!ok) {
            Response response;
            response.status = 401;
            response.classification.outcome = Outcome::Unauthorized;
            response.classification.message = u"not connected"_s.toStdString();
            call.done(response);
            return;
        }
        dispatch(std::move(call));
    });
}

void SpotifyClient::dispatch(Call call) {
    QNetworkRequest request{QUrl(apiBase + call.path)};
    request.setRawHeader("Authorization", "Bearer " + auth_.accessToken().toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
    request.setTransferTimeout(10000);
    QNetworkReply* reply = network_.sendCustomRequest(request, call.verb, call.body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, call = std::move(call)]() mutable {
        reply->deleteLater();
        Response response;
        response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response.body = reply->readAll();
        if (response.status == 0) {
            response.networkFailure = true;
            response.classification.outcome = Outcome::Other;
            response.classification.message = reply->errorString().toStdString();
            setOnline(false);
            call.done(response);
            return;
        }
        setOnline(true);
        std::optional<std::string> retryAfter;
        if (reply->hasRawHeader("Retry-After")) {
            retryAfter = reply->rawHeader("Retry-After").toStdString();
        }
        response.classification = classify(
            call.command, response.status,
            std::string_view(response.body.constData(), static_cast<std::size_t>(response.body.size())),
            retryAfter ? std::optional<std::string_view>(*retryAfter) : std::nullopt);

        const Outcome outcome = response.classification.outcome;
        if (outcome == Outcome::Unauthorized && call.refreshes == 0) {
            call.refreshes = 1;
            auth_.refreshNow([this, call = std::move(call)](bool refreshed) mutable {
                if (refreshed) {
                    dispatch(std::move(call));
                } else {
                    Response failed;
                    failed.status = 401;
                    failed.classification.outcome = Outcome::Unauthorized;
                    call.done(failed);
                }
            });
            return;
        }
        if (outcome == Outcome::RateLimited && call.rateLimitRetries < maxRateLimitRetries) {
            call.rateLimitRetries += 1;
            const int delay = response.classification.retryAfterSeconds * 1000;
            emit logMessage(u"spotify rate limited, retrying in %1 s"_s.arg(delay / 1000));
            QTimer::singleShot(delay, this,
                               [this, call = std::move(call)]() mutable { dispatch(std::move(call)); });
            return;
        }
        if (outcome == Outcome::NoActiveDevice && call.command == Command::Play && call.deviceRetries == 0 &&
            call.path.contains(u"device_id="_s)) {
            // Activation after a transfer is not instant; the device id is already on the call.
            call.deviceRetries = 1;
            QTimer::singleShot(deviceRetryDelayMs, this,
                               [this, call = std::move(call)]() mutable { dispatch(std::move(call)); });
            return;
        }
        call.done(response);
    });
}

void SpotifyClient::fetchProfile(std::function<void(std::optional<Profile>, const Response&)> done) {
    send(Call{Command::Read, "GET", u"/me"_s, {}, [done = std::move(done)](const Response& response) {
                  done(parseOr(response, &parseProfile), response);
              }});
}

void SpotifyClient::fetchPlayerState(std::function<void(std::optional<PlayerState>, const Response&)> done) {
    send(Call{Command::Read, "GET", u"/me/player"_s, {}, [done = std::move(done)](const Response& response) {
                  done(parseOr(response, &parsePlayerState), response);
              }});
}

void SpotifyClient::fetchDevices(std::function<void(std::vector<Device>, const Response&)> done) {
    send(Call{Command::Read,
              "GET",
              u"/me/player/devices"_s,
              {},
              [done = std::move(done)](const Response& response) {
                  done(parseOr(response, &parseDevices).value_or(std::vector<Device>{}), response);
              }});
}

void SpotifyClient::fetchPlaylists(int offset, int limit,
                                   std::function<void(std::optional<PlaylistPage>, const Response&)> done) {
    send(Call{Command::Read,
              "GET",
              u"/me/playlists?offset=%1&limit=%2"_s.arg(offset).arg(limit),
              {},
              [done = std::move(done)](const Response& response) {
                  done(parseOr(response, &parsePlaylistPage), response);
              }});
}

void SpotifyClient::play(const QString& contextUri, const QString& deviceId, Done done) {
    QString path = u"/me/player/play"_s;
    if (!deviceId.isEmpty()) {
        path += u"?device_id="_s + deviceId;
    }
    QByteArray body;
    if (!contextUri.isEmpty()) {
        body = QByteArray("{\"context_uri\":\"") + contextUri.toUtf8() + "\"}";
    }
    send(Call{Command::Play, "PUT", path, body, std::move(done)});
}

void SpotifyClient::pause(Done done) {
    send(Call{Command::Pause, "PUT", u"/me/player/pause"_s, {}, std::move(done)});
}

void SpotifyClient::next(Done done) {
    send(Call{Command::Next, "POST", u"/me/player/next"_s, {}, std::move(done)});
}

void SpotifyClient::previous(Done done) {
    send(Call{Command::Previous, "POST", u"/me/player/previous"_s, {}, std::move(done)});
}

void SpotifyClient::setVolume(int percent, Done done) {
    send(Call{Command::Volume,
              "PUT",
              u"/me/player/volume?volume_percent=%1"_s.arg(std::clamp(percent, 0, 100)),
              {},
              std::move(done)});
}

void SpotifyClient::transfer(const QString& deviceId, bool startPlaying, Done done) {
    const QByteArray body = QByteArray("{\"device_ids\":[\"") + deviceId.toUtf8() +
                            "\"],\"play\":" + (startPlaying ? "true" : "false") + "}";
    send(Call{Command::Transfer, "PUT", u"/me/player"_s, body, std::move(done)});
}

} // namespace cadence::spotify
