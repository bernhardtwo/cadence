import QtQuick
import QtTest
import Cadence.Theme

// Every timer of the app renders through TimerText. The regression covered here: Big Shoulders
// Display has proportional digits, so a countdown drawn as a plain Text shifted a few pixels every
// second. With cells, two strings of the same shape must give the same total width and the same
// cell positions, whatever the digits and the alignment.
TestCase {
    id: root

    name: "TimerText"
    when: windowShown
    width: 1400
    height: 600

    property var timerComponent: null

    // The offscreen platform has no system fonts, so the bundled ones are loaded here.
    FontLoader {
        id: displayFont

        source: Qt.resolvedUrl("../../src/app/fonts/BigShouldersDisplay/BigShouldersDisplay-Variable.ttf")
    }

    FontLoader {
        id: bodyFont

        source: Qt.resolvedUrl("../../src/app/fonts/Archivo/Archivo-Variable.ttf")
    }

    // Every font size and weight a timer of the app uses.
    readonly property var styles: [
        { label: "overlay 56", family: Theme.displayFamily, weight: Theme.displayWeightExtraBold, size: Theme.displaySizeMin },
        { label: "block 96", family: Theme.displayFamily, weight: Theme.displayWeightExtraBold, size: Theme.displaySizeMax },
        { label: "block 160", family: Theme.displayFamily, weight: Theme.displayWeightExtraBold, size: Theme.blockTimerSize },
        { label: "focus 420", family: Theme.displayFamily, weight: Theme.displayWeightExtraBold, size: Theme.timerSizeMax },
        { label: "body 15", family: Theme.bodyFamily, weight: Theme.bodyWeightMedium, size: Theme.bodySize }
    ]

    readonly property var alignments: [
        { label: "left", value: Text.AlignLeft },
        { label: "right", value: Text.AlignRight },
        { label: "center", value: Text.AlignHCenter }
    ]

    function init() {
        compare(displayFont.status, FontLoader.Ready)
        compare(bodyFont.status, FontLoader.Ready)
        if (root.timerComponent === null) {
            root.timerComponent = Qt.createComponent(Qt.resolvedUrl("../../src/app/qml/TimerText.qml"))
        }
        compare(root.timerComponent.status, Component.Ready, root.timerComponent.errorString())
    }

    function makeTimer(style, alignment, text, width) {
        const properties = { text: text, horizontalAlignment: alignment }
        if (width !== undefined) {
            properties.width = width
        }
        const timer = root.timerComponent.createObject(root, properties)
        verify(timer !== null)
        timer.font.family = style.family
        timer.font.weight = style.weight
        timer.font.pixelSize = style.size
        verify(timer.digitWidth > 0, "digit width measured")
        return timer
    }

    function colonOf(cells) {
        for (const cell of cells) {
            if (cell.text === ":") {
                return cell
            }
        }
        fail("no colon cell")
        return null
    }

    function compareCells(first, second, context) {
        const a = first.cells()
        const b = second.cells()
        compare(first.implicitWidth, second.implicitWidth, context + ": total width")
        compare(a.length, b.length, context + ": cell count")
        for (let i = 0; i < a.length; ++i) {
            compare(a[i].x, b[i].x, context + ": cell " + i + " x")
            compare(a[i].width, b[i].width, context + ": cell " + i + " width")
        }
        compare(colonOf(a).glyphX, colonOf(b).glyphX, context + ": colon x")
    }

    function styleRows() {
        const rows = []
        for (const style of root.styles) {
            for (const alignment of root.alignments) {
                rows.push({ tag: style.label + " " + alignment.label, style: style, alignment: alignment.value })
            }
        }
        return rows
    }

    function test_pairs_keep_width_and_positions_data() {
        return styleRows()
    }

    function test_pairs_keep_width_and_positions(data) {
        const pairs = [["18:08", "11:11"], ["10:00", "19:59"], ["00:00", "88:88"]]
        for (const pair of pairs) {
            const first = makeTimer(data.style, data.alignment, pair[0], 1300)
            const second = makeTimer(data.style, data.alignment, pair[1], 1300)
            compareCells(first, second, data.tag + " " + pair[0] + " vs " + pair[1])
            first.destroy()
            second.destroy()
        }
    }

    function test_every_digit_fits_its_cell_data() {
        return root.styles.map(style => ({ tag: style.label, style: style }))
    }

    function test_every_digit_fits_its_cell(data) {
        const timer = makeTimer(data.style, Text.AlignLeft, "0123456789")
        const cells = timer.cells()
        compare(cells.length, 10)
        let previousRight = 0
        for (const cell of cells) {
            compare(cell.width, timer.digitWidth, data.tag + ": digit " + cell.text + " cell width")
            verify(cell.glyphWidth <= cell.width, data.tag + ": digit " + cell.text + " fits")
            verify(cell.glyphX >= cell.x, data.tag + ": digit " + cell.text + " starts inside its cell")
            compare(cell.x, previousRight, data.tag + ": cells are contiguous")
            compare(cell.x, Math.round(cell.x), data.tag + ": cell edge on a whole pixel")
            previousRight = cell.x + cell.width
        }
        timer.destroy()
    }

    function test_countdown_never_moves_the_colon_data() {
        return styleRows()
    }

    function mmss(seconds) {
        return Math.floor(seconds / 60) + ":" + ("0" + seconds % 60).slice(-2)
    }

    function test_countdown_never_moves_the_colon(data) {
        const timer = makeTimer(data.style, data.alignment, mmss(18 * 60 + 8), 1300)
        const colonX = colonOf(timer.cells()).glyphX
        const totalWidth = timer.implicitWidth
        verify(colonX > 0, "colon sits after the minutes")
        // Every second from 18:08 down to 10:00: the shape of the string never changes.
        for (let seconds = 18 * 60 + 7; seconds >= 10 * 60; --seconds) {
            timer.text = mmss(seconds)
            compare(colonOf(timer.cells()).glyphX, colonX, data.tag + " at " + timer.text + ": colon x")
            compare(timer.implicitWidth, totalWidth, data.tag + " at " + timer.text + ": width")
        }
        timer.destroy()
    }

    function test_right_aligned_colon_survives_losing_a_digit() {
        // 10:00 to 9:59 drops the leading digit; anchored on the right, nothing behind it moves.
        const timer = makeTimer(root.styles[2], Text.AlignRight, "10:00", 1300)
        const before = timer.cells()
        timer.text = "9:59"
        const after = timer.cells()
        compare(after.length, before.length - 1)
        compare(colonOf(after).glyphX, colonOf(before).glyphX)
        compare(after[after.length - 1].x, before[before.length - 1].x)
        timer.destroy()
    }

    function test_alignment_places_content_in_width() {
        const timer = makeTimer(root.styles[2], Text.AlignRight, "18:08", 1000)
        let cells = timer.cells()
        compare(cells[cells.length - 1].x + cells[cells.length - 1].width, 1000, "right edge at width")
        timer.horizontalAlignment = Text.AlignHCenter
        cells = timer.cells()
        const left = cells[0].x
        const right = 1000 - (cells[cells.length - 1].x + cells[cells.length - 1].width)
        verify(Math.abs(left - right) <= 1, "centered within a pixel: " + left + " vs " + right)
        compare(left, Math.round(left), "centered content starts on a whole pixel")
        timer.horizontalAlignment = Text.AlignLeft
        compare(timer.cells()[0].x, 0, "left aligned starts at zero")
        timer.destroy()
    }

    function test_letters_keep_their_natural_width() {
        const timer = makeTimer(root.styles[2], Text.AlignLeft, "READY")
        const cells = timer.cells()
        compare(cells.length, 1, "one run for a word")
        compare(cells[0].text, "READY")
        const plain = plainTextComponent.createObject(root, { text: "READY" })
        plain.font.family = root.styles[2].family
        plain.font.weight = root.styles[2].weight
        plain.font.pixelSize = root.styles[2].size
        compare(cells[0].width, Math.ceil(plain.implicitWidth))
        verify(cells[0].width > 0)
        compare(timer.implicitHeight, plain.implicitHeight, "same line height as a plain Text")
        plain.destroy()
        timer.destroy()
    }

    function test_mixed_line_splits_digits_from_words() {
        const timer = makeTimer(root.styles[4], Text.AlignRight, "Block ends 23:40 · 1:36:13 left", 600)
        const texts = timer.cells().map(cell => cell.text)
        compare(texts, ["Block ends ", "2", "3", ":", "4", "0", " · ", "1", ":", "3", "6", ":", "1", "3", " left"])
        const before = timer.cells()
        timer.text = "Block ends 23:40 · 1:36:12 left"
        const after = timer.cells()
        for (let i = 0; i < before.length; ++i) {
            compare(after[i].x, before[i].x, "cell " + i + " x")
        }
        timer.destroy()
    }

    Component {
        id: plainTextComponent

        Text {}
    }
}
