# Follow-ups

Work that is agreed but not started. Each item names the milestone it belongs to once it is
scheduled.

## Profile option for isolated runs

Add a `--profile <dir>` command line option, available in every build, that isolates a run
completely:

- config directory (`templates/week.json`, `settings.json`) and data directory (`progress/`) under
  the given directory,
- a single instance key derived from the profile directory, so a profile never wakes another
  running copy,
- a launch at login registration that is a no-op for profiles, so a test run can never rewrite
  the registration of the copy the user actually runs.

It replaces the debug-only `CADENCE_TEST_TEMPLATE` and `CADENCE_TEST_DATA_DIR` variables and the
test session guards that key off them (see CONTRIBUTING.md). Release builds must keep ignoring the
environment variables until the option lands; the option itself is explicit on the command line
and therefore safe in release.

Motivation: milestone 4 could not verify the release binary in isolation because release builds
ignore the test variables and share the single instance key with the deployed copy.

## Clean shutdown from outside

The single instance socket only understands `show`. Add a `quit` command (and a `--quit` flag that
sends it) so a deployment script can close the running copy cleanly instead of terminating the
process. Progress is persisted on every action, so termination loses nothing today, but a
cooperative shutdown is the right tool once there is state that is not written immediately.

## System media controls fallback

Cadence drives Spotify through the Web API only. When the account is not Premium, or Spotify is
unreachable, the OS media session could still control whatever plays locally: SMTC on Windows,
MPRIS on Linux, the media remote command center on macOS. Play, pause, next and previous would
work for any player, with no track metadata or playlists. Out of scope for the music milestone.
