// Spike for milestone 6: connects to Spotify with Authorization Code + PKCE on the fixed loopback
// redirect and exercises the Web API endpoints Cadence needs, printing a status line per call.
// The client id comes from CADENCE_SPOTIFY_CLIENT_ID at runtime and is never stored anywhere.
// Tokens are never printed; only their lengths and expiry.

#include <QCoreApplication>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <functional>
#include <memory>
#include <vector>

using namespace Qt::StringLiterals;

namespace {

constexpr quint16 redirectPort = 43821;
const QString redirectHost = u"127.0.0.1"_s;

QTextStream& out() {
    static QTextStream stream(stdout);
    return stream;
}

struct Step {
    QString name;
    QString verb;
    QString path;
    QByteArray body;
    // Runs on the parsed response; may record data for later steps.
    std::function<void(int, const QJsonDocument&)> then;
};

class Spike : public QObject {
public:
    explicit Spike(const QString& clientId) : flow_(new QOAuth2AuthorizationCodeFlow(this)) {
        auto* handler = new QOAuthHttpServerReplyHandler(QHostAddress(redirectHost), redirectPort, this);
        handler->setCallbackPath(u"/callback"_s);
        handler->setCallbackText(u"Cadence is connected. You can close this tab."_s);
        if (!handler->isListening()) {
            out() << "ERROR: cannot listen on " << redirectHost << ":" << redirectPort << " (port busy?)\n";
            out().flush();
            QTimer::singleShot(0, qApp, [] { QCoreApplication::exit(2); });
            return;
        }
        out() << "listening on http://" << redirectHost << ":" << redirectPort << "/callback\n";

        flow_->setAuthorizationUrl(QUrl(u"https://accounts.spotify.com/authorize"_s));
        flow_->setTokenUrl(QUrl(u"https://accounts.spotify.com/api/token"_s));
        flow_->setClientIdentifier(clientId);
        flow_->setRequestedScopeTokens({"user-read-private", "user-read-playback-state",
                                        "user-modify-playback-state", "user-read-currently-playing",
                                        "playlist-read-private", "playlist-read-collaborative"});
        flow_->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);
        flow_->setReplyHandler(handler);

        connect(flow_, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, this, [](const QUrl& url) {
            out() << "opening browser for login\n";
            out().flush();
            QDesktopServices::openUrl(url);
        });
        connect(flow_, &QAbstractOAuth::granted, this, &Spike::onGranted);
        connect(flow_, &QAbstractOAuth2::serverReportedErrorOccurred, this,
                [](const QString& error, const QString& description, const QUrl&) {
                    out() << "AUTH ERROR: " << error << " " << description << "\n";
                    out().flush();
                });
        connect(flow_, &QAbstractOAuth::requestFailed, this, [](QAbstractOAuth::Error error) {
            out() << "REQUEST FAILED: " << static_cast<int>(error) << "\n";
            out().flush();
        });
        flow_->grant();
    }

private:
    void onGranted() {
        // A token refresh emits granted again; the steps run once.
        if (!steps_.empty()) {
            return;
        }
        const auto scopes = flow_->grantedScopeTokens();
        out() << "granted: access token length " << flow_->token().size() << ", refresh token length "
              << flow_->refreshToken().size() << ", expires " << flow_->expirationAt().toString(Qt::ISODate)
              << ", scopes " << QStringList(scopes.begin(), scopes.end()).join(u" "_s) << "\n";
        out().flush();
        buildSteps();
        runNext();
    }

