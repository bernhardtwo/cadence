# Spotify

Cadence controls the Spotify desktop app through the Web API (Spotify Connect). It plays no audio
itself and has no client secret anywhere. Every Cadence user registers their own Spotify app and
pastes its Client ID; that is what Spotify's Development Mode allows in 2026: the app owner needs
Premium and an app serves at most five users.

## Setup

1. Open the Spotify developer dashboard and create an app. Select the Web API.
2. Add exactly this redirect URI to the app: `http://127.0.0.1:43821/callback`. The dashboard
   rejects the form without a port, and `localhost` is not accepted by Cadence. The port is the
   constant `redirectPort` in `src/integrations/spotify/include/cadence/spotify/api.hpp`, so every
   Cadence build listens on the same address and every user can register the same URI.
3. Copy the app's Client ID. It is an identifier, not a secret, but keep it out of shared places
   all the same.
4. In Cadence, open Settings, paste the Client ID in the Music card and press Connect. The browser
   opens the Spotify login; approve the requested scopes. The card then reads "Connected as" your
   display name and "Premium" when the account has it.
5. Spotify Premium is required to control playback. The card warns when the account is not
   Premium; when Spotify does not say, Cadence relies on the 403 the API returns instead.

What Cadence stores: the Client ID and the playlist choices in `settings.json`, the refresh token
and the granted scopes in the operating system's credential store (Windows Credential Manager,
macOS Keychain, libsecret on Linux) through QtKeychain. Tokens never reach settings, progress files
or the daily log. Disconnect removes the credential store entry.

Scopes: `user-read-private`, `user-read-playback-state`, `user-modify-playback-state`,
`user-read-currently-playing`, `playlist-read-private`, `playlist-read-collaborative`. A connection
made with fewer scopes shows "Reconnect to update permissions"; one more login grants the rest.

## In the app

- Settings: the Music card above, a default playlist per activity chosen from your library, and
  the toggle "Start the activity's playlist when its block starts", off by default. Music never
  starts on its own unless that toggle is on.
- Focus mode: cover, track and artist, Prev, Play or Pause, Next, the track's progress and up to
  four playlist pills, the activity's default first, plus a picker for the whole library.
  Ctrl+Space, Ctrl+Right and Ctrl+Left drive playback while focus mode is on.
- Today: a one line "Playing" summary under the pomodoro grid.
- No active device: the app says "Open Spotify on this PC" and offers "Play here" when Spotify
  lists this computer, which transfers playback to it.

The player is polled every three seconds only while Today or focus mode is on screen.

## Behaviour learned from the API

Recorded against a Development Mode app in September 2026 and covered by fixtures in
`tests/spotify/fixtures`:

- Any 2xx is a success; several commands answer 200 where the documentation says 204.
- Starting playback right after a transfer can answer 404 `NO_ACTIVE_DEVICE`. Cadence always sends
  the device id on play and retries once after a short wait.
- Pausing while nothing plays answers 403 "Restriction violated". Cadence treats it as a no-op.
- 403 `PREMIUM_REQUIRED` becomes the Premium warning. 401 gets one token refresh and a retry. 429
  waits for `Retry-After` and retries.
- Playlist objects no longer carry a track count; the picker shows name, owner and cover.
- A token refresh emits `granted` again in Qt; it is not treated as a new login.

## Troubleshooting with the spike tool

`tools/spotify-spike` is a console program that logs in with your own Spotify app and exercises
every Web API call Cadence makes, printing one status line per call. Use it when the app reports
an error you cannot explain: it shows exactly which call fails and what Spotify answered.

It is not built by default. Configure with `-DCADENCE_BUILD_SPOTIFY_SPIKE=ON` and build the
`spotify_spike` target. The Client ID is read from the `CADENCE_SPOTIFY_CLIENT_ID` environment
variable at run time and nowhere else; tokens are never printed, only their lengths and expiry.

```bat
set CADENCE_SPOTIFY_CLIENT_ID=<your client id>
build\windows-msvc-debug\bin\spotify_spike.exe
```

It listens on `http://127.0.0.1:43821/callback`, opens the browser for login, then in order:
reads your profile, the player state, the devices and two playlists; transfers playback to the
first computer device; plays the first playlist; pauses, resumes, skips forward and back; sets the
volume to 50 percent; reads the player again; leaves playback paused; refreshes the token.

The daily log under the data directory (`logs/YYYY-MM-DD.log`) records every connect, refresh,
failure and playback command Cadence makes, without tokens.
