import QtQuick
import Cadence.Theme

// A labelled on/off switch drawn as a pill with a sliding knob.
Item {
    id: root

    property string label
    property bool checked: false

    signal toggled(bool checked)

    implicitWidth: track.width + Theme.spacing12 + caption.implicitWidth
    implicitHeight: Theme.touchTarget

    Accessible.role: Accessible.CheckBox
    Accessible.name: root.label
    Accessible.checked: root.checked
    Accessible.onPressAction: root.toggled(!root.checked)

    Rectangle {
        id: track

        anchors.verticalCenter: parent.verticalCenter
        width: Theme.touchTarget
        height: Theme.spacing24
        radius: Theme.pill
        color: root.checked ? Theme.accent : Theme.surfaceMuted
        border.width: root.checked ? 0 : 1
        border.color: Theme.line

        Rectangle {
            x: root.checked ? parent.width - width - Theme.spacing4 : Theme.spacing4
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.spacing16
            height: Theme.spacing16
            radius: Theme.pill
            color: root.checked ? Theme.textOnAccent : Theme.textMuted
        }
    }

    Text {
        id: caption

        anchors {
            left: track.right
            leftMargin: Theme.spacing12
            verticalCenter: parent.verticalCenter
        }
        text: root.label
        color: Theme.text
        font.family: Theme.bodyFamily
        font.pixelSize: Theme.bodySize
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }
}
