import QtQuick
import Cadence
import Cadence.Theme

// Full screen alarm surface. Modes: blockStart, break, done, unconfirmed. The break takes the
// accent as its background; everything else sits on the dark canvas.
Window {
    id: overlay

    property string mode: ""
    property int blockIndex: -1
    property string headline: ""
    property string detail: ""
    property bool showPushups: false
    property int defaultReps: Settings.defaultReps
    property int reps: overlay.defaultReps

    readonly property bool onAccent: overlay.mode === "break"
    readonly property color ink: overlay.onAccent ? Theme.textOnAccent : Theme.text
    readonly property color inkMuted: overlay.onAccent ? Theme.textOnAccentMuted : Theme.textMuted
    readonly property color headlineColor: overlay.onAccent ? Theme.textOnAccent : Theme.accent

    title: "Cadence Alarm"
    color: overlay.onAccent ? Theme.accent : Theme.bg
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    // Not owned by the main window, so it shows even while that one hides in the tray.
    transientParent: null

    function open(newMode, index, newHeadline, newDetail) {
        mode = newMode
        blockIndex = index
        headline = newHeadline
        detail = newDetail
        reps = overlay.defaultReps
        showFullScreen()
        raise()
        requestActivate()
        EventLog.write("overlay shown " + newMode + " block=" + index)
    }

    // reason names what closed the overlay: escape, or the action the user took.
    function dismiss(reason) {
        const why = reason === undefined ? "escape" : reason
        if (overlay.mode === "break" && overlay.showPushups && why !== "log-set") {
            DayController.dismissPrompt()
        }
        EventLog.write("overlay " + (why === "escape" ? "dismissed" : "action") + " " + overlay.mode + " " + why)
        showPushups = false
        hide()
    }

    function mmss(seconds) {
        const clamped = Math.max(0, seconds)
        return Math.floor(clamped / 60) + ":" + ("0" + clamped % 60).slice(-2)
    }

    Shortcut {
        sequence: "Escape"
        onActivated: overlay.dismiss()
    }

    // Return takes the primary action of the mode.
    Shortcut {
        sequences: ["Return", "Enter"]
        onActivated: {
            if (overlay.mode === "blockStart") {
                DayController.start()
                overlay.dismiss("start")
            } else if (overlay.mode === "break" && overlay.showPushups) {
                DayController.logPushups(overlay.reps)
                overlay.dismiss("log-set")
            } else if (overlay.mode === "unconfirmed") {
                DayController.confirm(overlay.blockIndex, true)
                overlay.dismiss("confirm-yes")
            } else {
                overlay.dismiss("continue")
            }
        }
    }

    // Top line: what kind of moment this is, and the countdown while on a break.
    Text {
        anchors {
            left: parent.left
            top: parent.top
            margins: Theme.spacing40
        }
        text: overlay.mode === "break" ? "BREAK"
            : overlay.mode === "done" ? "BLOCK DONE"
            : overlay.mode === "blockStart" ? "BLOCK START"
            : overlay.mode === "unconfirmed" ? "CONFIRM" : ""
        color: overlay.ink
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightSemiBold
        font.pixelSize: Theme.headingSizeMin
        font.letterSpacing: 2
    }

    Text {
        anchors {
            right: parent.right
            top: parent.top
            margins: Theme.spacing40
        }
        visible: overlay.mode === "break" && DayController.pomodoroOnBreak
        text: overlay.mmss(DayController.pomodoroRemainingSeconds)
        color: overlay.ink
        font.family: Theme.displayFamily
        font.weight: Theme.displayWeightExtraBold
        font.pixelSize: Theme.displaySizeMin
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacing40 * 2, Theme.contentMaxWidth)
        spacing: Theme.spacing24

        Text {
            width: parent.width
            text: overlay.detail
            color: overlay.inkMuted
            horizontalAlignment: Text.AlignHCenter
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.headingSizeMin
            font.letterSpacing: 2
            font.capitalization: Font.AllUppercase
        }

        Text {
            width: parent.width
            text: overlay.mode === "break" ? (overlay.showPushups ? "DROP AND GIVE ME" : "TAKE A BREAK") : overlay.headline
            color: overlay.headlineColor
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.pixelSize: overlay.mode === "break" ? Theme.overlayHeadlineSize
                          : Math.min(Theme.timerSizeMax, Math.max(Theme.timerSizeMin, Math.round(overlay.height * 0.3)))
            font.capitalization: Font.AllUppercase
        }

        // Push-up stepper: minus, the count in a dark box, plus.
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "break" && overlay.showPushups

            SignalButton {
                anchors.verticalCenter: parent.verticalCenter
                kind: "outline"
                text: "−"
                display: true
                width: Theme.buttonHeightLarge
                textColor: Theme.textOnAccent
                outlineColor: Theme.textOnAccent
                enabled: overlay.reps > 1
                onClicked: overlay.reps -= 1
            }

            Rectangle {
                width: Theme.buttonHeightLarge * 3
                height: Theme.buttonHeightLarge * 2
                radius: Theme.radius
                color: Theme.bg

                Text {
                    anchors.centerIn: parent
                    text: overlay.reps
                    color: Theme.accent
                    font.family: Theme.displayFamily
                    font.weight: Theme.displayWeightExtraBold
                    font.pixelSize: Theme.displaySizeMax
                }
            }

            SignalButton {
                anchors.verticalCenter: parent.verticalCenter
                kind: "outline"
                text: "+"
                display: true
                width: Theme.buttonHeightLarge
                textColor: Theme.textOnAccent
                outlineColor: Theme.textOnAccent
                onClicked: overlay.reps += 1
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: overlay.mode === "break" && overlay.showPushups
            text: "Today " + DayController.pushupsToday
            color: overlay.inkMuted
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.bodySize
            font.letterSpacing: 1
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "break"

            SignalButton {
                visible: overlay.showPushups
                kind: "dark"
                display: true
                text: "LOG SET"
                width: Theme.buttonHeightLarge * 4
                onClicked: { DayController.logPushups(overlay.reps); overlay.dismiss("log-set") }
            }

            SignalButton {
                visible: !overlay.showPushups
                kind: "dark"
                display: true
                text: "CONTINUE"
                width: Theme.buttonHeightLarge * 4
                onClicked: overlay.dismiss("continue")
            }

            SignalButton {
                kind: "outline"
                text: overlay.showPushups ? "Skip" : "Skip break"
                implicitHeight: Theme.buttonHeightLarge
                textColor: Theme.textOnAccent
                outlineColor: Theme.textOnAccent
                onClicked: {
                    if (!overlay.showPushups) {
                        DayController.skipPhase()
                        overlay.dismiss("skip-break")
                    } else {
                        overlay.dismiss("skip")
                    }
                }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "done"

            SignalButton {
                text: "Continue"
                primary: true
                display: true
                width: Theme.buttonHeightLarge * 4
                onClicked: overlay.dismiss("continue")
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "blockStart"

            SignalButton {
                text: "START"
                primary: true
                display: true
                width: Theme.buttonHeightLarge * 4
                onClicked: { DayController.start(); overlay.dismiss("start") }
            }

            SignalButton {
                kind: "outline"
                text: "Snooze 5 min"
                implicitHeight: Theme.buttonHeightLarge
                visible: DayController.canSnooze(overlay.blockIndex)
                onClicked: { DayController.snooze(overlay.blockIndex); overlay.dismiss("snooze") }
            }

            SignalButton {
                kind: "outline"
                text: "Skip"
                implicitHeight: Theme.buttonHeightLarge
                onClicked: { DayController.skip(); overlay.dismiss("skip-block") }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "unconfirmed"

            SignalButton {
                text: "YES"
                primary: true
                display: true
                width: Theme.buttonHeightLarge * 3
                onClicked: { DayController.confirm(overlay.blockIndex, true); overlay.dismiss("confirm-yes") }
            }

            SignalButton {
                kind: "outline"
                text: "No"
                implicitHeight: Theme.buttonHeightLarge
                width: Theme.buttonHeightLarge * 3
                onClicked: { DayController.confirm(overlay.blockIndex, false); overlay.dismiss("confirm-no") }
            }
        }
    }

    Text {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: Theme.spacing40
        }
        text: "Esc to dismiss"
        color: overlay.inkMuted
        font.family: Theme.bodyFamily
        font.pixelSize: Theme.labelSize
    }
}
