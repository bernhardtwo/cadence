import QtQuick
import QtTest
import Cadence
import Cadence.Theme

// The Restore action of a skipped row: shown only while the controller says the block can be
// restored, styled by block kind, and a click asks the controller for that block and nothing else.
TestCase {
    id: root

    name: "SkippedBlockRow"
    when: windowShown
    visible: true
    width: 560
    height: 200

    property var rowComponent: null

    function init() {
        DayController.reset()
        if (root.rowComponent === null) {
            root.rowComponent = Qt.createComponent(Qt.resolvedUrl("../../src/app/qml/SkippedBlockRow.qml"))
        }
        compare(root.rowComponent.status, Component.Ready, root.rowComponent.errorString())
    }

    function block(overrides) {
        const base = {
            index: 0,
            name: "Work",
            kind: "Anchored",
            state: "Skipped",
            stateText: "Skipped",
            skippedAt: "12:05",
            restorable: true,
            restoreCaption: "08:30 to 15:00 · 3:35 done · restoring continues at the current time with 2:50 left, 5 pomodoros."
        }
        return Object.assign(base, overrides)
    }

    function makeRow(data) {
        const row = root.rowComponent.createObject(root, { block: data, width: root.width })
        verify(row !== null)
        row.height = row.implicitHeight
        return row
    }

    function action(row) {
        return findChild(row, "action")
    }

    function test_restore_shown_only_when_allowed() {
        const open = makeRow(block({}))
        verify(action(open).visible)
        compare(action(open).text, "RESTORE")
        open.destroy()

        const closed = makeRow(block({ restorable: false, restoreCaption: "window closed" }))
        verify(!action(closed).visible)
        compare(action(closed).width, 0)
        const caption = findChild(closed, "caption")
        compare(caption.text, "window closed")
        closed.destroy()
    }

    function test_click_restores_that_block() {
        const row = makeRow(block({ index: 3 }))
        const button = action(row)
        compare(DayController.restoreCount, 0)
        mouseClick(button)
        compare(DayController.restoreCount, 1)
        compare(DayController.lastRestored, 3)
        row.destroy()
    }

    function test_flexible_row_gets_the_quiet_button() {
        const row = makeRow(block({ index: 1, name: "Lunch", kind: "Flexible", skippedAt: "11:50",
                                    restoreCaption: "Skipped at 11:50 · flexible, restoring puts it back in the queue after French." }))
        const button = action(row)
        verify(button.visible)
        compare(button.text, "Restore")
        compare(button.height, Theme.touchTarget)
        compare(button.effectiveKind, "outline")
        mouseClick(button)
        compare(DayController.lastRestored, 1)
        row.destroy()
    }

    function test_stamp_falls_back_to_the_state_without_a_time() {
        const row = makeRow(block({ skippedAt: "" }))
        compare(findChild(row, "stamp").text, "SKIPPED")
        row.destroy()
    }
}
