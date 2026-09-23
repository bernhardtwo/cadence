import QtQuick
import Cadence
import Cadence.Theme

// Full screen view of the running phase: the timer, its progress and nothing else.
Rectangle {
    id: root

    signal exitRequested()

    color: Theme.bg

    readonly property bool pomodoro: DayController.hasPomodoro
    readonly property int remaining: root.pomodoro ? DayController.pomodoroRemainingSeconds : DayController.remainingSeconds
    readonly property int length: root.pomodoro ? DayController.pomodoroPhaseSeconds : DayController.currentDurationMinutes * 60
    readonly property real progress: root.length > 0 ? Math.min(1, Math.max(0, 1 - root.remaining / root.length)) : 0

    function mmss(seconds) {
        const clamped = Math.max(0, seconds)
        const hours = Math.floor(clamped / 3600)
        const minutes = Math.floor(clamped / 60) % 60
        const secs = clamped % 60
        if (hours > 0) {
            return hours + ":" + ("0" + minutes).slice(-2) + ":" + ("0" + secs).slice(-2)
        }
        return minutes + ":" + ("0" + secs).slice(-2)
    }

    Text {
        id: header

        anchors {
            left: parent.left
            top: parent.top
            margins: Theme.spacing40
        }
        text: DayController.currentName.toUpperCase()
              + (root.pomodoro ? " · POMODORO " + DayController.pomodoroIndex + " / " + DayController.pomodoroTotal : "")
        color: Theme.textMuted
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightSemiBold
        font.pixelSize: Theme.bodySize
        font.letterSpacing: 2
    }

    SignalButton {
        anchors {
            right: parent.right
            top: parent.top
            margins: Theme.spacing40
        }
        kind: "outline"
        text: "Exit · Esc"
        onClicked: root.exitRequested()
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacing40 * 2, Theme.contentMaxWidth)
        spacing: Theme.spacing24

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.pomodoro ? DayController.pomodoroPhase.toUpperCase()
                                : DayController.currentActive ? "BLOCK" : "STARTS IN"
            color: Theme.textMuted
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightSemiBold
            font.pixelSize: Theme.headingSizeMin
            font.letterSpacing: 2
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.mmss(root.remaining)
            color: DayController.paused ? Theme.textMuted : Theme.accent
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.pixelSize: Math.min(Theme.timerSizeMax, Math.max(Theme.timerSizeMin, Math.round(root.height * 0.45)))
        }

        Rectangle {
            width: parent.width
            height: Theme.progressBarHeight
            radius: Theme.radius
            color: Theme.surface

            Rectangle {
                width: parent.width * root.progress
                height: parent.height
                radius: Theme.radius
                color: Theme.accent
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.pomodoro
            text: "Block ends " + DayController.currentEndText + " · " + DayController.remainingText + " left"
            color: Theme.textMuted
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.bodySize
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: DayController.paused ? "Paused · Space to resume" : "Space to pause"
            color: Theme.textMuted
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.labelSize
        }
    }

    // Reserved for the music controls of the Spotify milestone.
    Row {
        id: musicRow

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: Theme.spacing40
        }
        height: Theme.buttonHeightLarge
        visible: false
    }
}
