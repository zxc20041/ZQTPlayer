import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import ZQTPlayer 1.0

Page {
    id: root
    signal backHome()

    SettingsController {
        id: controller
    }

    StyleManager {
        id: styleManager
    }

    LanguageManager {
        id: languageManager
    }

    ThemeManager {
        id: themeManager
    }

    PlayerConfig {
        id: playerConfig
    }

    // Currently selected category index
    property int currentCategory: 0

    header: ToolBar {
        RowLayout {
            anchors.fill: parent

            ToolButton {
                text: "←"
                font.pointSize: 14
                onClicked: root.backHome()
            }

            Label {
                text: controller.title
                font.pointSize: 16
                font.bold: true
                Layout.fillWidth: true
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Left: Category list ──
        Pane {
            Layout.preferredWidth: 180
            Layout.fillHeight: true

            ListView {
                id: categoryList
                anchors.fill: parent
                model: [qsTr("General"), qsTr("Video Playback")]
                currentIndex: root.currentCategory
                delegate: ItemDelegate {
                    width: categoryList.width
                    text: modelData
                    highlighted: index === categoryList.currentIndex
                    onClicked: root.currentCategory = index
                }
            }
        }

        // ── Separator ──
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Qt.rgba(0, 0, 0, 0.12)
        }

        // ── Right: Settings content ──
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            Loader {
                width: parent.width
                sourceComponent: {
                    switch (root.currentCategory) {
                    case 0: return generalSettings;
                    case 1: return videoPlaybackSettings;
                    default: return generalSettings;
                    }
                }
            }
        }
    }

    // ── General Settings Component ──
    Component {
        id: generalSettings

        ColumnLayout {
            spacing: 24
            anchors.left: parent ? parent.left : undefined
            anchors.right: parent ? parent.right : undefined
            anchors.margins: 24
            anchors.topMargin: 24

            // ---------- Theme / Color ----------
            GroupBox {
                title: qsTr("Theme")
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    Label {
                        text: qsTr("Color mode")
                        font.bold: true
                    }

                    RowLayout {
                        spacing: 12
                        RadioButton {
                            text: qsTr("Light")
                            checked: themeManager.currentTheme === 0
                            onClicked: themeManager.setCurrentTheme(0)
                        }
                        RadioButton {
                            text: qsTr("Dark")
                            checked: themeManager.currentTheme === 1
                            onClicked: themeManager.setCurrentTheme(1)
                        }
                        RadioButton {
                            text: qsTr("Follow System")
                            checked: themeManager.currentTheme === 2
                            onClicked: themeManager.setCurrentTheme(2)
                        }
                    }
                }
            }

            // ---------- Style ----------
            GroupBox {
                title: qsTr("Style")
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        spacing: 12

                        Label {
                            text: qsTr("Application Style")
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ComboBox {
                            id: styleCombo
                            model: styleManager.availableStyles
                            currentIndex: styleManager.currentIndex
                            onActivated: function(index) {
                                styleManager.setCurrentIndex(index);
                            }
                        }
                    }

                    Label {
                        text: qsTr("Restart to apply")
                        font.italic: true
                        opacity: 0.6
                    }
                }
            }

            // ---------- Language ----------
            GroupBox {
                title: qsTr("Language")
                Layout.fillWidth: true

                RowLayout {
                    anchors.fill: parent
                    spacing: 12

                    Label {
                        text: qsTr("Display Language")
                        Layout.alignment: Qt.AlignVCenter
                    }

                    ComboBox {
                        id: langCombo
                        model: languageManager.availableLanguages
                        currentIndex: languageManager.currentIndex
                        onActivated: function(index) {
                            languageManager.setCurrentIndex(index);
                        }
                    }
                }
            }

            // Spacer
            Item {
                Layout.fillHeight: true
            }
        }
    }

    // ── Video Playback Settings Component ──
    Component {
        id: videoPlaybackSettings

        ColumnLayout {
            spacing: 24
            anchors.left: parent ? parent.left : undefined
            anchors.right: parent ? parent.right : undefined
            anchors.margins: 24
            anchors.topMargin: 24

            GroupBox {
                title: qsTr("Playback")
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    RowLayout {
                        spacing: 12

                        Label {
                            text: qsTr("Render Mode")
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ComboBox {
                            model: [qsTr("QVideoSink (CPU)"), qsTr("OpenGL Texture")]
                            currentIndex: playerConfig.renderMode
                            onActivated: function(index) {
                                playerConfig.renderMode = index;
                            }
                        }
                    }

                    RowLayout {
                        spacing: 12

                        Label {
                            text: qsTr("Scaling Filter")
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ComboBox {
                            model: [
                                qsTr("Point"),
                                qsTr("Fast Bilinear"),
                                qsTr("Bilinear"),
                                qsTr("Bicubic"),
                                qsTr("Lanczos")
                            ]
                            currentIndex: playerConfig.swsFilter
                            onActivated: function(index) {
                                playerConfig.swsFilter = index;
                            }
                        }
                    }

                    Switch {
                        text: qsTr("Real-time seek preview")
                        checked: playerConfig.realtimeSeekPreview
                        onToggled: playerConfig.realtimeSeekPreview = checked
                    }

                    RowLayout {
                        spacing: 12

                        Label {
                            text: qsTr("Decode Backend")
                            Layout.alignment: Qt.AlignVCenter
                        }

                        ComboBox {
                            model: [
                                qsTr("Software"),
                                qsTr("Auto Hardware"),
                                qsTr("D3D11VA"),
                                qsTr("VAAPI"),
                                qsTr("VideoToolbox")
                            ]
                            currentIndex: playerConfig.decodeBackend
                            onActivated: function(index) {
                                playerConfig.decodeBackend = index;
                            }
                        }
                    }

                    Switch {
                        text: qsTr("Allow hardware decode fallback")
                        checked: playerConfig.allowHwFallback
                        onToggled: playerConfig.allowHwFallback = checked
                    }

                    Switch {
                        text: qsTr("OpenGL Flip X")
                        checked: playerConfig.videoFlipX
                        onToggled: playerConfig.videoFlipX = checked
                    }

                    Switch {
                        text: qsTr("OpenGL Flip Y")
                        checked: playerConfig.videoFlipY
                        onToggled: playerConfig.videoFlipY = checked
                    }

                    Switch {
                        text: qsTr("OpenGL Lock Aspect Ratio")
                        checked: playerConfig.lockAspectRatio
                        onToggled: playerConfig.lockAspectRatio = checked
                    }
                }
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }
}
