pragma ComponentBehavior: Bound
import QtQuick
import Cadence
import Cadence.Theme

// The block's pomodoros as cells, seven per row: done cells fill with the accent, the running one
// is outlined and the rest stay muted.
Grid {
    id: root

    readonly property int total: DayController.pomodoroTotal
    readonly property int done: DayController.pomodorosDone
    readonly property int current: DayController.hasPomodoro && DayController.pomodoroPhase !== "Completed"
                                   ? DayController.pomodorosDone : -1
    readonly property int cellSize: Math.floor((width - (Theme.pomodoroColumns - 1) * Theme.spacing8)
                                               / Theme.pomodoroColumns)

    columns: Theme.pomodoroColumns
    spacing: Theme.spacing8

    Repeater {
        model: root.total

        Rectangle {
            id: cell

            required property int index

            width: root.cellSize
            height: root.cellSize
            radius: Theme.radius
            color: cell.index < root.done ? Theme.accent : Theme.surfaceMuted
            border.width: cell.index === root.current ? Theme.outlineWidth : 0
            border.color: Theme.accent
        }
    }
}
