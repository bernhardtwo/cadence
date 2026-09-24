import QtQuick
import Cadence
import Cadence.Theme

// One block of the day as a stacked bar. The current block is the big one: accent when active,
// outlined while it waits. The others show name, time and detail on one line.
Rectangle {
    id: root

    required property var block

    readonly property bool isCurrent: root.block.current
    readonly property bool isActive: root.block.state === "Active"
    readonly property bool isDone: root.block.state === "Done"
    readonly property bool isSkipped: root.block.state === "Skipped"
    readonly property bool doesNotFit: root.block.state === "Does not fit"
    readonly property bool isUnconfirmed: root.block.state === "Unconfirmed"
    readonly property bool big: root.isCurrent && (root.isActive || root.block.state === "Upcoming")
    readonly property bool accented: root.big && root.isActive
    readonly property color ink: root.accented ? Theme.textOnAccent : Theme.text
    readonly property color inkMuted: root.accented ? Theme.textOnAccentMuted : Theme.textMuted
    property bool asking: false

    radius: Theme.radius
    color: root.accented ? Theme.accent : root.big ? Theme.surfaceMuted : Theme.surface
    border.width: root.big && !root.accented ? Theme.outlineWidth : 0
    border.color: Theme.accent
    opacity: root.isDone ? 0.5 : 1

    function mmss(seconds) {
        const clamped = Math.max(0, seconds)
        const minutes = Math.floor(clamped / 60)
        const secs = clamped % 60
        return String(minutes).padStart(2, "0") + ":" + ("0" + secs).slice(-2)
    }

    // Compact row for every block that is not the current one.
    Item {
        anchors {
            fill: parent
            leftMargin: Theme.spacing20
            rightMargin: Theme.spacing20
        }
        visible: !root.big

        Text {
            id: compactName

            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
            }
            width: Math.min(implicitWidth, parent.width - compactRight.width - Theme.spacing16)
            text: root.block.name
            color: root.isSkipped ? Theme.textMuted : Theme.text
            elide: Text.ElideRight
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightSemiBold
            font.pixelSize: Theme.blockNameSize
            font.capitalization: Font.AllUppercase
            font.strikeout: root.isSkipped
        }

        Row {
            id: compactRight

            anchors {
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            spacing: Theme.spacing16

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.doesNotFit || root.block.overrunsCutoff
                text: root.doesNotFit ? qsTr("DOES NOT FIT") : qsTr("OVERRUNS CUTOFF")
                color: Theme.alert
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightSemiBold
                font.pixelSize: Theme.labelSize
                font.letterSpacing: 1
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.isUnconfirmed && !root.asking
                width: confirmLabel.implicitWidth + Theme.spacing16 * 2
                height: Theme.touchTarget - Theme.spacing8
                radius: Theme.pill
                color: Theme.alert

                Text {
                    id: confirmLabel

                    anchors.centerIn: parent
                    text: qsTr("CONFIRM?")
                    color: Theme.textOnAccent
                    font.family: Theme.bodyFamily
                    font.weight: Theme.bodyWeightSemiBold
                    font.pixelSize: Theme.labelSize
                    font.letterSpacing: 1
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.asking = true
                }
            }

            Row {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.isUnconfirmed && root.asking
                spacing: Theme.spacing8

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Did it happen?")
                    color: Theme.text
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.bodySize
                }

                SignalButton {
                    text: qsTr("Yes")
                    primary: true
                    implicitHeight: Theme.touchTarget - Theme.spacing8
                    onClicked: DayController.confirm(root.block.index, true)
                }

                SignalButton {
                    text: qsTr("No")
                    implicitHeight: Theme.touchTarget - Theme.spacing8
                    onClicked: DayController.confirm(root.block.index, false)
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("%1 to %2").arg(root.block.start).arg(root.block.end)
                color: Theme.textMuted
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.bodySize
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.block.detail
                color: Theme.textMuted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.labelSize
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.isDone || root.isSkipped
                text: root.block.stateText
                color: Theme.textMuted
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.labelSize
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1
            }
        }
    }

    // The current block: time range on top, name bottom left, timer bottom right.
    Item {
        anchors {
            fill: parent
            margins: Theme.spacing20
        }
        visible: root.big

        Text {
            id: bigTop

            anchors {
                top: parent.top
                left: parent.left
            }
            text: root.isActive ? qsTr("%1 to %2 · NOW").arg(root.block.start).arg(root.block.end)
                                : qsTr("%1 to %2 · NEXT").arg(root.block.start).arg(root.block.end)
            color: root.ink
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightSemiBold
            font.pixelSize: Theme.bodySize
            font.letterSpacing: 1
        }

        Text {
            anchors {
                top: parent.top
                right: parent.right
            }
            visible: root.block.overrunsCutoff
            text: qsTr("OVERRUNS CUTOFF")
            color: Theme.alert
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightSemiBold
            font.pixelSize: Theme.labelSize
            font.letterSpacing: 1
        }

        Text {
            id: bigName

            anchors {
                left: parent.left
                bottom: parent.bottom
                bottomMargin: -Theme.spacing8
            }
            width: parent.width - bigTimer.width - Theme.spacing24
            text: root.block.name
            color: root.ink
            elide: Text.ElideRight
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.pixelSize: Math.min(Theme.displaySizeMax, Math.max(Theme.displaySizeMin, Math.round(root.height * 0.4)))
            font.capitalization: Font.AllUppercase
        }

        TimerText {
            id: bigTimer

            anchors {
                right: parent.right
                bottom: parent.bottom
                bottomMargin: -Theme.spacing12
            }
            horizontalAlignment: Text.AlignRight
            text: !root.isActive
                  ? (DayController.remainingSeconds > 0 ? DayController.remainingText : qsTr("READY"))
                  : DayController.hasPomodoro ? root.mmss(DayController.pomodoroRemainingSeconds)
                                              : DayController.remainingText
            color: root.ink
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.pixelSize: Math.min(Theme.blockTimerSize, Math.max(Theme.displaySizeMax, Math.round(root.height * 0.62)))
        }

        Column {
            anchors {
                right: parent.right
                bottom: bigTimer.top
                bottomMargin: Theme.spacing8
            }
            spacing: Theme.spacing4

            Text {
                anchors.right: parent.right
                visible: root.isActive && DayController.hasPomodoro
                text: DayController.pomodoroOnBreak
                      ? DayController.pomodoroPhaseText.toUpperCase()
                      : qsTr("FOCUS %1 / %2").arg(DayController.pomodoroIndex).arg(DayController.pomodoroTotal)
                color: root.ink
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightSemiBold
                font.pixelSize: Theme.bodySize
                font.letterSpacing: 1
            }

            TimerText {
                anchors.right: parent.right
                horizontalAlignment: Text.AlignRight
                visible: root.isActive && DayController.hasPomodoro
                text: qsTr("Block ends %1 · %2 left").arg(root.block.end).arg(DayController.remainingText)
                color: root.inkMuted
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.bodySize
            }

            Text {
                anchors.right: parent.right
                visible: !root.isActive
                text: DayController.remainingSeconds > 0 ? qsTr("until start") : qsTr("waiting for you")
                color: root.inkMuted
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.bodySize
            }
        }
    }
}
