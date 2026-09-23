import QtQuick
import Cadence
import Cadence.Theme

// Full screen alarm surface. Modes: blockStart, focusEnd, unconfirmed.
Window {
    id: overlay

    property string mode: ""
    property int blockIndex: -1
    property string headline: ""
    property string detail: ""
    property bool showPushups: false
    property int reps: 10

    title: "Cadence Alarm"
    color: Theme.bg
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    // Not owned by the main window, so it shows even while that one hides in the tray.
    transientParent: null

    function open(newMode, index, newHeadline, newDetail) {
        mode = newMode
        blockIndex = index
        headline = newHeadline
        detail = newDetail
        reps = 10
        showFullScreen()
        raise()
        requestActivate()
    }

    function dismiss() {
        showPushups = false
        hide()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: overlay.dismiss()
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacing40 * 2, 1400)
        spacing: Theme.spacing24

        Text {
            width: parent.width
            text: overlay.detail
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.headingSizeMin
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 2
        }

        Text {
            width: parent.width
            text: overlay.headline
            color: Theme.accent
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.pixelSize: Math.min(Theme.timerSizeMax, Math.max(Theme.timerSizeMin, Math.round(overlay.height * 0.3)))
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "blockStart"

            SignalButton {
                text: "Start"
                primary: true
                onClicked: { DayController.start(); overlay.dismiss() }
            }

            SignalButton {
                text: "Snooze 5 min"
                visible: DayController.canSnooze(overlay.blockIndex)
                onClicked: { DayController.snooze(overlay.blockIndex); overlay.dismiss() }
            }

            SignalButton {
                text: "Skip"
                onClicked: { DayController.skip(); overlay.dismiss() }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "focusEnd"

            SignalButton {
                text: "Continue"
                primary: true
                onClicked: overlay.dismiss()
            }

            SignalButton {
                text: "Skip break"
                visible: DayController.hasPomodoro
                onClicked: { DayController.skipPhase(); overlay.dismiss() }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "unconfirmed"

            SignalButton {
                text: "Yes"
                primary: true
                onClicked: { DayController.confirm(overlay.blockIndex, true); overlay.dismiss() }
            }

            SignalButton {
                text: "No"
                onClicked: { DayController.confirm(overlay.blockIndex, false); overlay.dismiss() }
            }
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing12
            visible: overlay.mode === "focusEnd" && overlay.showPushups

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Push-ups"
                color: Theme.textMuted
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.labelSize
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.spacing12

                SignalButton {
                    text: "−"
                    width: Theme.touchTarget
                    enabled: overlay.reps > 1
                    onClicked: overlay.reps -= 1
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 80
                    text: overlay.reps
                    color: Theme.text
                    horizontalAlignment: Text.AlignHCenter
                    font.family: Theme.displayFamily
                    font.weight: Theme.displayWeightSemiBold
                    font.pixelSize: Theme.headingSizeMax
                }

                SignalButton {
                    text: "+"
                    width: Theme.touchTarget
                    onClicked: overlay.reps += 1
                }

                SignalButton {
                    text: "Log set"
                    primary: true
                    onClicked: { DayController.logPushups(overlay.reps); overlay.showPushups = false }
                }
            }
        }

        Text {
            width: parent.width
            text: "Esc to dismiss"
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.labelSize
        }
    }
}
