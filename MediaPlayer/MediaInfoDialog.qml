import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import ZQTPlayer 1.0

/// Sub-window (dialog) that shows media file metadata.
/// Usage from Home.qml:  mediaInfoDialog.open()
Dialog {
    id: root
    title: qsTr("Media Info")
    modal: false
    width: 400
    height: 380
    anchors.centerIn: parent

    required property PlayerWindowManager manager

    standardButtons: Dialog.Close

    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth

        GridLayout {
            columns: 2
            columnSpacing: 16
            rowSpacing: 8
            width: parent.width

            // ── File ──
            Label { text: qsTr("File"); font.bold: true; Layout.columnSpan: 2 }

            Label { text: qsTr("Path") }
            Label {
                text: root.manager.hasMedia ? root.manager.filePath : "-"
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }

            // ── Video ──
            Label { text: qsTr("Video"); font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 8 }

            Label { text: qsTr("Resolution") }
            Label { text: root.manager.hasMedia ? root.manager.resolutionText : "-" }

            Label { text: qsTr("Duration") }
            Label { text: root.manager.hasMedia ? root.manager.durationText : "-" }

            Label { text: qsTr("Codec") }
            Label { text: root.manager.hasMedia ? root.manager.videoCodec : "-" }

            Label { text: qsTr("Frame Rate") }
            Label { text: root.manager.hasMedia ? root.manager.frameRate.toFixed(2) + " fps" : "-" }

            Label { text: qsTr("Bit Rate") }
            Label { text: root.manager.hasMedia ? root.manager.bitRate.toFixed(0) + " kbps" : "-" }

            // ── Audio ──
            Label { text: qsTr("Audio"); font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 8 }

            Label { text: qsTr("Codec") }
            Label { text: root.manager.hasMedia ? root.manager.audioCodec : "-" }

            Label { text: qsTr("Sample Rate") }
            Label { text: root.manager.hasMedia ? root.manager.sampleRate + " Hz" : "-" }

            Label { text: qsTr("Channels") }
            Label { text: root.manager.hasMedia ? root.manager.audioChannels.toString() : "-" }
        }
    }
}
