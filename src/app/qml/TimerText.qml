pragma ComponentBehavior: Bound
import QtQuick
import Cadence.Theme

// A time string whose digits stay put while they count. Big Shoulders Display ships no tabular
// figures (its GSUB table has no tnum feature), so a plain Text shifts every second: a "1" is
// about half as wide as a "4". Here every digit gets its own cell as wide as the widest digit of
// the current font, centered like a tabular figure would be, and every run of other characters
// keeps its natural width and shaping. Cell edges are whole pixels so a glyph never lands on a
// different subpixel offset from one second to the next. Because each digit is its own Text,
// kerning between digits cannot move them either.
Item {
    id: root

    property string text: ""
    property color color: Theme.text
    property font font
    // Text.AlignLeft, Text.AlignRight or Text.AlignHCenter; applies when width exceeds the content.
    property int horizontalAlignment: Text.AlignLeft

    // Width of a digit cell: the widest digit of the font, rounded up to whole pixels.
    property int digitWidth: 0
    // One entry per digit and per run of other characters: { digit, start, length }. Replaced only
    // when the shape of the string changes, so a tick that changes digits updates cells in place.
    property var runs: []

    implicitWidth: row.width
    implicitHeight: row.height

    onTextChanged: root.updateRuns()
    Component.onCompleted: root.updateRuns()

    function isDigit(character) {
        return character >= "0" && character <= "9"
    }

    function updateRuns() {
        const next = []
        let index = 0
        while (index < root.text.length) {
            const digit = root.isDigit(root.text[index])
            let end = index + 1
            if (!digit) {
                while (end < root.text.length && !root.isDigit(root.text[end])) {
                    end += 1
                }
            }
            next.push({ digit: digit, start: index, length: end - index })
            index = end
        }
        if (JSON.stringify(next) !== JSON.stringify(root.runs)) {
            root.runs = next
        }
    }

    function widestDigit() {
        let widest = 0
        for (const digit of "0123456789") {
            widest = Math.max(widest, metrics.advanceWidth(digit))
        }
        return Math.ceil(widest)
    }

    // Every cell with its text and position in root coordinates, for tests and debugging. The Row
    // positions its children lazily, so the layout is forced first to report current values.
    function cells() {
        row.forceLayout()
        const out = []
        for (const child of row.children) {
            if ("part" in child) {
                out.push({
                    text: child.part,
                    x: row.x + child.x,
                    width: child.width,
                    glyphX: row.x + child.x + child.glyphX,
                    glyphWidth: child.glyphWidth
                })
            }
        }
        return out
    }

    FontMetrics {
        id: metrics

        font: root.font
        // Imperative on purpose: a binding could read the metrics before their font had updated.
        onFontChanged: root.digitWidth = root.widestDigit()
        Component.onCompleted: root.digitWidth = root.widestDigit()
    }

    Row {
        id: row

        x: root.horizontalAlignment === Text.AlignRight ? root.width - row.width
         : root.horizontalAlignment === Text.AlignHCenter ? Math.round((root.width - row.width) / 2)
         : 0

        Repeater {
            model: root.runs

            Item {
                id: cell

                required property var modelData

                readonly property string part: root.text.slice(cell.modelData.start, cell.modelData.start + cell.modelData.length)
                readonly property real glyphX: glyph.x
                readonly property real glyphWidth: glyph.implicitWidth

                width: cell.modelData.digit ? root.digitWidth : Math.ceil(glyph.implicitWidth)
                height: glyph.implicitHeight

                Text {
                    id: glyph

                    x: cell.modelData.digit ? Math.round((cell.width - glyph.implicitWidth) / 2) : 0
                    text: cell.part
                    color: root.color
                    font: root.font
                }
            }
        }
    }
}
