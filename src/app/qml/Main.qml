import QtQuick
import QtQuick.Controls
import Cadence
import Cadence.Theme

ApplicationWindow {
    id: root

    // Set by main before load; with a tray the window hides on close and only Quit exits.
    property bool trayAvailable: false

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

    Column {
        id: header

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: Theme.spacing40
        }
        spacing: Theme.spacing8

        Row {
            spacing: Theme.spacing16

            Text {
                text: "CADENCE"
                color: Theme.text
                font.family: Theme.displayFamily
                font.weight: Theme.displayWeightExtraBold
                font.pixelSize: Theme.headingSizeMax
                font.letterSpacing: 2
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: DayController.dateText
                color: Theme.textMuted
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.bodySize
            }
        }

        Text {
            width: parent.width
            visible: DayController.templateError.length > 0 || DayController.storageError.length > 0
            text: DayController.templateError.length > 0 ? DayController.templateError : DayController.storageError
            color: Theme.alert
            wrapMode: Text.WrapAnywhere
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.labelSize
        }

        Text {
            visible: DayController.doesNotFit || DayController.overrunsCutoff.length > 0
            text: DayController.doesNotFit ? "The day no longer fits before the cutoff"
                                           : "The current block will run past the cutoff"
            color: Theme.alert
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.labelSize
        }
    }

    Column {
        id: focus

        anchors {
            left: parent.left
            right: parent.horizontalCenter
            top: header.bottom
            bottom: actions.top
            margins: Theme.spacing40
        }
        spacing: Theme.spacing8

        Text {
            text: DayController.currentKind.length > 0
                  ? DayController.currentKind + " · " + DayController.currentState
                  : ""
            color: Theme.textMuted
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.labelSize
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1
        }

        Text {
            width: parent.width
            text: DayController.currentName
            color: Theme.text
            elide: Text.ElideRight
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.pixelSize: Theme.displaySizeMax
        }

        Text {
            text: DayController.remainingText
            color: DayController.running ? Theme.accent : Theme.textMuted
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.pixelSize: Math.min(Theme.timerSizeMax, Math.max(Theme.timerSizeMin, Math.round(root.height * 0.28)))
        }

        Text {
            visible: DayController.pomodoroTotal > 0
            text: DayController.hasPomodoro
                  ? "Pomodoro " + DayController.pomodoroIndex + "/" + DayController.pomodoroTotal
                    + " · " + DayController.pomodoroPhase
                    + " · " + Math.floor(DayController.pomodoroRemainingSeconds / 60)
                    + ":" + ("0" + DayController.pomodoroRemainingSeconds % 60).slice(-2)
                  : DayController.pomodoroTotal + " pomodoros"
            color: Theme.text
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.bodySize
        }
    }

    Row {
        id: actions

        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: Theme.spacing40
        }
        spacing: Theme.spacing12

        SignalButton {
            text: DayController.paused ? "Resume" : "Start"
            primary: true
            enabled: DayController.canStart || DayController.canResume
            onClicked: DayController.paused ? DayController.resume() : DayController.start()
        }

        SignalButton {
            text: "Pause"
            enabled: DayController.canPause
            onClicked: DayController.pause()
        }

        SignalButton {
            text: "Finish"
            enabled: DayController.canFinish
            onClicked: DayController.finish()
        }

        SignalButton {
            text: "Skip"
            enabled: DayController.canSkip
            onClicked: DayController.skip()
        }
    }

    ListView {
        id: planList

        anchors {
            left: parent.horizontalCenter
            right: parent.right
            top: header.bottom
            bottom: parent.bottom
            margins: Theme.spacing40
        }
        clip: true
        model: DayController.plan
        spacing: Theme.spacing4

        header: Text {
            text: DayController.freeDay ? "Free day" : "Today"
            color: Theme.textMuted
            bottomPadding: Theme.spacing12
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.labelSize
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1
        }

        delegate: Rectangle {
            id: row

            required property var modelData

            width: ListView.view.width
            height: Theme.touchTarget
            radius: Theme.radius
            color: row.modelData.current ? Theme.surfaceMuted : Theme.surface

            Row {
                anchors {
                    fill: parent
                    leftMargin: Theme.spacing16
                    rightMargin: Theme.spacing16
                }
                spacing: Theme.spacing16

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    text: row.modelData.start + " to " + row.modelData.end
                    color: Theme.textMuted
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.labelSize
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.name
                    color: row.modelData.state === "Skipped" ? Theme.textMuted : Theme.text
                    font.family: Theme.bodyFamily
                    font.weight: row.modelData.current ? Theme.bodyWeightSemiBold : Theme.bodyWeightRegular
                    font.pixelSize: Theme.bodySize
                    font.strikeout: row.modelData.state === "Skipped"
                }
            }

            Text {
                anchors {
                    right: parent.right
                    rightMargin: Theme.spacing16
                    verticalCenter: parent.verticalCenter
                }
                text: row.modelData.state + (row.modelData.overrunsCutoff ? " · overruns" : "")
                color: row.modelData.state === "Does not fit" || row.modelData.overrunsCutoff ? Theme.alert
                     : row.modelData.state === "Active" ? Theme.accent : Theme.textMuted
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.labelSize
            }
        }
    }
}
