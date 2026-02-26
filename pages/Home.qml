import QtQuick
import QtQuick.Controls 2.15
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

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: homeContent
    }

    Component {
        id: homeContent
        Page {
            Column {
                anchors.centerIn: parent
                spacing: 12

                Label {
                    text: controller.title
                    font.pointSize: 18
                }

                Button {
                    text: qsTr("Settings")
                    onClicked: stack.push(settingsPage)
                }
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
