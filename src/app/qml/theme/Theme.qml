pragma Singleton
import QtQuick

// Signal design tokens. Every visual value used by the app comes from here.
QtObject {
    // Colors
    readonly property color bg: "#0B0B0B"
    readonly property color surface: "#1A1A19"
    readonly property color surfaceMuted: "#2A2A28"
    readonly property color line: "#3A3A37"
    readonly property color textMuted: "#8F8C84"
    readonly property color text: "#F2F0EA"
    readonly property color accent: "#D6FF3F"
    // The token is called onAccent in the design spec; QML reserves on<Name> for signal handlers.
    readonly property color textOnAccent: "#0B0B0B"
    readonly property color alert: "#FF6B3D"
    // Muted text on the accent surface, for the secondary line of the active block and the break overlay.
    readonly property color textOnAccentMuted: "#3A4410"

    // Fonts. The families are registered from the bundled TTFs at startup.
    readonly property string displayFamily: "Big Shoulders Display"
    readonly property int displayWeightSemiBold: 600
    readonly property int displayWeightExtraBold: 800

    readonly property string bodyFamily: "Archivo"
    readonly property int bodyWeightRegular: 400
    readonly property int bodyWeightMedium: 500
    readonly property int bodyWeightSemiBold: 600

    // Type scale in pixels. Ranges scale with the available space.
    readonly property int timerSizeMin: 160
    readonly property int timerSizeMax: 420
    readonly property int displaySizeMin: 56
    readonly property int displaySizeMax: 96
    readonly property int headingSizeMin: 26
    readonly property int headingSizeMax: 32
    readonly property int wordmarkSize: 28
    readonly property int blockNameSize: 32
    readonly property int blockTimerSize: 160
    readonly property int overlayHeadlineSize: 88
    readonly property int bodySize: 15
    readonly property int labelSize: 13

    // Shape and spacing
    readonly property int radius: 4
    readonly property int pill: 999
    readonly property int spacing4: 4
    readonly property int spacing8: 8
    readonly property int spacing12: 12
    readonly property int spacing16: 16
    readonly property int spacing20: 20
    readonly property int spacing24: 24
    readonly property int spacing32: 32
    readonly property int spacing40: 40
    readonly property int touchTarget: 44
    readonly property int buttonHeightLarge: 64
    readonly property int outlineWidth: 2

    // Layout
    readonly property int topBarHeight: 72
    readonly property int sideColumnWidth: 360
    readonly property int blockMinHeight: 56
    readonly property int currentBlockMinHeight: 240
    readonly property int progressBarHeight: 8
    readonly property int pomodoroColumns: 7
    readonly property int fieldWidth: 120
    readonly property int panelWidth: 420
    readonly property int contentMaxWidth: 1400
}
