#include "spotifyplayer.hpp"

#include "settings.hpp"

#include <cadence/spotify/auth.hpp>
#include <cadence/spotify/client.hpp>

#include <QClipboard>
#include <QGuiApplication>
#include <QSysInfo>
#include <QVariantMap>

using namespace Qt::StringLiterals;
using namespace cadence::spotify;

namespace {

SpotifyPlayer* s_instance = nullptr;

constexpr int pollIntervalMs = 3000;
constexpr int playlistPageSize = 20;

} // namespace

SpotifyPlayer::SpotifyPlayer(Settings& settings, SpotifyAuth& auth, SpotifyClient& client, QObject* parent)
    : QObject(parent), settings_(settings), auth_(auth), client_(client) {
    pollTimer_.setInterval(pollIntervalMs);
    connect(&pollTimer_, &QTimer::timeout, this, &SpotifyPlayer::poll);
    connect(&auth_, &SpotifyAuth::stateChanged, this, &SpotifyPlayer::onAuthStateChanged);
    connect(&auth_, &SpotifyAuth::errorChanged, this, &SpotifyPlayer::changed);
    connect(&auth_, &SpotifyAuth::needsReconsentChanged, this, &SpotifyPlayer::changed);
    connect(&client_, &SpotifyClient::onlineChanged, this, &SpotifyPlayer::changed);
    connect(&settings_, &Settings::changed, this, [this] {
        if (auth_.clientId() != settings_.spotifyClientId()) {
            auth_.setClientId(settings_.spotifyClientId());
        }
        emit changed();
    });
}

SpotifyPlayer::~SpotifyPlayer() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

