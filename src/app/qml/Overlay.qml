import QtQuick
import Cadence
import Cadence.Theme

// Full screen alarm surface. Modes: blockStart, break, done, unconfirmed. The break takes the
// accent as its background; everything else sits on the dark canvas.
Window {
    id: overlay

    property string mode: ""
    property int blockIndex: -1
    property string headline: ""
    property string detail: ""
    property bool showPushups: false
    property int defaultReps: Settings.defaultReps
    property int reps: overlay.defaultReps
    // Keys are ignored for a moment after the overlay appears, so typing momentum in another app
    // cannot dismiss or trigger it. Buttons work at once.
    property bool keysArmed: false
    readonly property int armDelayMs: 2000

    readonly property bool onAccent: overlay.mode === "break"
    // Widest the headline may run while the quote slot on its left keeps its minimum width.
    readonly property int headlineMaxWidth: overlay.width - (Theme.spacing40 * 2 + Theme.quoteMinWidth) * 2
    readonly property color ink: overlay.onAccent ? Theme.textOnAccent : Theme.text
    readonly property color inkMuted: overlay.onAccent ? Theme.textOnAccentMuted : Theme.textMuted
    readonly property color headlineColor: overlay.onAccent ? Theme.textOnAccent : Theme.accent

    title: qsTr("Cadence Alarm")
    color: overlay.onAccent ? Theme.accent : Theme.bg
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    // Not owned by the main window, so it shows even while that one hides in the tray.
    transientParent: null

    function open(newMode, index, newHeadline, newDetail) {
        mode = newMode
        blockIndex = index
        headline = newHeadline
        detail = newDetail
        reps = overlay.defaultReps
        keysArmed = false
        armTimer.restart()
        overlay.openings += 1
        showFullScreen()
        raise()
        requestActivate()
        EventLog.write("overlay shown " + newMode + " block=" + index)
    }

    // Counts the openings: every one is a new appearance for its quote.
    property int openings: 0

    Timer {
        id: armTimer

        interval: overlay.armDelayMs
        onTriggered: overlay.keysArmed = true
    }

    // reason names what closed the overlay: escape, or the action the user took.
    function dismiss(reason) {
        const why = reason === undefined ? "escape" : reason
        if (overlay.mode === "break" && overlay.showPushups && why !== "log-set") {
            DayController.dismissPrompt()
        }
        EventLog.write("overlay " + (why === "escape" ? "dismissed" : "action") + " " + overlay.mode + " " + why)
        showPushups = false
        hide()
    }

    function mmss(seconds) {
        const clamped = Math.max(0, seconds)
        return String(Math.floor(clamped / 60)).padStart(2, "0") + ":" + ("0" + clamped % 60).slice(-2)
    }

    Shortcut {
        sequence: "Escape"
        enabled: overlay.keysArmed
        onActivated: overlay.dismiss()
    }

    // Return only continues; starting a block, logging a set or confirming need a click.
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: overlay.keysArmed && (overlay.mode === "done" || (overlay.mode === "break" && !overlay.showPushups))
        onActivated: overlay.dismiss("continue")
    }

    // Top line: what kind of moment this is, and the countdown while on a break.
    Text {
        anchors {
            left: parent.left
            top: parent.top
            margins: Theme.spacing40
        }
        text: overlay.mode === "break" ? qsTr("BREAK")
            : overlay.mode === "done" ? qsTr("BLOCK DONE")
            : overlay.mode === "blockStart" ? qsTr("BLOCK START")
            : overlay.mode === "unconfirmed" ? qsTr("CONFIRM") : ""
        color: overlay.ink
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightSemiBold
        font.pixelSize: Theme.headingSizeMin
        font.letterSpacing: 2
    }

    TimerText {
        anchors {
            right: parent.right
            top: parent.top
            margins: Theme.spacing40
        }
        horizontalAlignment: Text.AlignRight
        visible: overlay.mode === "break" && DayController.pomodoroOnBreak
        text: overlay.mmss(DayController.pomodoroRemainingSeconds)
        color: overlay.ink
        font.family: Theme.displayFamily
        font.weight: Theme.displayWeightExtraBold
        font.pixelSize: Theme.displaySizeMin
    }

    // Bottom left, level with the bottom of the centered stack and never under it.
    QuoteBlock {
        id: overlayQuote

        readonly property real stackLeft: {
            let left = stack.x + stack.width
            for (const item of [detailText, headlineText, breakRow, doneRow, startRow, confirmRow]) {
                if (item.visible) {
                    const content = item.contentWidth !== undefined ? item.contentWidth : item.width
                    left = Math.min(left, stack.x + (stack.width - content) / 2)
                }
            }
            return left
        }

        anchors {
            left: parent.left
            leftMargin: Theme.spacing40
            bottom: stack.bottom
        }
        width: Math.min(Theme.quoteMaxWidthCard, overlayQuote.stackLeft - Theme.spacing40 * 2)
        shown: overlayQuote.width >= Theme.quoteMinWidth
        surface: "overlay"
        // A push-up prompt turns the break into the push-up surface, which draws from its own group.
        phaseKey: overlay.visible && overlay.mode !== "unconfirmed"
                  ? overlay.openings + "/" + overlay.mode + (overlay.showPushups ? "/pushups" : "")
                  : ""
        context: overlay.mode === "break" ? (overlay.showPushups ? "pushups" : "break")
               : overlay.mode === "blockStart" ? "blockStart" : "blockEnd"
        variant: "overlay"
        showOriginal: Settings.showQuoteOriginals
        quoteColor: overlay.ink
        attributionColor: overlay.onAccent ? Theme.quoteInkOnAccentMuted : Theme.textMuted
        originalColor: overlay.onAccent ? Theme.quoteInkOnAccentMuted : Theme.textMuted
    }

    Column {
        id: stack

        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacing40 * 2, Theme.contentMaxWidth)
        spacing: Theme.spacing24

        Text {
            id: detailText

            width: parent.width
            text: overlay.detail
            color: overlay.inkMuted
            horizontalAlignment: Text.AlignHCenter
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.headingSizeMin
            font.letterSpacing: 2
            font.capitalization: Font.AllUppercase
        }

        Text {
            id: headlineText

            // The largest size the headline may take: the break wording has a fixed size, a
            // block name scales with the screen.
            readonly property int maxSize: overlay.mode === "break" ? Theme.overlayHeadlineSize
                : Math.min(Theme.timerSizeMax, Math.max(Theme.timerSizeMin, Math.round(overlay.height * 0.3)))

            anchors.horizontalCenter: parent.horizontalCenter
            // Narrower than the stack so the quote keeps its slot: every headline, the fixed
            // break one included, shrinks to fit this width.
            width: Math.min(parent.width, overlay.headlineMaxWidth)
            text: overlay.mode === "break" ? (overlay.showPushups ? qsTr("DROP AND GIVE ME") : qsTr("TAKE A BREAK")) : overlay.headline
            // A block name is user content of any length: the headline shrinks until it fits two
            // lines within half the screen. The break headline gets one line of its own size; a
            // fixed box, not implicitHeight, so the fit cannot feed back into the height.
            height: overlay.mode === "break" ? Math.round(Theme.overlayHeadlineSize * 1.25)
                                             : Math.round(overlay.height * 0.5)
            color: overlay.headlineColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.capitalization: Font.AllUppercase

            // Text.Fit stops once two lines fit the height and lets a long word elide, so the
            // size is searched with a probe instead: the largest one at which nothing is cut.
            function refit() {
                const fits = function(size) {
                    headlineProbe.font.pixelSize = size
                    return !headlineProbe.truncated && headlineProbe.contentWidth <= headlineProbe.width
                           && headlineProbe.contentHeight <= headlineProbe.height
                }
                headlineProbe.width = headlineText.width
                headlineProbe.height = headlineText.height
                headlineProbe.text = headlineText.text
                let low = Theme.headingSizeMax
                let high = headlineText.maxSize
                if (fits(high)) {
                    low = high
                }
                while (high - low > 1) {
                    const middle = Math.floor((low + high) / 2)
                    if (fits(middle)) {
                        low = middle
                    } else {
                        high = middle
                    }
                }
                headlineText.font.pixelSize = low
            }

            onTextChanged: headlineText.refit()
            onWidthChanged: headlineText.refit()
            onHeightChanged: headlineText.refit()
            onMaxSizeChanged: headlineText.refit()
            Component.onCompleted: headlineText.refit()
        }

        Text {
            id: headlineProbe

            visible: false
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.capitalization: Font.AllUppercase
        }

        // Push-up stepper: minus, the count in a dark box, plus.
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "break" && overlay.showPushups

            SignalButton {
                anchors.verticalCenter: parent.verticalCenter
                kind: "outline"
                text: "−"
                display: true
                width: Theme.buttonHeightLarge
                textColor: Theme.textOnAccent
                outlineColor: Theme.textOnAccent
                enabled: overlay.reps > 1
                onClicked: overlay.reps -= 1
            }

            Rectangle {
                width: Math.max(Theme.buttonHeightLarge * 3, implicitWidth)
                height: Theme.buttonHeightLarge * 2
                radius: Theme.radius
                color: Theme.bg

                Text {
                    anchors.centerIn: parent
                    text: overlay.reps
                    color: Theme.accent
                    font.family: Theme.displayFamily
                    font.weight: Theme.displayWeightExtraBold
                    font.pixelSize: Theme.displaySizeMax
                }
            }

            SignalButton {
                anchors.verticalCenter: parent.verticalCenter
                kind: "outline"
                text: "+"
                display: true
                width: Theme.buttonHeightLarge
                textColor: Theme.textOnAccent
                outlineColor: Theme.textOnAccent
                onClicked: overlay.reps += 1
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: overlay.mode === "break" && overlay.showPushups
            text: qsTr("Today %1").arg(DayController.pushupsToday)
            color: overlay.inkMuted
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.bodySize
            font.letterSpacing: 1
        }

        Row {
            id: breakRow

            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "break"

            SignalButton {
                visible: overlay.showPushups
                kind: "dark"
                display: true
                text: qsTr("LOG SET")
                width: Math.max(Theme.buttonHeightLarge * 4, implicitWidth)
                onClicked: { DayController.logPushups(overlay.reps); overlay.dismiss("log-set") }
            }

            SignalButton {
                visible: !overlay.showPushups
                kind: "dark"
                display: true
                text: qsTr("CONTINUE")
                width: Math.max(Theme.buttonHeightLarge * 4, implicitWidth)
                onClicked: overlay.dismiss("continue")
            }

            SignalButton {
                kind: "outline"
                text: overlay.showPushups ? qsTr("Skip") : qsTr("Skip break")
                implicitHeight: Theme.buttonHeightLarge
                textColor: Theme.textOnAccent
                outlineColor: Theme.textOnAccent
                onClicked: {
                    if (!overlay.showPushups) {
                        DayController.skipPhase()
                        overlay.dismiss("skip-break")
                    } else {
                        overlay.dismiss("skip")
                    }
                }
            }
        }

        Row {
            id: doneRow

            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "done"

            SignalButton {
                text: qsTr("Continue")
                primary: true
                display: true
                width: Math.max(Theme.buttonHeightLarge * 4, implicitWidth)
                onClicked: overlay.dismiss("continue")
            }
        }

        Row {
            id: startRow

            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "blockStart"

            SignalButton {
                text: qsTr("START")
                primary: true
                display: true
                width: Math.max(Theme.buttonHeightLarge * 4, implicitWidth)
                onClicked: { DayController.start(); overlay.dismiss("start") }
            }

            SignalButton {
                kind: "outline"
                text: qsTr("Snooze 5 min")
                implicitHeight: Theme.buttonHeightLarge
                visible: DayController.canSnooze(overlay.blockIndex)
                onClicked: { DayController.snooze(overlay.blockIndex); overlay.dismiss("snooze") }
            }

            SignalButton {
                kind: "outline"
                text: qsTr("Skip")
                implicitHeight: Theme.buttonHeightLarge
                onClicked: { DayController.skip(); overlay.dismiss("skip-block") }
            }
        }

        Row {
            id: confirmRow

            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing16
            visible: overlay.mode === "unconfirmed"

            SignalButton {
                text: qsTr("YES")
                primary: true
                display: true
                width: Math.max(Theme.buttonHeightLarge * 3, implicitWidth)
                onClicked: { DayController.confirm(overlay.blockIndex, true); overlay.dismiss("confirm-yes") }
            }

            SignalButton {
                kind: "outline"
                text: qsTr("No")
                implicitHeight: Theme.buttonHeightLarge
                width: Math.max(Theme.buttonHeightLarge * 3, implicitWidth)
                onClicked: { DayController.confirm(overlay.blockIndex, false); overlay.dismiss("confirm-no") }
            }
        }
    }

    Text {
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: Theme.spacing40
        }
        text: overlay.keysArmed ? qsTr("Esc to dismiss") : qsTr("Keys enabled in 2 s")
        color: overlay.inkMuted
        font.family: Theme.bodyFamily
        font.pixelSize: Theme.labelSize
    }
}
