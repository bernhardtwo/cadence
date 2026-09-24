#include <cadence/spotify/api.hpp>
#include <cadence/spotify/auth.hpp>
#include <cadence/spotify/pkce.hpp>

#include <QDesktopServices>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QRandomGenerator>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <qtkeychain/keychain.h>

#include <array>
#include <cstdint>
#include <utility>

using namespace Qt::StringLiterals;

namespace cadence::spotify {

namespace {

const QString keychainService = u"Cadence"_s;
const QString keychainKey = u"spotify"_s;
constexpr int refreshLeadSeconds = 60;

QString joinScopes(const std::set<std::string>& scopes) {
    QStringList list;
    for (const std::string& scope : scopes) {
        list.push_back(QString::fromStdString(scope));
    }
    return list.join(u' ');
}

} // namespace

SpotifyAuth::SpotifyAuth(QObject* parent)
    : QObject(parent), flow_(new QOAuth2AuthorizationCodeFlow(this)), refreshTimer_(new QTimer(this)) {
    refreshTimer_->setSingleShot(true);
    connect(refreshTimer_, &QTimer::timeout, this, [this] {
        if (state_ == State::Connected) {
            refreshNow([](bool) {});
        }
    });
    configureFlow();
}

SpotifyAuth::~SpotifyAuth() = default;

QString SpotifyAuth::redirectUri() {
    return QString::fromUtf8(cadence::spotify::redirectUri.data(),
                             static_cast<qsizetype>(cadence::spotify::redirectUri.size()));
}

void SpotifyAuth::configureFlow() {
    flow_->setAuthorizationUrl(QUrl(u"https://accounts.spotify.com/authorize"_s));
    flow_->setTokenUrl(QUrl(u"https://accounts.spotify.com/api/token"_s));
    QSet<QByteArray> scopes;
    for (const std::string& scope : requiredScopes()) {
        scopes.insert(QByteArray::fromStdString(scope));
    }
    flow_->setRequestedScopeTokens(scopes);
    // PKCE is done by hand with the tested helpers rather than by Qt, so the same code path is
    // what the RFC vector checks.
    flow_->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::None);
    flow_->setModifyParametersFunction(
        [this](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant>* parameters) {
            if (stage == QAbstractOAuth::Stage::RequestingAuthorization) {
                parameters->insert(u"code_challenge_method"_s, u"S256"_s);
                parameters->insert(u"code_challenge"_s, QString::fromStdString(codeChallenge(verifier_)));
            } else if (stage == QAbstractOAuth::Stage::RequestingAccessToken) {
                parameters->insert(u"code_verifier"_s, QString::fromStdString(verifier_));
            }
        });

    connect(flow_, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, this, [this](const QUrl& url) {
        emit logMessage(u"spotify opening browser for login"_s);
        QDesktopServices::openUrl(url);
    });
    connect(flow_, &QAbstractOAuth::granted, this, &SpotifyAuth::onGranted);
    connect(flow_, &QAbstractOAuth2::serverReportedErrorOccurred, this,
            [this](const QString& error, const QString& description, const QUrl&) {
                onFailed(description.isEmpty() ? error : error + u": "_s + description);
            });
    connect(flow_, &QAbstractOAuth::requestFailed, this, [this](QAbstractOAuth::Error error) {
        switch (error) {
        case QAbstractOAuth::Error::NetworkError:
            onFailed(u"network error while talking to Spotify"_s);
            break;
        case QAbstractOAuth::Error::ServerError:
            onFailed(u"Spotify refused the request"_s);
            break;
        case QAbstractOAuth::Error::OAuthTokenNotFoundError:
        case QAbstractOAuth::Error::OAuthTokenSecretNotFoundError:
        case QAbstractOAuth::Error::OAuthCallbackNotVerified:
        case QAbstractOAuth::Error::NoError:
        case QAbstractOAuth::Error::ClientError:
        case QAbstractOAuth::Error::ExpiredError:
            onFailed(u"authorization failed"_s);
            break;
        }
    });
}

