import QtQuick
import QtQuick.Controls 2.15

Window {
    width: 1280
    height: 700
    minimumWidth: 360
    minimumHeight: 180
    visible: true
    title: qsTr("ZQTPlayer")

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: homePage
    }

    Component {
        id: homePage
        Home {
            onOpenSettings: stack.push(settingsPage)
        }
    }

    Component {
        id: settingsPage
        Settings {
            onBackHome: stack.pop()
        }
    }
}
