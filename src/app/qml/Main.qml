import QtQuick
import QtQuick.Controls
import Cadence
import Cadence.Theme

ApplicationWindow {
    id: root

    width: 1280
    height: 800
    minimumWidth: 640
    minimumHeight: 400
    visible: true
    title: "Cadence"
    color: Theme.bg

    Text {
        id: wordmark

        anchors {
            top: parent.top
            left: parent.left
            margins: Theme.spacing40
        }
        text: "CADENCE"
        color: Theme.text
        font.family: Theme.displayFamily
        font.weight: Theme.displayWeightExtraBold
        font.pixelSize: Theme.displaySizeMin
        font.letterSpacing: 2
    }

    Text {
        id: timer

        anchors.centerIn: parent
        text: "25:00"
        color: Theme.accent
        font.family: Theme.displayFamily
        font.weight: Theme.displayWeightExtraBold
        font.pixelSize: Math.min(Theme.timerSizeMax,
                                 Math.max(Theme.timerSizeMin, Math.round(root.height * 0.42)))
    }

    Text {
        id: versionLabel

        anchors {
            bottom: parent.bottom
            right: parent.right
            margins: Theme.spacing40
        }
        text: "v" + AppInfo.version
        color: Theme.textMuted
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightMedium
        font.pixelSize: Theme.labelSize
    }
}
