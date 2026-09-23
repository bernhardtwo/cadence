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
    minimumWidth: 640
    minimumHeight: 480
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
            if (overlay.visible && overlay.mode === "focusEnd") {
                overlay.showPushups = true
            }
        }

        function onAlarmRaised(kind, blockIndex, title, message) {
            if (kind === DayController.BlockStart) {
                if (DayController.blockFullscreenAlarm(blockIndex)) {
                    overlay.open("blockStart", blockIndex, title, message)
                }
            } else if (kind === DayController.PomodoroFocusEnd) {
                overlay.open("focusEnd", blockIndex, DayController.currentName, message)
                overlay.showPushups = overlay.pushupsPending
                overlay.pushupsPending = false
            } else if (kind === DayController.UnconfirmedPending) {
                overlay.open("unconfirmed", blockIndex, title, "Did you do this?")
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
            }
        }
    }

    FocusScreen {
        anchors.fill: parent
        visible: root.focusMode
        onExitRequested: root.exitFocusMode()
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
