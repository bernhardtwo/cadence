import QtQuick
import Cadence.Theme

// Every control we render is our own; this is the only button in the app.
Rectangle {
    id: root

    property string text
    property bool primary: false

    signal clicked()

    implicitWidth: label.implicitWidth + Theme.spacing24 * 2
    implicitHeight: Theme.touchTarget
    radius: Theme.radius
    color: primary ? Theme.accent : Theme.surfaceMuted
    opacity: enabled ? 1 : 0.35
    border.width: primary ? 0 : 1
    border.color: Theme.line

    Text {
        id: label

        anchors.centerIn: parent
        text: root.text
        color: root.primary ? Theme.textOnAccent : Theme.text
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightSemiBold
        font.pixelSize: Theme.bodySize
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
