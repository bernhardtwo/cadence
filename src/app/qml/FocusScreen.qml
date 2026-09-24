import QtQuick
import Cadence
import Cadence.Theme

// Full screen view of the running phase: the timer, its progress and nothing else.
Rectangle {
    id: root

    signal exitRequested()

    color: Theme.bg

    readonly property bool pomodoro: DayController.hasPomodoro
    readonly property int remaining: root.pomodoro ? DayController.pomodoroRemainingSeconds : DayController.remainingSeconds
    readonly property int length: root.pomodoro ? DayController.pomodoroPhaseSeconds : DayController.currentDurationMinutes * 60
    readonly property real progress: root.length > 0 ? Math.min(1, Math.max(0, 1 - root.remaining / root.length)) : 0

    function mmss(seconds) {
        const clamped = Math.max(0, seconds)
        const hours = Math.floor(clamped / 3600)
        const minutes = Math.floor(clamped / 60) % 60
        const secs = clamped % 60
        if (hours > 0) {
            return hours + ":" + String(minutes).padStart(2, "0") + ":" + ("0" + secs).slice(-2)
        }
        return String(minutes).padStart(2, "0") + ":" + ("0" + secs).slice(-2)
    }

    Text {
        id: header

        anchors {
            left: parent.left
            top: parent.top
            margins: Theme.spacing40
        }
        text: DayController.currentName.toUpperCase()
              + (root.pomodoro ? " · POMODORO " + DayController.pomodoroIndex + " / " + DayController.pomodoroTotal : "")
        color: Theme.textMuted
        font.family: Theme.bodyFamily
        font.weight: Theme.bodyWeightSemiBold
        font.pixelSize: Theme.bodySize
        font.letterSpacing: 2
    }

    SignalButton {
        anchors {
            right: parent.right
            top: parent.top
            margins: Theme.spacing40
        }
        kind: "outline"
        text: "Exit · Esc"
        onClicked: root.exitRequested()
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacing40 * 2, Theme.contentMaxWidth)
        spacing: Theme.spacing24

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.pomodoro ? DayController.pomodoroPhase.toUpperCase()
                                : DayController.currentActive ? "BLOCK" : "STARTS IN"
            color: Theme.textMuted
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightSemiBold
            font.pixelSize: Theme.headingSizeMin
            font.letterSpacing: 2
        }

        TimerText {
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            text: root.mmss(root.remaining)
            color: DayController.paused ? Theme.textMuted : Theme.accent
            font.family: Theme.displayFamily
            font.weight: Theme.displayWeightExtraBold
            font.pixelSize: Math.min(Theme.timerSizeMax, Math.max(Theme.timerSizeMin, Math.round(root.height * 0.45)))
        }

        Rectangle {
            width: parent.width
            height: Theme.progressBarHeight
            radius: Theme.radius
            color: Theme.surface

            Rectangle {
                width: parent.width * root.progress
                height: parent.height
                radius: Theme.radius
                color: Theme.accent
            }
        }

        TimerText {
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            visible: root.pomodoro
            text: "Block ends " + DayController.currentEndText + " · " + DayController.remainingText + " left"
            color: Theme.textMuted
            font.family: Theme.bodyFamily
            font.weight: Theme.bodyWeightMedium
            font.pixelSize: Theme.bodySize
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: DayController.paused ? "Paused · Space to resume" : "Space to pause"
            color: Theme.textMuted
            font.family: Theme.bodyFamily
            font.pixelSize: Theme.labelSize
        }
    }

    signal pickPlaylistRequested()

    function mmssMs(ms) {
        return root.mmss(Math.floor(Math.max(0, ms) / 1000))
    }

    // Music: what plays, the transport, the progress and the playlist pills.
    Item {
        id: musicRow

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: Theme.spacing40
        }
        height: Theme.buttonHeightLarge * 2
        visible: Spotify.connected

        Rectangle {
            id: cover

            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
            }
            width: Theme.buttonHeightLarge
            height: Theme.buttonHeightLarge
            radius: Theme.radius
            color: Theme.surface

            Image {
                anchors.fill: parent
                source: Spotify.imageUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
        }

        Column {
            id: nowPlaying

            anchors {
                left: cover.right
                leftMargin: Theme.spacing16
                right: transport.left
                rightMargin: Theme.spacing16
                verticalCenter: parent.verticalCenter
            }
            spacing: Theme.spacing4

            Text {
                width: parent.width
                text: Spotify.hasActiveDevice ? Spotify.trackName : "Open Spotify on this PC"
                color: Theme.text
                elide: Text.ElideRight
                font.family: Theme.bodyFamily
                font.weight: Theme.bodyWeightSemiBold
                font.pixelSize: Theme.bodySize
            }

            Text {
                width: parent.width
                text: Spotify.hasActiveDevice ? Spotify.artists : (Spotify.errorText.length > 0 ? Spotify.errorText : "")
                color: Theme.textMuted
                elide: Text.ElideRight
                font.family: Theme.bodyFamily
                font.pixelSize: Theme.labelSize
            }

            Rectangle {
                width: parent.width
                height: Theme.progressBarHeight
                radius: Theme.radius
                color: Theme.surface

                Rectangle {
                    width: Spotify.durationMs > 0 ? parent.width * Math.min(1, Spotify.progressMs / Spotify.durationMs) : 0
                    height: parent.height
                    radius: Theme.radius
                    color: Theme.accent
                }
            }
        }

        Row {
            id: transport

            anchors {
                right: pills.left
                rightMargin: Theme.spacing24
                verticalCenter: parent.verticalCenter
            }
            spacing: Theme.spacing8

            SignalButton {
                visible: Spotify.canTransferHere
                kind: "primary"
                text: "Play here"
                onClicked: Spotify.transferHere()
            }

            SignalButton {
                text: "Prev"
                enabled: Spotify.hasActiveDevice
                onClicked: Spotify.previous()
            }

            SignalButton {
                kind: "light"
                text: Spotify.isPlaying ? "Pause" : "Play"
                enabled: Spotify.hasActiveDevice || Spotify.canTransferHere
                onClicked: Spotify.playPause()
            }

            SignalButton {
                text: "Next"
                enabled: Spotify.hasActiveDevice
                onClicked: Spotify.next()
            }
        }

        // The activity's default playlist first, then the first library playlists, then the picker.
        Row {
            id: pills

            anchors {
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            spacing: Theme.spacing4

            readonly property string activityId: DayController.currentActivityId
            readonly property string defaultUri: Settings.playlistUriFor(pills.activityId)
            readonly property string defaultName: Settings.playlistNameFor(pills.activityId)
            readonly property var entries: {
                const list = []
                if (pills.defaultUri.length > 0) {
                    list.push({ uri: pills.defaultUri, name: pills.defaultName })
                }
                for (const item of Spotify.playlists) {
                    if (list.length >= 4) {
                        break
                    }
                    if (item.uri !== pills.defaultUri) {
                        list.push({ uri: item.uri, name: item.name })
                    }
                }
                return list
            }

            Repeater {
                model: pills.entries

                NavPill {
                    id: pill

                    required property var modelData

                    text: pill.modelData.name
                    active: Spotify.contextUri === pill.modelData.uri
                    onClicked: Spotify.playPlaylist(pill.modelData.uri)
                }
            }

            NavPill {
                text: "+"
                onClicked: root.pickPlaylistRequested()
            }
        }
    }
}
