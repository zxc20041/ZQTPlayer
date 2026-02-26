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
                model: [qsTr("General")]
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
}
