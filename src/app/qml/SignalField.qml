import QtQuick
import Cadence.Theme

// Single line text entry. The value comes from the model and always wins over the text in the
// field: when the selection moves while a field is focused, the field must show the new block and
// must not commit the old text to it on blur. The model only changes on user actions, so typing is
// never interrupted by a refresh. A focus scope, so focusing the field focuses its input.
FocusScope {
    id: root

    property string label
    property string value
    property string placeholder
    property bool numeric: false
    property bool error: false
    property alias text: input.text
    readonly property bool editing: input.activeFocus

    signal committed(string text)

    implicitWidth: Theme.fieldWidth
    implicitHeight: Theme.touchTarget + Theme.spacing20

    onValueChanged: input.text = root.value

    Text {
        id: caption

        anchors {
            left: parent.left
            top: parent.top
        }
        text: root.label
        color: root.error ? Theme.alert : Theme.textMuted
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightMedium
        font.pixelSize: Theme.labelSize
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
    }

    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: Theme.touchTarget
        radius: Theme.radius
        color: Theme.surface
        border.width: 1
        border.color: root.error ? Theme.alert : input.activeFocus ? Theme.accent : Theme.line

        TextInput {
            id: input

            anchors {
                fill: parent
                leftMargin: Theme.spacing12
                rightMargin: Theme.spacing12
            }
            focus: true
            verticalAlignment: TextInput.AlignVCenter
            text: root.value
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: Theme.textOnAccent
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.bodySize
            clip: true
            inputMethodHints: root.numeric ? Qt.ImhDigitsOnly : Qt.ImhNone
            validator: root.numeric ? intValidator : null

            onEditingFinished: {
                if (input.text !== root.value) {
                    root.committed(input.text)
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: input.text.length === 0 && !input.activeFocus
                text: root.placeholder
                color: Theme.textMuted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.bodySize
            }
        }
    }

    IntValidator {
        id: intValidator

        bottom: 0
        top: 1440
    }
}