SpotifyPlayer* SpotifyPlayer::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine) {
    Q_UNUSED(qmlEngine);
    Q_ASSERT(s_instance != nullptr);
    Q_ASSERT(jsEngine->thread() == s_instance->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void SpotifyPlayer::setInstance(SpotifyPlayer* instance) {
    s_instance = instance;
}

void SpotifyPlayer::restore() {
    auth_.setClientId(settings_.spotifyClientId());
    if (!settings_.spotifyClientId().isEmpty()) {
        auth_.restore();
    }
}

bool SpotifyPlayer::configured() const {
    return !settings_.spotifyClientId().isEmpty();
}

bool SpotifyPlayer::connected() const {
    return auth_.state() == SpotifyAuth::State::Connected;
}

bool SpotifyPlayer::connecting() const {
    return auth_.state() == SpotifyAuth::State::Connecting;
}

bool SpotifyPlayer::online() const {
    return client_.online();
}

bool SpotifyPlayer::needsReconsent() const {
    return auth_.needsReconsent();
}

QString SpotifyPlayer::redirectUri() const {
    return SpotifyAuth::redirectUri();
}

QString SpotifyPlayer::statusText() const {
    switch (auth_.state()) {
    case SpotifyAuth::State::Disconnected:
        return configured() ? u"Not connected"_s : u"Paste your Client ID to begin"_s;
    case SpotifyAuth::State::Connecting:
        return u"Connecting"_s;
    case SpotifyAuth::State::Connected:
        if (displayName_.isEmpty()) {
            return u"Connected"_s;
        }
        return premium() ? u"Connected as %1 · Premium"_s.arg(displayName_)
                         : u"Connected as %1"_s.arg(displayName_);
    case SpotifyAuth::State::Error:
        return u"Connection problem"_s;
    }
    return {};
}

QString SpotifyPlayer::errorText() const {
    if (!auth_.error().isEmpty()) {
        return auth_.error();
    }
    if (connected() && !client_.online()) {
        return u"Spotify is unreachable right now"_s;
    }
    return error_;
}

QString SpotifyPlayer::deviceName() const {
    return state_.device ? QString::fromStdString(state_.device->name) : QString();
}

QString SpotifyPlayer::trackName() const {
    return state_.track ? QString::fromStdString(state_.track->name) : QString();
}

QString SpotifyPlayer::artists() const {
    return state_.track ? QString::fromStdString(state_.track->artists) : QString();
}

QString SpotifyPlayer::imageUrl() const {
    return state_.track ? QString::fromStdString(state_.track->imageUrl) : QString();
}

void SpotifyPlayer::setError(const QString& text) {
    if (error_ == text) {
        return;
    }
    error_ = text;
    emit changed();
}

void SpotifyPlayer::onAuthStateChanged() {
    if (connected()) {
        fetchProfile();
    } else if (auth_.state() == SpotifyAuth::State::Disconnected) {
        displayName_.clear();
        product_.clear();
        state_ = PlayerState{};
        playlists_.clear();
        playlistsHasMore_ = false;
        emit playlistsChanged();
    }
    applyPollingState();
    emit changed();
}

void SpotifyPlayer::fetchProfile() {
    client_.fetchProfile([this](std::optional<Profile> profile, const SpotifyClient::Response& response) {
        if (profile) {
            displayName_ = QString::fromStdString(profile->displayName);
            product_ = profile->product ? QString::fromStdString(*profile->product) : QString();
            setError({});
            emit logMessage(
                u"spotify profile loaded, product %1"_s.arg(product_.isEmpty() ? u"unknown"_s : product_));
        } else {
            handleCommandResult(u"profile"_s, response.classification, response.networkFailure);
        }
        emit changed();
    });
}

void SpotifyPlayer::setPollingEnabled(bool enabled) {
    if (pollingEnabled_ == enabled) {
        return;
    }
    pollingEnabled_ = enabled;
    applyPollingState();
    emit changed();
}

void SpotifyPlayer::applyPollingState() {
    const bool shouldPoll = pollingEnabled_ && connected();
    if (shouldPoll && !pollTimer_.isActive()) {
        pollTimer_.start();
        poll();
    } else if (!shouldPoll && pollTimer_.isActive()) {
        pollTimer_.stop();
    }
}

void SpotifyPlayer::poll() {
    polls_ += 1;
    client_.fetchPlayerState(
        [this](std::optional<PlayerState> state, const SpotifyClient::Response& response) {
            if (state) {
                state_ = *state;
                if (state_.active && state_.device && state_.device->type == "Computer") {
                    thisDeviceId_ = QString::fromStdString(state_.device->id);
                    thisDeviceName_ = QString::fromStdString(state_.device->name);
                }
                setError({});
            } else if (!response.networkFailure) {
                handleCommandResult(u"player"_s, response.classification, false);
            }
            emit changed();
        });
    // Without an active device, look for this computer so the transfer offer can appear.
    if (!hasActiveDevice() && (polls_ % 3 == 1 || thisDeviceId_.isEmpty())) {
        client_.fetchDevices([this](std::vector<Device> devices, const SpotifyClient::Response&) {
            pickThisDevice(devices);
            emit changed();
        });
    }
}

// The device to offer for a transfer: the computer whose name matches this machine, else the
// first computer listed.
void SpotifyPlayer::pickThisDevice(const std::vector<Device>& devices) {
    const QString host = QSysInfo::machineHostName();
    QString firstComputerId;
    QString firstComputerName;
    for (const Device& device : devices) {
        if (device.type != "Computer") {
            continue;
        }
        const QString name = QString::fromStdString(device.name);
        if (name.compare(host, Qt::CaseInsensitive) == 0) {
            thisDeviceId_ = QString::fromStdString(device.id);
            thisDeviceName_ = name;
            return;
        }
        if (firstComputerId.isEmpty()) {
            firstComputerId = QString::fromStdString(device.id);
            firstComputerName = name;
        }
    }
    thisDeviceId_ = firstComputerId;
    thisDeviceName_ = firstComputerName;
}

void SpotifyPlayer::handleCommandResult(const QString& what, const Classification& result,
                                        bool networkFailure) {
    if (networkFailure) {
        // The client already flipped online; the status line says unreachable.
        emit changed();
        return;
    }
    switch (result.outcome) {
    case Outcome::Ok:
    case Outcome::NothingToPause:
        setError({});
        break;
    case Outcome::NoActiveDevice:
        setError(u"Open Spotify on this PC"_s);
        break;
    case Outcome::PremiumRequired:
        setError(u"Spotify Premium is required to control playback"_s);
        break;
    case Outcome::Unauthorized:
        setError(u"Spotify signed you out. Connect again."_s);
        break;
    case Outcome::RateLimited:
        setError(u"Spotify asked to slow down"_s);
        break;
    case Outcome::Forbidden:
        setError(u"Spotify refused that command"_s);
        break;
    case Outcome::NotFound:
    case Outcome::ServerError:
    case Outcome::Other:
        setError(u"Spotify error on %1: %2"_s.arg(what, QString::fromStdString(result.message)));
        break;
    }
    if (result.outcome != Outcome::Ok && result.outcome != Outcome::NothingToPause) {
        emit logMessage(u"spotify %1 failed: %2"_s.arg(what, error_));
    }
}

void SpotifyPlayer::connectAccount() {
    auth_.setClientId(settings_.spotifyClientId());
    auth_.connectAccount();
    emit changed();
}

void SpotifyPlayer::disconnectAccount() {
    auth_.disconnectAccount();
}

void SpotifyPlayer::playPause() {
    if (!connected()) {
        return;
    }
    if (isPlaying()) {
        client_.pause([this](const SpotifyClient::Response& response) {
            handleCommandResult(u"pause"_s, response.classification, response.networkFailure);
            poll();
        });
    } else {
        const QString device = hasActiveDevice() ? QString() : thisDeviceId_;
        client_.play({}, device, [this](const SpotifyClient::Response& response) {
            handleCommandResult(u"play"_s, response.classification, response.networkFailure);
            poll();
        });
    }
}

void SpotifyPlayer::next() {
    if (!connected()) {
        return;
    }
    client_.next([this](const SpotifyClient::Response& response) {
        handleCommandResult(u"next"_s, response.classification, response.networkFailure);
        poll();
    });
}

void SpotifyPlayer::previous() {
    if (!connected()) {
        return;
    }
    client_.previous([this](const SpotifyClient::Response& response) {
        handleCommandResult(u"previous"_s, response.classification, response.networkFailure);
        poll();
    });
}

void SpotifyPlayer::playPlaylist(const QString& uri) {
    if (!connected() || uri.isEmpty()) {
        return;
    }
    // The device id always goes along: it makes the one retry after an activation lag possible.
    const QString device = state_.device ? QString::fromStdString(state_.device->id) : thisDeviceId_;
    emit logMessage(u"spotify play playlist"_s);
    client_.play(uri, device, [this](const SpotifyClient::Response& response) {
        handleCommandResult(u"play playlist"_s, response.classification, response.networkFailure);
        poll();
    });
}

void SpotifyPlayer::transferHere() {
    if (!connected() || thisDeviceId_.isEmpty()) {
        return;
    }
    emit logMessage(u"spotify transfer to this computer"_s);
    client_.transfer(thisDeviceId_, false, [this](const SpotifyClient::Response& response) {
        handleCommandResult(u"transfer"_s, response.classification, response.networkFailure);
        poll();
    });
}

void SpotifyPlayer::loadPlaylists(bool reset) {
    if (!connected() || playlistsLoading_) {
        return;
    }
    if (reset) {
        playlists_.clear();
        playlistsOffset_ = 0;
        playlistsHasMore_ = false;
    }
    playlistsLoading_ = true;
    emit playlistsChanged();
    client_.fetchPlaylists(playlistsOffset_, playlistPageSize,
                           [this](std::optional<PlaylistPage> page, const SpotifyClient::Response& response) {
                               playlistsLoading_ = false;
                               if (page) {
                                   for (const Playlist& playlist : page->items) {
                                       QVariantMap row;
                                       row[u"uri"_s] = QString::fromStdString(playlist.uri);
                                       row[u"name"_s] = QString::fromStdString(playlist.name);
                                       row[u"owner"_s] = QString::fromStdString(playlist.ownerName);
                                       row[u"imageUrl"_s] = QString::fromStdString(playlist.imageUrl);
                                       playlists_.push_back(row);
                                   }
                                   playlistsOffset_ += static_cast<int>(page->items.size());
                                   playlistsHasMore_ = page->hasNext && !page->items.empty();
                               } else {
                                   handleCommandResult(u"playlists"_s, response.classification,
                                                       response.networkFailure);
                               }
                               emit playlistsChanged();
                           });
}

void SpotifyPlayer::refreshNow() {
    if (connected()) {
        poll();
    }
}

void SpotifyPlayer::copyToClipboard(const QString& text) {
    QGuiApplication::clipboard()->setText(text);
}

// Music never starts on its own unless the setting says so and the activity has a playlist.
void SpotifyPlayer::onBlockStarted(int blockIndex, const QString& activityId) {
    Q_UNUSED(blockIndex);
    if (!settings_.spotifyAutoplay() || !connected()) {
        return;
    }
    const QString uri = settings_.playlistUriFor(activityId);
    if (uri.isEmpty()) {
        return;
    }
    emit logMessage(u"spotify autoplay for activity %1"_s.arg(activityId));
    playPlaylist(uri);
}
