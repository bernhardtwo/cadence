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

    // The width of all pills in one row; a narrower explicit width wraps them onto more rows and
    // the height follows.
    readonly property real rowWidth: {
        let total = 0
        for (const child of pills.children) {
            if (child.implicitWidth > 0) {
                total += child.implicitWidth + pills.spacing
            }
        }
        return Math.max(0, total - pills.spacing)
    }

    implicitWidth: root.rowWidth
    implicitHeight: pills.implicitHeight + Theme.spacing20

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

    Flow {
        id: pills

        anchors {
            left: parent.left
            right: parent.right
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
