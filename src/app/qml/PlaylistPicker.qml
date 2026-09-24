pragma ComponentBehavior: Bound
import QtQuick
import Cadence
import Cadence.Theme

// Signal styled picker over the current screen: search across the loaded playlists, page in
// more, choose one. Escape closes it.
Rectangle {
    id: root

    property bool open: false
    property string title: qsTr("Choose a playlist")
    property string query: ""

    signal chosen(string uri, string name)

    anchors.fill: parent
    visible: root.open
    color: Qt.rgba(Theme.bg.r, Theme.bg.g, Theme.bg.b, 0.92)
    z: 10

    readonly property var filtered: {
        const needle = root.query.trim().toLowerCase()
        if (needle.length === 0) {
            return Spotify.playlists
        }
        return Spotify.playlists.filter(item => item.name.toLowerCase().indexOf(needle) >= 0)
    }

    function show(newTitle) {
        root.title = newTitle
        root.query = ""
        root.open = true
        if (Spotify.playlists.length === 0) {
            Spotify.loadPlaylists(true)
        }
        search.forceActiveFocus()
    }

    function close() {
        root.open = false
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.close()
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.open
        onActivated: root.close()
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spacing40 * 2, Theme.panelWidth + Theme.spacing40 * 4)
        height: Math.min(parent.height - Theme.spacing40 * 2, 640)
        radius: Theme.radius
        color: Theme.surface
        border.width: 1
        border.color: Theme.line

        MouseArea {
            anchors.fill: parent
        }

        Column {
            anchors {
                fill: parent
                margins: Theme.spacing24
            }
            spacing: Theme.spacing12

            Text {
                text: root.title
                color: Theme.text
                font.family: Theme.displayFamily
                font.weight: Theme.displayWeightExtraBold
                font.pixelSize: Theme.headingSizeMax
                font.capitalization: Font.AllUppercase
            }

            SignalField {
                id: search

                width: parent.width
                label: qsTr("Search")
                placeholder: qsTr("Type to filter")
                value: root.query
                onTextChanged: root.query = search.text
            }

            ListView {
                id: list

                width: parent.width
                height: parent.height - y - loadMore.height - Theme.spacing12
                clip: true
                model: root.filtered
                spacing: Theme.spacing4
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    id: row

                    required property var modelData

                    width: list.width
                    height: Theme.buttonHeightLarge
                    radius: Theme.radius
                    color: rowMouse.containsMouse ? Theme.surfaceMuted : "transparent"

                    Row {
                        anchors {
                            fill: parent
                            leftMargin: Theme.spacing8
                            rightMargin: Theme.spacing8
                        }
                        spacing: Theme.spacing12

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.touchTarget
                            height: Theme.touchTarget
                            radius: Theme.radius
                            color: Theme.surfaceMuted

                            Image {
                                anchors.fill: parent
                                source: row.modelData.imageUrl
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - Theme.touchTarget - Theme.spacing12

                            Text {
                                width: parent.width
                                text: row.modelData.name
                                color: Theme.text
                                elide: Text.ElideRight
                                font.family: Theme.bodyFamily
                                font.weight: Theme.bodyWeightSemiBold
                                font.pixelSize: Theme.bodySize
                            }

                            Text {
                                width: parent.width
                                text: row.modelData.owner
                                color: Theme.textMuted
                                elide: Text.ElideRight
                                font.family: Theme.bodyFamily
                                font.pixelSize: Theme.labelSize
                            }
                        }
                    }

                    MouseArea {
                        id: rowMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.chosen(row.modelData.uri, row.modelData.name)
                            root.close()
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    text: Spotify.playlistsLoading ? qsTr("Loading") : Spotify.connected ? qsTr("No playlists") : qsTr("Not connected")
                    color: Theme.textMuted
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.bodySize
                }
            }

            Row {
                id: loadMore

                spacing: Theme.spacing12

                SignalButton {
                    text: Spotify.playlistsLoading ? qsTr("Loading") : qsTr("Load more")
                    enabled: Spotify.playlistsHasMore && !Spotify.playlistsLoading
                    onClicked: Spotify.loadPlaylists(false)
                }

                SignalButton {
                    kind: "outline"
                    text: qsTr("Close · Esc")
                    onClicked: root.close()
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("%n loaded", "", Spotify.playlists.length)
                    color: Theme.textMuted
                    font.family: Theme.bodyFamily
                    font.pixelSize: Theme.labelSize
                }
            }
        }
    }
}
