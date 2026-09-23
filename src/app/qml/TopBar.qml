pragma ComponentBehavior: Bound
import QtQuick
import Cadence
import Cadence.Theme

// Wordmark, screen navigation and the day summary. Shared by every main screen.
Item {
    id: root

    property int currentScreen: 0
    readonly property var screens: ["Today", "Stats", "Templates", "Settings"]

    signal screenRequested(int index)

    implicitHeight: Theme.topBarHeight

    Text {
        id: wordmark

        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
        }
        text: "CADENCE"
        color: Theme.text
        font.family: Theme.displayFamily
        font.weight: Theme.displayWeightExtraBold
        font.pixelSize: Theme.wordmarkSize
        font.letterSpacing: 2
    }

    Row {
        anchors {
            left: wordmark.right
            leftMargin: Theme.spacing40
            verticalCenter: parent.verticalCenter
        }
        spacing: Theme.spacing4

        Repeater {
            model: root.screens

            NavPill {
                id: pill

                required property int index
                required property string modelData

                text: pill.modelData
                active: root.currentScreen === pill.index
                onClicked: root.screenRequested(pill.index)
            }
        }
    }

    Text {
        anchors {
            right: parent.right
            verticalCenter: parent.verticalCenter
        }
        text: DayController.summaryText
        color: Theme.textMuted
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightMedium
        font.pixelSize: Theme.bodySize
    }
}