void SpotifyAuth::setClientId(const QString& clientId) {
    clientId_ = clientId.trimmed();
    flow_->setClientIdentifier(clientId_);
}

QString SpotifyAuth::accessToken() const {
    return flow_->token();
}

QDateTime SpotifyAuth::expiresAt() const {
    return flow_->expirationAt();
}

void SpotifyAuth::setState(State state) {
    if (state_ == state) {
        return;
    }
    state_ = state;
    emit stateChanged();
}

void SpotifyAuth::setError(const QString& error) {
    if (error_ == error) {
        return;
    }
    error_ = error;
    emit errorChanged();
}

void SpotifyAuth::setNeedsReconsent(bool needs) {
    if (needsReconsent_ == needs) {
        return;
    }
    needsReconsent_ = needs;
    emit needsReconsentChanged();
}

void SpotifyAuth::connectAccount() {
    if (clientId_.isEmpty()) {
        setError(u"Paste the Client ID of your Spotify app first"_s);
        setState(State::Error);
        return;
    }
    if (state_ == State::Connecting) {
        return;
    }
    delete handler_;
    handler_ = new QOAuthHttpServerReplyHandler(QHostAddress(u"127.0.0.1"_s), redirectPort, this);
    handler_->setCallbackPath(u"/callback"_s);
    handler_->setCallbackText(u"Cadence is connected to Spotify. You can close this tab."_s);
    if (!handler_->isListening()) {
        delete handler_;
        handler_ = nullptr;
        setError(u"Port %1 is in use by another program. Close it and try again."_s.arg(redirectPort));
        setState(State::Error);
        emit logMessage(u"spotify connect failed: redirect port busy"_s);
        return;
    }
    flow_->setReplyHandler(handler_);

    std::array<std::uint8_t, 32> random{};
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(random.data()),
                                          random.size() / sizeof(quint32));
    verifier_ = codeVerifier(random);

    setError({});
    interactive_ = true;
    setState(State::Connecting);
    emit logMessage(u"spotify connect started"_s);
    flow_->grant();
}

void SpotifyAuth::disconnectAccount() {
    refreshTimer_->stop();
    flow_->setToken({});
    flow_->setRefreshToken({});
    grantedScopes_.clear();
    interactive_ = false;
    clearCredentials();
    setNeedsReconsent(false);
    setError({});
    setState(State::Disconnected);
    emit logMessage(u"spotify disconnected"_s);
    finishPending(false);
}

void SpotifyAuth::restore() {
    auto* job = new QKeychain::ReadPasswordJob(keychainService, this);
    job->setKey(keychainKey);
    job->setAutoDelete(true);
    connect(job, &QKeychain::Job::finished, this, [this, job] {
        if (job->error() != QKeychain::NoError) {
            if (job->error() != QKeychain::EntryNotFound) {
                emit logMessage(u"spotify credential store read failed: "_s + job->errorString());
            }
            setState(State::Disconnected);
            return;
        }
        const QJsonObject stored = QJsonDocument::fromJson(job->textData().toUtf8()).object();
        const QString refreshToken = stored.value(u"refresh_token"_s).toString();
        if (refreshToken.isEmpty() || clientId_.isEmpty()) {
            setState(State::Disconnected);
            return;
        }
        grantedScopes_ = parseScopes(stored.value(u"scope"_s).toString().toStdString());
        setNeedsReconsent(!missingScopes(grantedScopes_).empty());
        flow_->setRefreshToken(refreshToken);
        interactive_ = false;
        setState(State::Connecting);
        emit logMessage(u"spotify restoring session"_s);
        flow_->refreshTokens();
    });
    job->start();
}

