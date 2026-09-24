import QtQuick
import QtQuick.Controls
import Cadence
import Cadence.Theme

ApplicationWindow {
    id: root

    // Set by main before load; with a tray the window hides on close and only Quit exits.
    property bool trayAvailable: false
    property int currentScreen: 0
    property bool focusMode: false
    property int visibilityBeforeFocus: Window.Windowed

    width: 1280
    height: 800
    // Fits a 1366 by 768 screen at 125 percent scaling with the taskbar showing. The width is
    // set by the French Templates buttons next to the block panel; see docs/follow-ups.md.
    minimumWidth: 1024
    minimumHeight: 520
    visible: true
    title: "Cadence"
    color: Theme.bg

    onClosing: function(close) {
        if (root.trayAvailable) {
            close.accepted = false
            root.hide()
        }
    }

    // Keyboard shortcuts must not fire while a text field is being edited.
    function editingText() {
        const item = root.activeFocusItem
        return item !== null && item !== undefined && ("cursorPosition" in item)
    }

    function enterFocusMode() {
        if (root.focusMode) {
            return
        }
        root.visibilityBeforeFocus = root.visibility
        root.focusMode = true
        root.showFullScreen()
    }

    function exitFocusMode() {
        if (!root.focusMode) {
            return
        }
        root.focusMode = false
        if (root.visibilityBeforeFocus === Window.Maximized) {
            root.showMaximized()
        } else {
            root.showNormal()
        }
    }

    function toggleStartPause() {
        if (DayController.canPause) {
            DayController.pause()
        } else if (DayController.canResume) {
            DayController.resume()
        } else if (DayController.canStart) {
            DayController.start()
        }
    }

    Overlay {
        id: overlay

        // A push-up prompt arrives just before the focus end alarm of the same break.
        property bool pushupsPending: false
    }

    Connections {
        target: DayController

        function onPushupPrompt(blockIndex, setIndex) {
            overlay.pushupsPending = true
            if (overlay.visible && overlay.mode === "break") {
                overlay.showPushups = true
            }
        }

        function onAlarmRaised(kind, blockIndex, title, message) {
            if (kind === DayController.BlockStart) {
                if (DayController.blockFullscreenAlarm(blockIndex)) {
                    overlay.open("blockStart", blockIndex, title, qsTr("%1 to %2").arg(DayController.currentStartText).arg(DayController.currentEndText))
                }
            } else if (kind === DayController.PomodoroFocusEnd) {
                const mode = DayController.pomodoroOnBreak ? "break" : "done"
                overlay.open(mode, blockIndex, DayController.currentName, mode === "break" ? DayController.pomodoroPhaseText : qsTr("Last pomodoro finished"))
                overlay.showPushups = mode === "break" && overlay.pushupsPending
                overlay.pushupsPending = false
            } else if (kind === DayController.UnconfirmedPending) {
                overlay.open("unconfirmed", blockIndex, title, qsTr("Did you do this?"))
            }
        }
    }

    Item {
        id: chrome

        anchors {
            fill: parent
            leftMargin: Theme.spacing40
            rightMargin: Theme.spacing40
            bottomMargin: Theme.spacing40
        }
        visible: !root.focusMode

        TopBar {
            id: topBar

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            currentScreen: root.currentScreen
            onScreenRequested: function(index) { root.currentScreen = index }
        }

        Item {
            id: screens

            anchors {
                left: parent.left
                right: parent.right
                top: topBar.bottom
                topMargin: Theme.spacing16
                bottom: parent.bottom
            }

            TodayScreen {
                anchors.fill: parent
                visible: root.currentScreen === 0
                onFocusModeRequested: root.enterFocusMode()
            }

            StatsScreen {
                anchors.fill: parent
                visible: root.currentScreen === 1
            }

            TemplatesScreen {
                anchors.fill: parent
                visible: root.currentScreen === 2
            }

            SettingsScreen {
                anchors.fill: parent
                visible: root.currentScreen === 3
                onChoosePlaylistRequested: function(activityId, activityName) {
                    picker.forActivity = activityId
                    picker.show(qsTr("Playlist for %1").arg(activityName))
                }
            }
        }
    }

    FocusScreen {
        anchors.fill: parent
        visible: root.focusMode
        onExitRequested: root.exitFocusMode()
        onPickPlaylistRequested: {
            picker.forActivity = ""
            picker.show(qsTr("Play a playlist"))
        }
    }

    // One picker for both uses: playing now from focus mode, or choosing an activity's default.
    PlaylistPicker {
        id: picker

        property string forActivity: ""

        onChosen: function(uri, name) {
            if (picker.forActivity.length > 0) {
                Settings.setPlaylistFor(picker.forActivity, uri, name)
            } else {
                Spotify.playPlaylist(uri)
            }
        }
    }

    // The player is polled only while a screen that shows it is on screen.
    Binding {
        target: Spotify
        property: "pollingEnabled"
        value: root.visible && (root.focusMode || root.currentScreen === 0)
    }

    Shortcut {
        sequence: "Ctrl+Right"
        enabled: root.focusMode && Spotify.connected
        onActivated: Spotify.next()
    }

    Shortcut {
        sequence: "Ctrl+Left"
        enabled: root.focusMode && Spotify.connected
        onActivated: Spotify.previous()
    }

    Shortcut {
        sequence: "Ctrl+Space"
        enabled: root.focusMode && Spotify.connected
        onActivated: Spotify.playPause()
    }

    Shortcut {
        sequence: "Space"
        onActivated: if (!root.editingText()) root.toggleStartPause()
    }

    Shortcut {
        sequence: "F"
        onActivated: {
            if (root.editingText()) {
                return
            }
            if (root.focusMode) {
                root.exitFocusMode()
            } else {
                root.enterFocusMode()
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.focusMode
        onActivated: root.exitFocusMode()
    }

    Shortcut {
        sequence: "Ctrl+1"
        onActivated: root.currentScreen = 0
    }

    Shortcut {
        sequence: "Ctrl+2"
        onActivated: root.currentScreen = 1
    }

    Shortcut {
        sequence: "Ctrl+3"
        onActivated: root.currentScreen = 2
    }

    Shortcut {
        sequence: "Ctrl+4"
        onActivated: root.currentScreen = 3
    }
}
