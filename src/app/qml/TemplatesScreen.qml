pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
import Cadence
import Cadence.Theme

// Week template editor: weekday pills and the block list on the left, the selected block's
// fields on the right. Every edit goes straight to TemplateEditor, which validates through core.
Item {
    id: root

    readonly property var dayNames: ["MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"]
    readonly property int rowStep: Theme.buttonHeightLarge + Theme.spacing8
    readonly property var block: TemplateEditor.block
    readonly property bool hasBlock: TemplateEditor.selectedBlock >= 0
    readonly property var dayIssues: TemplateEditor.issues.filter(issue => issue.day === TemplateEditor.selectedDay)
    readonly property int otherIssues: TemplateEditor.issues.length - root.dayIssues.length

    function blockIssues(index) {
        return root.dayIssues.filter(issue => issue.block === index)
    }

    Column {
        id: left

        anchors {
            left: parent.left
            right: panel.left
            rightMargin: Theme.spacing40
            top: parent.top
            bottom: parent.bottom
        }
        spacing: Theme.spacing16

        Row {
            spacing: Theme.spacing8

            Repeater {
                model: 7

                Rectangle {
                    id: dayPill

                    required property int index
                    readonly property bool planned: TemplateEditor.plannedDays[dayPill.index]
                    readonly property bool active: TemplateEditor.selectedDay === dayPill.index

                    width: Theme.buttonHeightLarge
                    height: Theme.touchTarget
                    radius: Theme.pill
                    color: dayPill.active ? Theme.text : dayPill.planned ? Theme.surfaceMuted : "transparent"

                    Shape {
                        anchors.fill: parent
                        visible: !dayPill.planned && !dayPill.active
                        preferredRendererType: Shape.CurveRenderer

                        ShapePath {
                            strokeColor: Theme.line
                            strokeWidth: 1
                            strokeStyle: ShapePath.DashLine
                            dashPattern: [4, 3]
                            fillColor: "transparent"
                            startX: dayPill.height / 2
                            startY: 0.5
                            PathLine { x: dayPill.width - dayPill.height / 2; y: 0.5 }
                            PathArc { x: dayPill.width - dayPill.height / 2; y: dayPill.height - 0.5; radiusX: dayPill.height / 2; radiusY: dayPill.height / 2 }
                            PathLine { x: dayPill.height / 2; y: dayPill.height - 0.5 }
                            PathArc { x: dayPill.height / 2; y: 0.5; radiusX: dayPill.height / 2; radiusY: dayPill.height / 2 }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: root.dayNames[dayPill.index]
                        color: dayPill.active ? Theme.textOnAccent : dayPill.planned ? Theme.text : Theme.textMuted
                        font.family: Theme.bodyFamily
                        font.weight: Theme.bodyWeightSemiBold
                        font.pixelSize: Theme.labelSize
                        font.letterSpacing: 1
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: TemplateEditor.selectDay(dayPill.index)
                    }
                }
            }
        }

        Row {
            spacing: Theme.spacing12

            SignalButton {
                anchors.bottom: parent.bottom
                text: "Copy Mon to Tue-Fri"
                onClicked: TemplateEditor.copyMondayToWeekdays()
            }

            SignalButton {
                anchors.bottom: parent.bottom
                kind: "outline"
                text: TemplateEditor.dayPlanned ? "Make free day" : "Plan this day"
                onClicked: TemplateEditor.setDayPlanned(!TemplateEditor.dayPlanned)
            }

            SignalField {
                label: "Day start"
                value: TemplateEditor.dayStartText
                placeholder: "08:30"
                visible: TemplateEditor.dayPlanned
                error: root.dayIssues.some(issue => issue.location.endsWith("dayStart"))
                onCommitted: function(text) { TemplateEditor.setDayStart(text) }
            }

            SignalField {
                label: "Cutoff"
                value: TemplateEditor.dayCutoffText
                placeholder: "23:00"
                visible: TemplateEditor.dayPlanned
                error: root.dayIssues.some(issue => issue.location.endsWith("dayCutoff"))
                onCommitted: function(text) { TemplateEditor.setDayCutoff(text) }
            }
        }

        Text {
            visible: !TemplateEditor.dayPlanned
            text: "FREE DAY"
            color: Theme.textMuted
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.pixelSize: Theme.displaySizeMin
        }

        Flickable {
            id: list

            width: parent.width
            height: parent.height - y - addButton.height - Theme.spacing16
            visible: TemplateEditor.dayPlanned
            clip: true
            contentHeight: rows.height
            boundsBehavior: Flickable.StopAtBounds

            Item {
                id: rows

                width: list.width
                height: Math.max(0, TemplateEditor.blocks.length * root.rowStep - Theme.spacing8)

                Repeater {
                    model: TemplateEditor.blocks

                    Rectangle {
                        id: row

                        required property var modelData
                        required property int index
                        readonly property bool selected: TemplateEditor.selectedBlock === row.index

                        width: rows.width
                        height: Theme.buttonHeightLarge
                        y: row.index * root.rowStep
                        radius: Theme.radius
                        color: row.selected ? Theme.surfaceMuted : Theme.surface
                        border.width: row.selected ? Theme.outlineWidth : 0
                        border.color: Theme.accent

                        MouseArea {
                            anchors.fill: parent
                            onClicked: TemplateEditor.selectBlock(row.index)
                        }

                        // Drag handle: the row follows the pointer and lands on the slot it is over.
                        Item {
                            id: handle

                            anchors {
                                left: parent.left
                                top: parent.top
                                bottom: parent.bottom
                            }
                            width: Theme.touchTarget

                            Column {
                                anchors.centerIn: parent
                                spacing: Theme.spacing4

                                Repeater {
                                    model: 3

                                    Rectangle {
                                        width: Theme.spacing16
                                        height: 2
                                        color: Theme.textMuted
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.SizeVerCursor
                                drag.target: row
                                drag.axis: Drag.YAxis
                                drag.minimumY: 0
                                drag.maximumY: Math.max(0, rows.height - row.height)
                                onPressed: {
                                    row.z = 1
                                    TemplateEditor.selectBlock(row.index)
                                }
                                onReleased: {
                                    const target = Math.round(row.y / root.rowStep)
                                    row.z = 0
                                    row.y = Qt.binding(() => row.index * root.rowStep)
                                    TemplateEditor.moveBlock(row.index, target)
                                }
                            }
                        }

                        Text {
                            id: rowName

                            anchors {
                                left: handle.right
                                leftMargin: Theme.spacing8
                                verticalCenter: parent.verticalCenter
                            }
                            width: Math.min(implicitWidth, parent.width - handle.width - rowRight.width - Theme.spacing32)
                            text: row.modelData.name
                            color: Theme.text
                            elide: Text.ElideRight
                            font.family: Theme.displayFamily
                            font.weight: Theme.displayWeightSemiBold
                            font.pixelSize: Theme.headingSizeMax
                            font.capitalization: Font.AllUppercase
                        }

                        Row {
                            id: rowRight

                            anchors {
                                right: parent.right
                                rightMargin: Theme.spacing16
                                verticalCenter: parent.verticalCenter
                            }
                            spacing: Theme.spacing16

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: row.modelData.hasIssue
                                width: Theme.spacing8
                                height: Theme.spacing8
                                radius: Theme.pill
                                color: Theme.alert
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: row.modelData.detail
                                color: Theme.textMuted
                                font.family: Theme.bodyFamily
                                font.pixelSize: Theme.labelSize
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: row.modelData.kind
                                color: Theme.textMuted
                                font.family: Theme.bodyFamily
                                font.weight: Theme.bodyWeightSemiBold
                                font.pixelSize: Theme.labelSize
                                font.capitalization: Font.AllUppercase
                                font.letterSpacing: 1
                            }
                        }
                    }
                }
            }
        }

        SignalButton {
            id: addButton

            kind: "outline"
            text: "+ Add block"
            visible: TemplateEditor.dayPlanned
            onClicked: TemplateEditor.addBlock()
        }
    }

    // Right panel: the selected block.
    Flickable {
        id: panel

        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
        }
        width: Theme.panelWidth
        clip: true
        contentHeight: fields.height
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: fields

            width: panel.width
            spacing: Theme.spacing16

            Text {
                text: root.hasBlock ? "BLOCK" : "NO BLOCK SELECTED"
                color: Theme.textMuted
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightMedium
                font.pixelSize: Theme.labelSize
                font.letterSpacing: 1
            }

            SignalField {
                width: parent.width
                visible: root.hasBlock
                label: "Name"
                value: root.hasBlock ? root.block.name : ""
                onCommitted: function(text) { TemplateEditor.setBlockName(TemplateEditor.selectedBlock, text) }
            }

            SignalChoice {
                visible: root.hasBlock
                label: "Timing"
                options: ["Flexible", "Anchored", "Soft"]
                current: root.hasBlock ? root.block.kind : ""
                onChosen: function(option) { TemplateEditor.setBlockKind(TemplateEditor.selectedBlock, option) }
            }

            Row {
                spacing: Theme.spacing12
                visible: root.hasBlock

                SignalField {
                    visible: root.hasBlock && root.block.kind !== "Flexible"
                    label: root.hasBlock && root.block.kind === "Soft" ? "Earliest start" : "Start"
                    value: root.hasBlock ? root.block.startText : ""
                    placeholder: "09:00"
                    error: root.blockIssues(TemplateEditor.selectedBlock).some(issue => issue.message.indexOf("start") >= 0)
                    onCommitted: function(text) { TemplateEditor.setBlockStart(TemplateEditor.selectedBlock, text) }
                }

                SignalField {
                    label: "Duration (min)"
                    numeric: true
                    value: root.hasBlock && root.block.durationMinutes > 0 ? String(root.block.durationMinutes) : ""
                    placeholder: root.hasBlock && root.block.pomodoroEnabled ? "from plan" : "60"
                    error: root.blockIssues(TemplateEditor.selectedBlock).some(issue => issue.message.indexOf("duration") >= 0)
                    onCommitted: function(text) { TemplateEditor.setBlockDuration(TemplateEditor.selectedBlock, parseInt(text) || 0) }
                }
            }

            SignalToggle {
                visible: root.hasBlock
                label: "Pomodoros"
                checked: root.hasBlock && root.block.pomodoroEnabled
                onToggled: function(checked) { TemplateEditor.setBlockPomodoroEnabled(TemplateEditor.selectedBlock, checked) }
            }

            Row {
                spacing: Theme.spacing12
                visible: root.hasBlock && root.block.pomodoroEnabled

                SignalField {
                    label: "Count"
                    numeric: true
                    value: root.hasBlock && root.block.pomodoroCount > 0 ? String(root.block.pomodoroCount) : ""
                    placeholder: root.hasBlock ? "fit " + root.block.resolvedCount : ""
                    onCommitted: function(text) { TemplateEditor.setBlockPomodoroCount(TemplateEditor.selectedBlock, parseInt(text) || 0) }
                }

                SignalField {
                    label: "Focus"
                    numeric: true
                    value: root.hasBlock ? String(root.block.focus) : ""
                    onCommitted: function(text) { TemplateEditor.setBlockFocus(TemplateEditor.selectedBlock, parseInt(text) || 0) }
                }
            }

            Row {
                spacing: Theme.spacing12
                visible: root.hasBlock && root.block.pomodoroEnabled

                SignalField {
                    label: "Short break"
                    numeric: true
                    value: root.hasBlock ? String(root.block.shortBreak) : ""
                    onCommitted: function(text) { TemplateEditor.setBlockShortBreak(TemplateEditor.selectedBlock, parseInt(text) || 0) }
                }

                SignalField {
                    label: "Long break"
                    numeric: true
                    value: root.hasBlock ? String(root.block.longBreak) : ""
                    onCommitted: function(text) { TemplateEditor.setBlockLongBreak(TemplateEditor.selectedBlock, parseInt(text) || 0) }
                }

                SignalField {
                    label: "Long every"
                    numeric: true
                    value: root.hasBlock ? String(root.block.longBreakEvery) : ""
                    onCommitted: function(text) { TemplateEditor.setBlockLongBreakEvery(TemplateEditor.selectedBlock, parseInt(text) || 0) }
                }
            }

            SignalToggle {
                visible: root.hasBlock && root.block.pomodoroEnabled
                label: "Push-ups on each break"
                checked: root.hasBlock && root.block.pushups
                onToggled: function(checked) { TemplateEditor.setBlockPushups(TemplateEditor.selectedBlock, checked) }
            }

            SignalToggle {
                visible: root.hasBlock
                label: "Full-screen alarm at start"
                checked: root.hasBlock && root.block.fullscreenAlarm
                onToggled: function(checked) { TemplateEditor.setBlockFullscreenAlarm(TemplateEditor.selectedBlock, checked) }
            }

            Item {
                width: 1
                height: Theme.spacing8
            }

            Column {
                width: parent.width
                spacing: Theme.spacing4

                Repeater {
                    model: root.dayIssues

                    Text {
                        id: issueText

                        required property var modelData

                        width: fields.width
                        text: (issueText.modelData.block >= 0 ? "Block " + (issueText.modelData.block + 1) + ": " : "")
                              + issueText.modelData.message
                        color: Theme.alert
                        wrapMode: Text.Wrap
                        font.family: Theme.bodyFamily
                        font.pixelSize: Theme.labelSize
                    }
                }

                Text {
                    visible: root.otherIssues > 0
                    text: root.otherIssues === 1 ? "1 problem on another day" : root.otherIssues + " problems on other days"
                    color: Theme.alert
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.labelSize
                }

                Text {
                    visible: TemplateEditor.error.length > 0
                    width: fields.width
                    text: TemplateEditor.error
                    color: Theme.alert
                    wrapMode: Text.Wrap
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.labelSize
                }
            }

            Row {
                spacing: Theme.spacing12

                SignalButton {
                    text: "Save"
                    primary: true
                    enabled: TemplateEditor.dirty && TemplateEditor.valid
                    onClicked: TemplateEditor.save()
                }

                SignalButton {
                    kind: "outline"
                    text: "Revert"
                    enabled: TemplateEditor.dirty
                    onClicked: TemplateEditor.revert()
                }

                SignalButton {
                    kind: "outline"
                    text: "Delete"
                    textColor: Theme.alert
                    visible: root.hasBlock
                    onClicked: TemplateEditor.deleteBlock(TemplateEditor.selectedBlock)
                }
            }

            Text {
                visible: !TemplateEditor.dirty
                text: "Saved"
                color: Theme.textMuted
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.labelSize
            }
        }
    }
}
