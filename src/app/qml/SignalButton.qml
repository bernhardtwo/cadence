import QtQuick
import Cadence.Theme

// Every control we render is our own; this is the only button in the app.
// kind: "surface" (default), "primary" (accent fill), "light" (text fill), "outline", "dark".
Rectangle {
    id: root

    property string text
    property string kind: "surface"
    property bool primary: false
    property bool display: false
    property int fontSize: root.display ? Theme.headingSizeMax : Theme.bodySize
    property color textColor: root.effectiveKind === "primary" || root.effectiveKind === "light" ? Theme.textOnAccent
                            : root.effectiveKind === "dark" ? Theme.accent : Theme.text
    property color outlineColor: Theme.line

    readonly property string effectiveKind: root.primary ? "primary" : root.kind

    signal clicked()

    implicitWidth: label.implicitWidth + Theme.spacing24 * 2
    implicitHeight: root.display ? Theme.buttonHeightLarge : Theme.touchTarget
    radius: Theme.radius
    color: root.effectiveKind === "primary" ? Theme.accent
         : root.effectiveKind === "light" ? Theme.text
         : root.effectiveKind === "dark" ? Theme.bg
         : root.effectiveKind === "outline" ? "transparent"
         : Theme.surfaceMuted
    opacity: enabled ? 1 : 0.35
    border.width: root.effectiveKind === "outline" || root.effectiveKind === "surface" ? 1 : 0
    border.color: root.outlineColor

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: root.clicked()

    Text {
        id: label

        anchors.centerIn: parent
        text: root.text
        color: root.textColor
        font.family: root.display ? Theme.displayFamily : Theme.bodyFamily
        font.weight: root.display ? Theme.displayWeightExtraBold : Theme.bodyWeightSemiBold
        font.pixelSize: root.fontSize
        font.letterSpacing: root.display ? 1 : 0
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
