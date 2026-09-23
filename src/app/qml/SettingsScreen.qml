import QtQuick
import Cadence
import Cadence.Theme

// Preferences in four sections. Every control writes straight to the Settings singleton, which
// persists and applies the change at once.
Item {
    id: root

    component SectionTitle: Text {
        color: Theme.textMuted
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightMedium
        font.pixelSize: Theme.labelSize
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
    }

    Flickable {
        anchors.fill: parent
        clip: true
        contentHeight: sections.height
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: sections

            width: Math.min(parent.width, Theme.contentMaxWidth)
            spacing: Theme.spacing32

            Column {
                spacing: Theme.spacing16

                SectionTitle { text: "Startup" }

                SignalToggle {
                    label: "Launch at login"
                    checked: Settings.launchAtLogin
                    onToggled: function(checked) { Settings.launchAtLogin = checked }
                }

                SignalToggle {
                    label: "Start minimized to tray"
                    checked: Settings.startMinimized
                    onToggled: function(checked) { Settings.startMinimized = checked }
                }
            }

            Column {
                spacing: Theme.spacing16

                SectionTitle { text: "Alarms" }

                SignalChoice {
                    label: "Sound"
                    options: ["Default", "Soft", "Silent"]
                    current: Settings.sound
                    onChosen: function(option) { Settings.sound = option }
                }

                SignalField {
                    label: "Max snoozes per block"
                    numeric: true
                    value: String(Settings.maxSnoozes)
                    onCommitted: function(text) { Settings.maxSnoozes = parseInt(text) || 0 }
                }

                SignalToggle {
                    label: "Warn when the day no longer fits"
                    checked: Settings.warnDayNoLongerFits
                    onToggled: function(checked) { Settings.warnDayNoLongerFits = checked }
                }
            }

            Column {
                spacing: Theme.spacing16

                SectionTitle { text: "Push-ups" }

                SignalField {
                    label: "Default reps"
                    numeric: true
                    value: String(Settings.defaultReps)
                    onCommitted: function(text) { Settings.defaultReps = parseInt(text) || 1 }
                }
            }

            Column {
                spacing: Theme.spacing16

                SectionTitle { text: "Music" }

                Rectangle {
                    width: Theme.panelWidth
                    height: Theme.buttonHeightLarge + Theme.spacing24
                    radius: Theme.radius
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.line
                    opacity: 0.5

                    Column {
                        anchors {
                            left: parent.left
                            leftMargin: Theme.spacing16
                            verticalCenter: parent.verticalCenter
                        }
                        spacing: Theme.spacing4

                        Text {
                            text: "Spotify: coming soon"
                            color: Theme.text
                            font.family: Theme.bodyFamily
                            font.weight: Theme.bodyWeightSemiBold
                            font.pixelSize: Theme.bodySize
                        }

                        Text {
                            text: "Focus playlists and break silence arrive with the music milestone"
                            color: Theme.textMuted
                            font.family: Theme.bodyFamily
                            font.pixelSize: Theme.labelSize
                        }
                    }
                }
            }

            Column {
                spacing: Theme.spacing4

                Text {
                    text: Settings.path
                    color: Theme.textMuted
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.labelSize
                }

                Text {
                    visible: Settings.error.length > 0
                    width: sections.width
                    text: Settings.error
                    color: Theme.alert
                    wrapMode: Text.WrapAnywhere
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.labelSize
                }
            }
        }
    }
}
