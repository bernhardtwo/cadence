pragma ComponentBehavior: Bound
import QtQuick
import Cadence.Theme

// A row of exclusive pills. options are the labels shown; values, when given, are the matching
// keys reported and compared, so labels can be translated while keys stay stable.
Item {
    id: root

    property string label
    property var options: []
    property var values: []
    property string current: ""

    function valueAt(index) {
        return root.values.length > index ? root.values[index] : root.options[index]
    }

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
                required property int index

                text: pill.modelData
                active: root.current === root.valueAt(pill.index)
                implicitHeight: Theme.touchTarget
                onClicked: root.chosen(root.valueAt(pill.index))
            }
        }
    }
}
