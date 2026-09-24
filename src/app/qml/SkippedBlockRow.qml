import QtQuick
import Cadence
import Cadence.Theme

// The content of a skipped block's row: the struck name, when it was skipped, what restoring
// would do and the Restore action while it still makes sense.
Item {
    id: root

    required property var block

    readonly property bool anchored: root.block.kind === "Anchored"
    readonly property bool restorable: root.block.restorable === true

    implicitHeight: Math.max(action.visible ? action.height : 0, column.implicitHeight) + Theme.spacing16 * 2

    Column {
        id: column

        anchors {
            left: parent.left
            leftMargin: Theme.spacing20
            right: action.left
            rightMargin: action.visible ? Theme.spacing16 : 0
            verticalCenter: parent.verticalCenter
        }
        spacing: Theme.spacing4

        Row {
            width: parent.width
            spacing: Theme.spacing12

            Text {
                id: name

                width: Math.min(implicitWidth, parent.width - stamp.width - parent.spacing)
                text: root.block.name
                color: Theme.textMuted
                elide: Text.ElideRight
                font.family: Theme.displayFamily
                font.weight: Theme.displayWeightExtraBold
                font.pixelSize: Theme.skippedNameSize
                font.capitalization: Font.AllUppercase
                font.strikeout: true
            }

            Text {
                id: stamp
                objectName: "stamp"

                anchors.verticalCenter: name.verticalCenter
                text: root.block.skippedAt.length > 0 ? qsTr("SKIPPED AT %1").arg(root.block.skippedAt)
                                                      : root.block.stateText.toUpperCase()
                color: Theme.alert
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightSemiBold
                font.pixelSize: Theme.labelSize
                font.letterSpacing: 1
            }
        }

        Text {
            id: caption
            objectName: "caption"

            width: parent.width
            text: root.block.restoreCaption
            color: Theme.textSoft
            wrapMode: Text.Wrap
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightRegular
            font.pixelSize: Theme.captionSize
        }
    }

    SignalButton {
        id: action
        objectName: "action"

        anchors {
            right: parent.right
            rightMargin: Theme.spacing20
            verticalCenter: parent.verticalCenter
        }
        visible: root.restorable
        width: visible ? implicitWidth : 0
        kind: root.anchored ? "primary" : "outline"
        display: root.anchored
        fontSize: root.anchored ? Theme.displaySizeButton : Theme.bodySize
        implicitHeight: root.anchored ? Theme.buttonHeightMedium : Theme.touchTarget
        text: root.anchored ? qsTr("RESTORE") : qsTr("Restore")
        onClicked: DayController.restore(root.block.index)
    }
}
