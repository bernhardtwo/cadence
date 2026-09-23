import QtQuick
import Cadence.Theme

// One entry of the top navigation. Active pills fill with the text color and flip the label dark.
Rectangle {
    id: root

    property string text
    property bool active: false

    signal clicked()

    implicitWidth: label.implicitWidth + Theme.spacing20 * 2
    implicitHeight: Theme.touchTarget - Theme.spacing8
    radius: Theme.pill
    color: root.active ? Theme.text : "transparent"

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: root.clicked()

    Text {
        id: label

        anchors.centerIn: parent
        text: root.text
        color: root.active ? Theme.textOnAccent : Theme.textMuted
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightSemiBold
        font.pixelSize: Theme.bodySize
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
