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

## Responsive layout below the minimum window size

The main window refuses to shrink below 1024 by 520, which fits a 1366 by 768 screen at 125
percent scaling with the taskbar showing (1093 by 614 logical, about 566 left for the window and
39 of that for its frame). What the screens do at that size:

- Templates: the day fields wrap under the two buttons, the day pills narrow to 44 and wrap
  below that, and the block list scrolls with one and a half rows in view. The width floor is
  the two French buttons, 484 wide, next to the 420 block panel and the margins.
- Settings: the music card stacks under the left column below 1080 and the page scrolls.
- Today: the side column scrolls, the block timer is capped at 60 percent of the bar and the
  block name shrinks to 56 before it elides.
- Stats and focus mode have nothing to wrap.

Going lower needs a redesign: the Templates panel would have to collapse or float, and the
top bar would have to elide the summary, which is a `TimerText` with fixed digit cells. The
summary already touches the navigation pills at 1024 in French with a block name of about
thirty characters.

## Clean shutdown from outside

The single instance socket only understands `show`. Add a `quit` command (and a `--quit` flag that
sends it) so a deployment script can close the running copy cleanly instead of terminating the
process. Progress is persisted on every action, so termination loses nothing today, but a
cooperative shutdown is the right tool once there is state that is not written immediately.
Until then `scripts/deploy-windows.ps1` terminates the process and, after the relaunch, polls
until exactly one process runs from the target folder for three consecutive seconds, so a slow
start or a second instance handing over to the first cannot pass as a healthy deploy.

## System media controls fallback

Cadence drives Spotify through the Web API only. When the account is not Premium, or Spotify is
unreachable, the OS media session could still control whatever plays locally: SMTC on Windows,
MPRIS on Linux, the media remote command center on macOS. Play, pause, next and previous would
work for any player, with no track metadata or playlists. Out of scope for the music milestone.
