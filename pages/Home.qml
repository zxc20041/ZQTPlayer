import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import ZQTPlayer 1.0

ApplicationWindow {
    id: root
    width: 1280
    height: 700
    minimumWidth: 360
    minimumHeight: 180
    visible: true
    title: qsTr("ZQTPlayer")

    HomeController {
        id: controller
    }

    PlayerWindowManager {
        id: playerManager
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: homeContent
    }

    Component {
        id: homeContent
        Page {
            // ── Drop area covers the whole page ──
            DropArea {
                anchors.fill: parent
                enabled: playerManager.dropEnabled
                keys: ["text/uri-list"]

                onDropped: function(drop) {
                    dropOverlay.visible = false;
                    if (drop.hasUrls && drop.urls.length > 0) {
                        let path = drop.urls[0].toString();
                        if (playerManager.openMedia(path)) {
                            mediaInfoDialog.open();
                        }
                    }
                }

                onEntered: function(drag) {
                    dropOverlay.visible = true;
                }

                onExited: {
                    dropOverlay.visible = false;
                }

                // ── Normal content ──
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12

                    Label {
                        text: controller.title
                        font.pointSize: 18
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: qsTr("Drag a media file here to open")
                        opacity: 0.5
                        Layout.alignment: Qt.AlignHCenter
                    }

                    RowLayout {
                        spacing: 12
                        Layout.alignment: Qt.AlignHCenter

                        Button {
                            text: qsTr("Settings")
                            onClicked: stack.push(settingsPage)
                        }

                        Button {
                            text: qsTr("Media Info")
                            enabled: playerManager.hasMedia
                            onClicked: mediaInfoDialog.open()
                        }
                    }
                }

                // ── Drop highlight overlay ──
                Rectangle {
                    id: dropOverlay
                    anchors.fill: parent
                    visible: false
                    color: Qt.rgba(0, 0.5, 1, 0.15)
                    border.color: Qt.rgba(0, 0.5, 1, 0.6)
                    border.width: 3
                    radius: 8

                    Label {
                        anchors.centerIn: parent
                        text: qsTr("Drop to open")
                        font.pointSize: 16
                        opacity: 0.8
                    }
                }
            }

            // ── Media info dialog ──
            MediaInfoDialog {
                id: mediaInfoDialog
                manager: playerManager
            }
        }
    }

    Component {
        id: settingsPage
        Settings {
            onBackHome: stack.pop()
        }
    }
}
