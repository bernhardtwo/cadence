# Spotify

Cadence controls the Spotify desktop app through the Web API (Spotify Connect). It plays no audio
itself. This page covers how to set it up and how to troubleshoot it.

## Setup

Written with the music milestone; see the Settings screen for the same steps in short.

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

Expected answers with a Development Mode app in 2026: 200 on reads, 204 on transfer, play with a
device id and volume, 200 on pause, resume, next and previous, and 403 on pause when nothing is
playing. A 404 with `NO_ACTIVE_DEVICE` right after a transfer means the device had not activated
yet; the app waits and passes the device id, the spike does the same.
