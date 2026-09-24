pragma ComponentBehavior: Bound
import QtQuick
import Cadence
import Cadence.Theme

// Preferences in four sections. Every control writes straight to the Settings singleton, which
// persists and applies the change at once.
Item {
    id: root

    // The window owns the playlist picker; the card only says which activity wants one.
    signal choosePlaylistRequested(string activityId, string activityName)

    function choosePlaylistFor(activityId, activityName) {
        root.choosePlaylistRequested(activityId, activityName);
    }

    component SectionTitle: Text {
        color: Theme.textMuted
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightMedium
        font.pixelSize: Theme.labelSize
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1
    }

    Flickable {
        id: page

        anchors.fill: parent
        clip: true
        contentHeight: sections.height + Theme.spacing40
        boundsBehavior: Flickable.StopAtBounds

        // A thin indicator at the right edge shows there is more below the fold.
        Rectangle {
            x: page.width - width
            y: page.contentY + page.visibleArea.yPosition * page.height
            width: Theme.spacing4
            height: Math.max(Theme.touchTarget, page.visibleArea.heightRatio * page.height)
            radius: Theme.pill
            color: Theme.line
            visible: page.contentHeight > page.height
        }

        Row {
            id: sections

            width: Math.min(parent.width, Theme.contentMaxWidth)
            spacing: Theme.spacing40

            // Startup, alarms and push-ups on the left; the music card on the right so the whole
            // screen fits the baseline window.
            Column {
                id: leftColumn
                width: Theme.panelWidth + Theme.spacing40
                spacing: Theme.spacing32

                Column {
                    spacing: Theme.spacing16

                    SectionTitle {
                        text: qsTr("Startup")
                    }

                    SignalToggle {
                        label: qsTr("Launch at login")
                        checked: Settings.launchAtLogin
                        onToggled: function (checked) {
                            Settings.launchAtLogin = checked;
                        }
                    }

                    SignalToggle {
                        label: qsTr("Start minimized to tray")
                        checked: Settings.startMinimized
                        onToggled: function (checked) {
                            Settings.startMinimized = checked;
                        }
                    }
                }

                Column {
                    spacing: Theme.spacing16

                    SectionTitle {
                        text: qsTr("Alarms")
                    }

                    SignalChoice {
                        label: qsTr("Sound")
                        options: [qsTr("Default"), qsTr("Soft"), qsTr("Silent")]
                        values: ["Default", "Soft", "Silent"]
                        current: Settings.sound
                        onChosen: function (option) {
                            Settings.sound = option;
                        }
                    }

                    SignalField {
                        label: qsTr("Max snoozes per block")
                        numeric: true
                        value: String(Settings.maxSnoozes)
                        onCommitted: function (text) {
                            Settings.maxSnoozes = parseInt(text) || 0;
                        }
                    }

                    SignalToggle {
                        label: qsTr("Warn when the day no longer fits")
                        checked: Settings.warnDayNoLongerFits
                        onToggled: function (checked) {
                            Settings.warnDayNoLongerFits = checked;
                        }
                    }
                }

                Column {
                    spacing: Theme.spacing16

                    SectionTitle {
                        text: qsTr("Push-ups")
                    }

                    SignalField {
                        label: qsTr("Default reps")
                        numeric: true
                        value: String(Settings.defaultReps)
                        onCommitted: function (text) {
                            Settings.defaultReps = parseInt(text) || 1;
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

            Column {
                id: rightColumn
                spacing: Theme.spacing16

                SectionTitle {
                    text: qsTr("Music")
                }

                // Spotify card: the user's own app, the fixed redirect, the connection and the
                // playlist per activity.
                Rectangle {
                    id: card
                    width: Theme.panelWidth + Theme.spacing40 * 2
                    height: musicCard.height + Theme.spacing24 * 2
                    radius: Theme.radius
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.line

                    Column {
                        id: musicCard

                        anchors {
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            margins: Theme.spacing24
                        }
                        spacing: Theme.spacing16

                        Text {
                            text: "Spotify"
                            color: Theme.text
                            font.family: Theme.displayFamily
                            font.weight: Theme.displayWeightExtraBold
                            font.pixelSize: Theme.headingSizeMax
                            font.capitalization: Font.AllUppercase
                        }

                        Column {
                            spacing: Theme.spacing4

                            Repeater {
                                model: [qsTr("1. Create an app at developer.spotify.com and select the Web API."), qsTr("2. Add exactly this redirect URI to the app: %1").arg(Spotify.redirectUri), qsTr("3. Paste the app's Client ID below. No client secret is needed."), qsTr("4. Connect and log in. Spotify Premium is required to control playback."), qsTr("5. Development Mode apps allow up to five users; each user registers their own app.")]

                                Text {
                                    id: guideLine

                                    required property string modelData

                                    width: musicCard.width
                                    text: guideLine.modelData
                                    color: Theme.textMuted
                                    wrapMode: Text.Wrap
                                    font.family: Theme.bodyFamily
                                    font.pixelSize: Theme.labelSize
                                }
                            }
                        }

                        SignalField {
                            width: parent.width
                            label: qsTr("Client ID")
                            placeholder: qsTr("32 characters from your Spotify app")
                            value: Settings.spotifyClientId
                            onCommitted: function (text) {
                                Settings.spotifyClientId = text;
                            }
                        }

                        Row {
                            spacing: Theme.spacing12

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: Theme.spacing4

                                Text {
                                    text: qsTr("REDIRECT URI")
                                    color: Theme.textMuted
                                    font.family: Theme.bodyFamily
                                    font.weight: Theme.bodyWeightMedium
                                    font.pixelSize: Theme.labelSize
                                    font.letterSpacing: 1
                                }

                                Text {
                                    text: Spotify.redirectUri
                                    color: Theme.text
                                    font.family: Theme.bodyFamily
                                    font.pixelSize: Theme.bodySize
                                }
                            }

                            SignalButton {
                                anchors.verticalCenter: parent.verticalCenter
                                kind: "outline"
                                text: qsTr("Copy")
                                onClicked: Spotify.copyToClipboard(Spotify.redirectUri)
                            }
                        }

                        Row {
                            spacing: Theme.spacing12

                            SignalButton {
                                text: Spotify.connecting ? qsTr("Connecting") : qsTr("Connect")
                                primary: true
                                visible: !Spotify.connected
                                enabled: Spotify.configured && !Spotify.connecting
                                onClicked: Spotify.connectAccount()
                            }

                            SignalButton {
                                kind: "outline"
                                text: qsTr("Disconnect")
                                visible: Spotify.connected
                                onClicked: Spotify.disconnectAccount()
                            }

                            SignalButton {
                                text: qsTr("Reconnect to update permissions")
                                primary: true
                                visible: Spotify.connected && Spotify.needsReconsent
                                onClicked: Spotify.connectAccount()
                            }
                        }

                        Column {
                            width: parent.width
                            spacing: Theme.spacing4

                            Text {
                                width: parent.width
                                text: Spotify.statusText
                                color: Spotify.connected ? Theme.text : Theme.textMuted
                                wrapMode: Text.Wrap
                                font.family: Theme.bodyFamily
                                font.weight: Theme.bodyWeightMedium
                                font.pixelSize: Theme.bodySize
                            }

                            Text {
                                width: parent.width
                                visible: Spotify.premiumWarning
                                text: qsTr("Spotify Premium is required to control playback")
                                color: Theme.alert
                                wrapMode: Text.Wrap
                                font.family: Theme.bodyFamily
                                font.weight: Theme.bodyWeightMedium
                                font.pixelSize: Theme.bodySize
                            }

                            Text {
                                width: parent.width
                                visible: Spotify.errorText.length > 0
                                text: Spotify.errorText
                                color: Theme.alert
                                wrapMode: Text.Wrap
                                font.family: Theme.bodyFamily
                                font.pixelSize: Theme.labelSize
                            }
                        }

                        SignalToggle {
                            label: qsTr("Start the activity's playlist when its block starts")
                            checked: Settings.spotifyAutoplay
                            onToggled: function (checked) {
                                Settings.spotifyAutoplay = checked;
                            }
                        }

                        Text {
                            text: qsTr("DEFAULT PLAYLIST PER ACTIVITY")
                            color: Theme.textMuted
                            font.family: Theme.bodyFamily
                            font.weight: Theme.bodyWeightMedium
                            font.pixelSize: Theme.labelSize
                            font.letterSpacing: 1
                        }

                        Column {
                            width: parent.width
                            spacing: Theme.spacing8

                            Repeater {
                                model: DayController.activities

                                Row {
                                    id: activityRow

                                    required property var modelData

                                    width: musicCard.width
                                    spacing: Theme.spacing12

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: Theme.fieldWidth
                                        text: activityRow.modelData.name
                                        color: Theme.text
                                        elide: Text.ElideRight
                                        font.family: Theme.displayFamily
                                        font.weight: Theme.displayWeightSemiBold
                                        font.pixelSize: Theme.headingSizeMin
                                        font.capitalization: Font.AllUppercase
                                    }

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: activityRow.width - Theme.fieldWidth - chooseButton.width - clearButton.width - Theme.spacing12 * 3
                                        text: Settings.playlistNameFor(activityRow.modelData.id).length > 0 ? Settings.playlistNameFor(activityRow.modelData.id) : qsTr("None")
                                        color: Theme.textMuted
                                        elide: Text.ElideRight
                                        font.family: Theme.bodyFamily
                                        font.pixelSize: Theme.bodySize
                                    }

                                    SignalButton {
                                        id: chooseButton

                                        text: qsTr("Choose")
                                        enabled: Spotify.connected
                                        onClicked: root.choosePlaylistFor(activityRow.modelData.id, activityRow.modelData.name)
                                    }

                                    SignalButton {
                                        id: clearButton

                                        kind: "outline"
                                        text: qsTr("Clear")
                                        enabled: Settings.playlistUriFor(activityRow.modelData.id).length > 0
                                        onClicked: Settings.clearPlaylistFor(activityRow.modelData.id)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
