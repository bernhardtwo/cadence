import QtQuick
import Cadence.Theme

// Placeholder until the statistics milestone.
Item {
    id: root

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacing12

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "STATS"
            color: Theme.textMuted
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.pixelSize: Theme.displaySizeMax
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Stats arrive in the next milestone"
            color: Theme.textMuted
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.bodySize
        }
    }
}
