#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

#include <functional>
#include <set>
#include <string>
#include <vector>

class QOAuth2AuthorizationCodeFlow;
class QOAuthHttpServerReplyHandler;

namespace cadence::spotify {

// Authorization Code with PKCE against the user's own Spotify app. The refresh token lives in the
// OS credential store and never anywhere else; access tokens stay in memory. Nothing here logs a
// token, only state changes.
class SpotifyAuth : public QObject {
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        // Waiting for the browser login or for the token endpoint.
        Connecting,
        Connected,
        Error,
    };
    Q_ENUM(State)

    explicit SpotifyAuth(QObject* parent = nullptr);
    ~SpotifyAuth() override;

    QString clientId() const { return clientId_; }
    void setClientId(const QString& clientId);

    State state() const { return state_; }
    QString error() const { return error_; }
    // The stored grant lacks a scope Cadence needs now; the user has to log in again.
    bool needsReconsent() const { return needsReconsent_; }
    QString accessToken() const;
    QDateTime expiresAt() const;
    static QString redirectUri();

    // Opens the browser. Fails at once when the redirect port is busy.
    void connectAccount();
    // Forgets the tokens here and in the credential store.
    void disconnectAccount();
    // At startup: loads the refresh token from the credential store and refreshes silently.
    void restore();
    // Runs done(true) with a token good for at least a minute, refreshing first when needed.
    void ensureFreshToken(std::function<void(bool)> done);
    // One forced refresh, for a 401 the client did not expect.
    void refreshNow(std::function<void(bool)> done);

signals:
    void stateChanged();
    void errorChanged();
    void needsReconsentChanged();
    // Human readable, token free, for the event log.
    void logMessage(const QString& line);

private:
    void setState(State state);
    void setError(const QString& error);
    void setNeedsReconsent(bool needs);
    void onGranted();
    void onFailed(const QString& why);
    void configureFlow();
    void scheduleRefresh();
    void storeCredentials();
    void clearCredentials();
    void finishPending(bool ok);

    QOAuth2AuthorizationCodeFlow* flow_;
    QOAuthHttpServerReplyHandler* handler_ = nullptr;
    QString clientId_;
    State state_ = State::Disconnected;
    QString error_;
    bool needsReconsent_ = false;
    bool interactive_ = false;
    std::string verifier_;
    std::set<std::string> grantedScopes_;
    std::vector<std::function<void(bool)>> pending_;
    class QTimer* refreshTimer_;
};

} // namespace cadence::spotify
