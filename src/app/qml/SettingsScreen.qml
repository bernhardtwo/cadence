import QtQuick
import Cadence.Theme

// Filled by the settings commit.
Item {
    id: root

    Text {
        anchors.centerIn: parent
        text: "SETTINGS"
        color: Theme.textMuted
        font.family: Theme.displayFamily
        font.weight: Theme.displayWeightExtraBold
        font.pixelSize: Theme.displaySizeMax
    }
}