    void buildSteps() {
        steps_ = {
            Step{u"GET /me"_s, u"GET"_s, u"/me"_s, {},
                 [this](int, const QJsonDocument& doc) {
                     const QJsonObject me = doc.object();
                     out() << "  display_name=" << me.value(u"display_name"_s).toString()
                           << " product=" << me.value(u"product"_s).toString()
                           << " country=" << me.value(u"country"_s).toString() << "\n";
                 }},
            Step{u"GET /me/player"_s, u"GET"_s, u"/me/player"_s, {},
                 [](int status, const QJsonDocument& doc) {
                     if (status == 204) {
                         out() << "  no active device\n";
                     } else {
                         const QJsonObject state = doc.object();
                         out() << "  is_playing=" << state.value(u"is_playing"_s).toBool()
                               << " device=" << state.value(u"device"_s).toObject().value(u"name"_s).toString()
                               << " item=" << state.value(u"item"_s).toObject().value(u"name"_s).toString() << "\n";
                     }
                 }},
            Step{u"GET /me/player/devices"_s, u"GET"_s, u"/me/player/devices"_s, {},
                 [this](int, const QJsonDocument& doc) {
                     for (const QJsonValue& value : doc.object().value(u"devices"_s).toArray()) {
                         const QJsonObject device = value.toObject();
                         out() << "  device name=" << device.value(u"name"_s).toString()
                               << " type=" << device.value(u"type"_s).toString()
                               << " active=" << device.value(u"is_active"_s).toBool()
                               << " volume=" << device.value(u"volume_percent"_s).toInt() << "\n";
                         if (device.value(u"type"_s).toString() == u"Computer"_s && deviceId_.isEmpty()) {
                             deviceId_ = device.value(u"id"_s).toString();
                         }
                     }
                 }},
            Step{u"GET /me/playlists?limit=2"_s, u"GET"_s, u"/me/playlists?limit=2"_s, {},
                 [this](int, const QJsonDocument& doc) {
                     const QJsonObject page = doc.object();
                     out() << "  total=" << page.value(u"total"_s).toInt()
                           << " next=" << (page.value(u"next"_s).isNull() ? "null" : "present") << "\n";
                     for (const QJsonValue& value : page.value(u"items"_s).toArray()) {
                         const QJsonObject playlist = value.toObject();
                         out() << "  playlist name=" << playlist.value(u"name"_s).toString() << " tracks="
                               << QString::fromUtf8(QJsonDocument(playlist.value(u"tracks"_s).toObject())
                                                        .toJson(QJsonDocument::Compact))
                               << " images=" << playlist.value(u"images"_s).toArray().size()
                               << " keys=" << playlist.keys().join(u","_s) << "\n";
                         if (playlistUri_.isEmpty()) {
                             playlistUri_ = playlist.value(u"uri"_s).toString();
                         }
                     }
                 }},
        };
        // Playback steps are built after the device and playlist are known.
        steps_.push_back(Step{u"(playback steps)"_s, u"BUILD"_s, {}, {}, nullptr});
    }

    void buildPlaybackSteps() {
        if (deviceId_.isEmpty()) {
            out() << "no Computer device listed: skipping transfer, playing on the active device if any\n";
        } else {
            QJsonObject transfer;
            transfer[u"device_ids"_s] = QJsonArray{deviceId_};
            transfer[u"play"_s] = false;
            steps_.push_back(Step{u"PUT /me/player (transfer)"_s, u"PUT"_s, u"/me/player"_s,
                                  QJsonDocument(transfer).toJson(QJsonDocument::Compact), nullptr});
            steps_.push_back(Step{u"(wait 2 s)"_s, u"WAIT"_s, {}, {}, nullptr});
        }
        QJsonObject play;
        play[u"context_uri"_s] = playlistUri_;
        // The device id goes on the play call too: activation after a transfer is not instant.
        const QString playPath =
            deviceId_.isEmpty() ? u"/me/player/play"_s : u"/me/player/play?device_id="_s + deviceId_;
        steps_.push_back(Step{u"PUT /me/player/play (playlist, device_id)"_s, u"PUT"_s, playPath,
                              QJsonDocument(play).toJson(QJsonDocument::Compact), nullptr});
        steps_.push_back(Step{u"(wait 3 s)"_s, u"WAIT"_s, {}, {}, nullptr});
        steps_.push_back(Step{u"PUT /me/player/pause"_s, u"PUT"_s, u"/me/player/pause"_s, {}, nullptr});
        steps_.push_back(Step{u"(wait 2 s)"_s, u"WAIT"_s, {}, {}, nullptr});
        steps_.push_back(Step{u"PUT /me/player/play (resume)"_s, u"PUT"_s, u"/me/player/play"_s, {}, nullptr});
        steps_.push_back(Step{u"POST /me/player/next"_s, u"POST"_s, u"/me/player/next"_s, {}, nullptr});
        steps_.push_back(Step{u"(wait 2 s)"_s, u"WAIT"_s, {}, {}, nullptr});
        steps_.push_back(Step{u"POST /me/player/previous"_s, u"POST"_s, u"/me/player/previous"_s, {}, nullptr});
        steps_.push_back(Step{u"PUT /me/player/volume?volume_percent=50"_s, u"PUT"_s,
                              u"/me/player/volume?volume_percent=50"_s, {}, nullptr});
        steps_.push_back(Step{u"(wait 2 s)"_s, u"WAIT"_s, {}, {}, nullptr});
        steps_.push_back(Step{u"GET /me/player (after)"_s, u"GET"_s, u"/me/player"_s, {},
                              [](int status, const QJsonDocument& doc) {
                                  if (status != 204) {
                                      const QJsonObject state = doc.object();
                                      out() << "  is_playing=" << state.value(u"is_playing"_s).toBool()
                                            << " progress_ms=" << state.value(u"progress_ms"_s).toInt()
                                            << " item=" << state.value(u"item"_s).toObject().value(u"name"_s).toString()
                                            << " volume="
                                            << state.value(u"device"_s).toObject().value(u"volume_percent"_s).toInt()
                                            << "\n";
                                  }
                              }});
        steps_.push_back(Step{u"PUT /me/player/pause (leave paused)"_s, u"PUT"_s, u"/me/player/pause"_s, {}, nullptr});
        steps_.push_back(Step{u"(refresh token)"_s, u"REFRESH"_s, {}, {}, nullptr});
    }

