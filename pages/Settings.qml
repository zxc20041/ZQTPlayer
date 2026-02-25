import QtQuick
import QtQuick.Controls 2.15
import ZQTPlayer 1.0

Page {
    id: root
    signal backHome()

    SettingsController {
        id: controller
    }

    Column {
        anchors.centerIn: parent
        spacing: 12

        Text {
            text: controller.title
            font.pointSize: 18
        }

        Button {
            text: qsTr("Back")
            onClicked: root.backHome()
        }
    }
}