void SpotifyAuth::ensureFreshToken(std::function<void(bool)> done) {
    if (state_ != State::Connected && state_ != State::Connecting) {
        done(false);
        return;
    }
    const QDateTime expiry = flow_->expirationAt();
    const bool fresh = !flow_->token().isEmpty() && expiry.isValid() &&
                       QDateTime::currentDateTime().secsTo(expiry) > refreshLeadSeconds;
    if (fresh && state_ == State::Connected) {
        done(true);
        return;
    }
    refreshNow(std::move(done));
}

void SpotifyAuth::refreshNow(std::function<void(bool)> done) {
    pending_.push_back(std::move(done));
    if (state_ == State::Connecting) {
        return;
    }
    if (flow_->refreshToken().isEmpty()) {
        finishPending(false);
        return;
    }
    interactive_ = false;
    setState(State::Connecting);
    flow_->refreshTokens();
}

void SpotifyAuth::finishPending(bool ok) {
    const auto callbacks = std::move(pending_);
    pending_.clear();
    for (const auto& callback : callbacks) {
        callback(ok);
    }
}

// Qt emits granted for the first login and for every refresh alike; only the first is a login.
void SpotifyAuth::onGranted() {
    const bool login = interactive_;
    interactive_ = false;
    if (login) {
        const auto tokens = flow_->grantedScopeTokens();
        grantedScopes_.clear();
        for (const QByteArray& token : tokens) {
            grantedScopes_.insert(token.toStdString());
        }
        setNeedsReconsent(!missingScopes(grantedScopes_).empty());
        delete handler_;
        handler_ = nullptr;
        emit logMessage(u"spotify connected"_s);
    } else {
        emit logMessage(u"spotify token refreshed"_s);
    }
    if (!flow_->refreshToken().isEmpty()) {
        storeCredentials();
    }
    setError({});
    setState(State::Connected);
    scheduleRefresh();
    finishPending(true);
}

void SpotifyAuth::onFailed(const QString& why) {
    const bool login = interactive_;
    interactive_ = false;
    delete handler_;
    handler_ = nullptr;
    setError(why);
    emit logMessage((login ? u"spotify connect failed: "_s : u"spotify refresh failed: "_s) + why);
    // A refresh that fails keeps the stored grant; the next attempt may succeed. A failed login
    // leaves nothing to keep.
    if (login) {
        flow_->setToken({});
        flow_->setRefreshToken({});
        setState(State::Disconnected);
    } else {
        setState(flow_->refreshToken().isEmpty() ? State::Disconnected : State::Error);
    }
    finishPending(false);
}

void SpotifyAuth::scheduleRefresh() {
    const QDateTime expiry = flow_->expirationAt();
    if (!expiry.isValid()) {
        return;
    }
    const qint64 seconds = QDateTime::currentDateTime().secsTo(expiry) - refreshLeadSeconds;
    refreshTimer_->start(static_cast<int>(std::max<qint64>(5, seconds)) * 1000);
}

void SpotifyAuth::storeCredentials() {
    QJsonObject stored;
    stored[u"refresh_token"_s] = flow_->refreshToken();
    stored[u"scope"_s] = joinScopes(grantedScopes_);
    auto* job = new QKeychain::WritePasswordJob(keychainService, this);
    job->setKey(keychainKey);
    job->setAutoDelete(true);
    job->setTextData(QString::fromUtf8(QJsonDocument(stored).toJson(QJsonDocument::Compact)));
    connect(job, &QKeychain::Job::finished, this, [this, job] {
        if (job->error() != QKeychain::NoError) {
            emit logMessage(u"spotify credential store write failed: "_s + job->errorString());
            setError(u"Could not save the login in the credential store: "_s + job->errorString());
        }
    });
    job->start();
}

void SpotifyAuth::clearCredentials() {
    auto* job = new QKeychain::DeletePasswordJob(keychainService, this);
    job->setKey(keychainKey);
    job->setAutoDelete(true);
    job->start();
}

} // namespace cadence::spotify