    void runNext() {
        if (next_ >= steps_.size()) {
            out() << "done\n";
            out().flush();
            QCoreApplication::exit(0);
            return;
        }
        const Step step = steps_[next_++];
        if (step.verb == u"BUILD"_s) {
            buildPlaybackSteps();
            runNext();
            return;
        }
        if (step.verb == u"WAIT"_s) {
            out() << step.name << "\n";
            out().flush();
            QTimer::singleShot(step.name.contains(u"3 s"_s) ? 3000 : 2000, this, &Spike::runNext);
            return;
        }
        if (step.verb == u"REFRESH"_s) {
            const QString before = flow_->token();
            connect(flow_, &QAbstractOAuth2::tokenChanged, this, [this, before](const QString& token) {
                out() << "refresh: new access token " << (token != before ? "differs" : "same")
                      << ", refresh token length " << flow_->refreshToken().size() << ", expires "
                      << flow_->expirationAt().toString(Qt::ISODate) << "\n";
                out().flush();
                runNext();
            }, Qt::SingleShotConnection);
            flow_->refreshTokens();
            return;
        }
        QNetworkRequest request(QUrl(u"https://api.spotify.com/v1"_s + step.path));
        request.setRawHeader("Authorization", "Bearer " + flow_->token().toUtf8());
        request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
        QNetworkReply* reply = nullptr;
        if (step.verb == u"GET"_s) {
            reply = network_.get(request);
        } else if (step.verb == u"PUT"_s) {
            reply = network_.put(request, step.body);
        } else {
            reply = network_.post(request, step.body);
        }
        connect(reply, &QNetworkReply::finished, this, [this, reply, step] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            out() << step.name << " -> " << status;
            if (reply->error() != QNetworkReply::NoError && status == 0) {
                out() << " network error: " << reply->errorString();
            }
            if (status >= 400) {
                out() << " body: " << QString::fromUtf8(body.left(300));
            }
            if (reply->hasRawHeader("Retry-After")) {
                out() << " retry-after=" << reply->rawHeader("Retry-After");
            }
            out() << "\n";
            if (step.then && status < 400) {
                step.then(status, QJsonDocument::fromJson(body));
            }
            out().flush();
            runNext();
        });
    }

    QOAuth2AuthorizationCodeFlow* flow_;
    QNetworkAccessManager network_;
    std::vector<Step> steps_;
    std::size_t next_ = 0;
    QString deviceId_;
    QString playlistUri_;
};

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    const QString clientId = QString::fromLocal8Bit(qgetenv("CADENCE_SPOTIFY_CLIENT_ID")).trimmed();
    if (clientId.isEmpty()) {
        out() << "set CADENCE_SPOTIFY_CLIENT_ID in the environment\n";
        return 1;
    }
    Spike spike(clientId);
    return QGuiApplication::exec();
}
