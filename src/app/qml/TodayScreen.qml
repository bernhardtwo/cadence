pragma ComponentBehavior: Bound
import QtQuick
import Cadence
import Cadence.Theme

// The day as stacked bars on the left and the pomodoro column with the block actions on the right.
Item {
    id: root

    signal focusModeRequested()

    readonly property var blocks: DayController.plan
    readonly property int totalMinutes: {
        let total = 0
        for (const block of root.blocks) {
            if (block.state !== "Skipped") {
                total += Math.max(1, block.durationMinutes)
            }
        }
        return Math.max(1, total)
    }

    function barHeight(block) {
        const available = bars.height - (root.blocks.length - 1) * Theme.spacing8
        const proportional = Math.max(1, block.durationMinutes) * available / root.totalMinutes
        const minimum = block.current && block.state !== "Done" && block.state !== "Skipped"
                        ? Theme.currentBlockMinHeight : Theme.blockMinHeight
        if (block.state === "Skipped" || block.state === "Done") {
            return Theme.blockMinHeight
        }
        return Math.max(minimum, Math.round(proportional))
    }

    Flickable {
        id: bars

        anchors {
            left: parent.left
            right: sidePane.left
            rightMargin: Theme.spacing24
            top: parent.top
            bottom: parent.bottom
        }
        clip: true
        contentHeight: barColumn.height
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: barColumn

            width: bars.width
            spacing: Theme.spacing8

            Text {
                visible: DayController.freeDay
                text: qsTr("FREE DAY")
                color: Theme.textMuted
                font.family: Theme.displayFamily
                font.weight: Theme.displayWeightExtraBold
                font.pixelSize: Theme.displaySizeMax
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

            Repeater {
                model: root.blocks

                BlockBar {
                    id: bar

                    required property var modelData

                    width: barColumn.width
                    height: root.barHeight(bar.modelData)
                    block: bar.modelData
                }
            }
        }
    }

    // The side column scrolls when the window is too short for the grid and the actions.
    Flickable {
        id: sidePane

        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
        }
        width: Theme.sideColumnWidth
        clip: true
        contentHeight: side.height
        boundsBehavior: Flickable.StopAtBounds

        Rectangle {
            x: sidePane.width - width
            y: sidePane.contentY + sidePane.visibleArea.yPosition * sidePane.height
            width: Theme.spacing4
            height: Math.max(Theme.touchTarget, sidePane.visibleArea.heightRatio * sidePane.height)
            radius: Theme.pill
            color: Theme.line
            visible: sidePane.contentHeight > sidePane.height
        }

        Column {
            id: side

            width: sidePane.width
            spacing: Theme.spacing12

            Text {
                text: DayController.freeDay ? "" : DayController.currentName
                color: Theme.textMuted
                width: parent.width
                elide: Text.ElideRight
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.labelSize
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
            }

            PomodoroGrid {
                width: parent.width
            }

            Text {
                width: parent.width
                text: DayController.hasPomodoro
                      ? qsTr("%1 of %2 · %3").arg(DayController.pomodoroIndex).arg(DayController.pomodoroTotal).arg(DayController.nextBreakText)
                      : DayController.pomodoroTotal > 0 ? qsTr("%n pomodoro(s)", "", DayController.pomodoroTotal) : qsTr("No pomodoros")
                color: Theme.text
                elide: Text.ElideRight
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.bodySize
            }

            // Compact now playing line, only while connected.
            Row {
                width: parent.width
                spacing: Theme.spacing8
                visible: Spotify.connected

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.spacing24
                    height: Theme.spacing24
                    radius: Theme.radius
                    color: Theme.surfaceMuted

                    Image {
                        anchors.fill: parent
                        source: Spotify.imageUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - Theme.spacing24 - Theme.spacing8
                    text: Spotify.hasActiveDevice
                          ? (Spotify.isPlaying ? qsTr("Playing %1 · %2") : qsTr("Paused %1 · %2")).arg(Spotify.trackName).arg(Spotify.artists)
                          : qsTr("Open Spotify on this PC")
                    color: Theme.textMuted
                    elide: Text.ElideRight
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.labelSize
                }
            }

            Item {
                width: 1
                height: Theme.spacing8
            }

            SignalButton {
                width: parent.width
                display: true
                kind: DayController.canStart ? "primary" : "light"
                text: DayController.canStart ? qsTr("START BLOCK") : DayController.canResume ? qsTr("RESUME") : qsTr("FOCUS MODE")
                enabled: DayController.canStart || DayController.canResume || DayController.running
                onClicked: {
                    if (DayController.canStart) {
                        DayController.start()
                    } else if (DayController.canResume) {
                        DayController.resume()
                    } else {
                        root.focusModeRequested()
                    }
                }
            }

            Row {
                width: parent.width
                spacing: Theme.spacing12

                SignalButton {
                    width: (parent.width - Theme.spacing12) / 2
                    text: DayController.paused ? qsTr("Resume") : qsTr("Pause")
                    enabled: DayController.canPause || DayController.canResume
                    onClicked: DayController.paused ? DayController.resume() : DayController.pause()
                }

                SignalButton {
                    width: (parent.width - Theme.spacing12) / 2
                    text: qsTr("Skip block")
                    enabled: DayController.canSkip
                    onClicked: DayController.skip()
                }
            }

            Row {
                width: parent.width
                spacing: Theme.spacing12

                SignalButton {
                    width: (parent.width - Theme.spacing12) / 2
                    text: qsTr("Postpone")
                    enabled: DayController.canPostpone
                    onClicked: DayController.postpone()
                }

                SignalButton {
                    width: (parent.width - Theme.spacing12) / 2
                    text: qsTr("+10 min")
                    enabled: DayController.canExtend
                    onClicked: DayController.extend(10)
                }
            }

            SignalButton {
                width: parent.width
                kind: "outline"
                text: qsTr("Finish block")
                visible: DayController.canFinish
                onClicked: DayController.finish()
            }

            SignalButton {
                width: parent.width
                kind: "outline"
                text: qsTr("Focus mode")
                visible: !DayController.running && !DayController.freeDay
                onClicked: root.focusModeRequested()
            }

            Text {
                width: parent.width
                visible: DayController.doesNotFit
                text: qsTr("The day no longer fits before the cutoff")
                color: Theme.alert
                wrapMode: Text.Wrap
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.labelSize
            }
        }
    }
}
