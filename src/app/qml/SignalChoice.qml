pragma ComponentBehavior: Bound
import QtQuick
import Cadence.Theme

// A row of exclusive pills. options is a list of strings; current is the selected one.
Item {
    id: root

    property string label
    property var options: []
    property string current: ""

    signal chosen(string option)

    implicitWidth: pills.implicitWidth
    implicitHeight: Theme.touchTarget + Theme.spacing20

    Text {
        anchors {
            left: parent.left
            top: parent.top
        }
        text: root.label
        color: Theme.textMuted
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightMedium
        font.pixelSize: Theme.labelSize
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
    }

    Row {
        id: pills

        anchors {
            left: parent.left
            bottom: parent.bottom
        }
        spacing: Theme.spacing4

        Repeater {
            model: root.options

            NavPill {
                id: pill

                required property string modelData

                text: pill.modelData
                active: root.current === pill.modelData
                implicitHeight: Theme.touchTarget
                onClicked: root.chosen(pill.modelData)
            }
        }
    }
}
