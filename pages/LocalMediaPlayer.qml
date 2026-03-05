import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import QtMultimedia
import ZQTPlayer 1.0

Page {
    id: root

    signal backHome()

    /// File path received from Home page via StackView push
    property string mediaSource: ""

    function formatTime(seconds) {
        const total = Math.max(0, Math.floor(seconds));
        const h = Math.floor(total / 3600);
        const m = Math.floor((total % 3600) / 60);
        const s = total % 60;

        function pad2(v) { return v < 10 ? "0" + v : "" + v; }
        return pad2(h) + ":" + pad2(m) + ":" + pad2(s);
    }

    PlayerWindowManager {
        id: playerManager
        videoSink: videoOutput.videoSink
        glFrameSink: openGLVideoItem
    }

    // ── Video output ──
    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: playerManager.config.renderMode === 0
    }

    OpenGLVideoItem {
        id: openGLVideoItem
        anchors.fill: parent
        visible: playerManager.config.renderMode === 1
        flipX: playerManager.config.videoFlipX
        flipY: playerManager.config.videoFlipY
        preserveAspectRatio: playerManager.config.lockAspectRatio
    }

    // ── Drop area for opening files directly ──
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]

        onDropped: function(drop) {
            dropOverlay.visible = false;
            if (drop.hasUrls && drop.urls.length > 0) {
                let path = drop.urls[0].toString();
                root.mediaSource = path;
                playerManager.openMedia(path);
            }
        }

        onEntered: function(drag) {
            dropOverlay.visible = true;
        }

        onExited: {
            dropOverlay.visible = false;
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
        z: 100

        Label {
            anchors.centerIn: parent
            text: qsTr("Drop to open")
            font.pointSize: 16
            opacity: 0.8
        }
    }

    // ── Top bar ──
    header: ToolBar {
        background: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.6)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8

            ToolButton {
                text: "←"
                font.pointSize: 14
                onClicked: root.backHome()
            }

            Label {
                text: playerManager.hasMedia ? playerManager.filePath : qsTr("No media")
                font.pointSize: 12
                color: "white"
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }

            ToolButton {
                text: "ℹ"
                font.pointSize: 14
                enabled: playerManager.hasMedia
                onClicked: mediaInfoDialog.open()
            }
        }
    }

    // ── Bottom control bar ──
    footer: ToolBar {
        background: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.6)
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 4

            // ── Progress slider ──
            Slider {
                id: progressSlider
                Layout.fillWidth: true
                property real seekPreview: 0
                property bool pendingSeek: false
                property real pendingTarget: 0
                property real lastPreviewSeek: -1
                property bool resumeAfterRelease: false
                from: 0
                to: playerManager.hasMedia ? playerManager.duration : 1
                value: 0
                enabled: playerManager.hasMedia

                function requestSeek(target, lockUi) {
                    if (!playerManager.hasMedia) return;
                    if (lockUi) {
                        pendingSeek = true;
                        pendingTarget = target;
                        settleTimer.restart();
                    }
                    playerManager.seek(target);
                }

                Binding {
                    target: progressSlider
                    property: "value"
                    value: playerManager.position
                    when: !progressSlider.pressed && !progressSlider.pendingSeek
                }

                onPressedChanged: {
                    if (pressed) {
                        if (playerManager.config.realtimeSeekPreview) {
                            resumeAfterRelease = playerManager.playing;
                            if (resumeAfterRelease)
                                playerManager.pause();
                        } else {
                            resumeAfterRelease = false;
                        }

                        pendingSeek = false;
                        settleTimer.stop();
                        seekPreview = value;
                        lastPreviewSeek = -1;
                        throttleTimer.stop();
                    } else {
                        throttleTimer.stop();
                        const target = seekPreview;
                        Qt.callLater(function() {
                            requestSeek(target, true);
                            if (progressSlider.resumeAfterRelease) {
                                playerManager.play();
                                progressSlider.resumeAfterRelease = false;
                            }
                        });
                    }
                }

                onValueChanged: {
                    if (pressed) {
                        seekPreview = value;
                    }
                }

                onMoved: {
                    seekPreview = value;

                    if (!playerManager.config.realtimeSeekPreview) return;

                    if (!throttleTimer.running) {
                        throttleTimer.start();
                    } else {
                        throttleTimer.restart();
                    }
                }

                Timer {
                    id: throttleTimer
                    interval: 120
                    repeat: false
                    onTriggered: {
                        if (!progressSlider.pressed) return;

                        const target = progressSlider.seekPreview;
                        if (Math.abs(target - progressSlider.lastPreviewSeek) < 0.2)
                            return;

                        progressSlider.lastPreviewSeek = target;
                        playerManager.seek(target);
                    }
                }

                Timer {
                    id: settleTimer
                    interval: 900
                    repeat: false
                    onTriggered: {
                        progressSlider.pendingSeek = false;
                    }
                }
            }

            Connections {
                target: playerManager
                function onPositionChanged() {
                    if (!progressSlider.pendingSeek)
                        return;

                    if (Math.abs(playerManager.position - progressSlider.pendingTarget) <= 1.0) {
                        progressSlider.pendingSeek = false;
                        settleTimer.stop();
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                // ── Time labels ──
                Label {
                    id: currentTimeLabel
                    text: progressSlider.pressed
                          ? root.formatTime(progressSlider.seekPreview)
                          : (progressSlider.pendingSeek
                             ? root.formatTime(progressSlider.pendingTarget)
                             : playerManager.positionText)
                    color: "white"
                    font.pointSize: 10
                    font.family: "Consolas"
                }

                Item { Layout.fillWidth: true }

                // ── Playback controls ──
                RowLayout {
                    spacing: 16
                    Layout.alignment: Qt.AlignHCenter

                    RoundButton {
                        text: "⏮"
                        font.pointSize: 14
                        onClicked: {
                            // TODO: previous / rewind
                        }
                    }

                    RoundButton {
                        id: playPauseBtn
                        text: playerManager.playing ? "⏸" : "▶"
                        font.pointSize: 18
                        implicitWidth: 56
                        implicitHeight: 56
                        enabled: playerManager.hasMedia
                        onClicked: {
                            playerManager.togglePlayPause();
                        }
                    }

                    RoundButton {
                        text: "⏭"
                        font.pointSize: 14
                        onClicked: {
                            // TODO: next / fast-forward
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // ── Volume control ──
                RowLayout {
                    spacing: 4

                    Label {
                        text: playerManager.config.muted ? "🔇" : "🔊"
                        font.pointSize: 12

                        MouseArea {
                            anchors.fill: parent
                            onClicked: playerManager.config.muted = !playerManager.config.muted
                        }
                    }

                    Slider {
                        id: volumeSlider
                        from: 0
                        to: 100
                        value: playerManager.config.volume
                        implicitWidth: 100

                        onMoved: {
                            playerManager.config.volume = Math.round(value);
                        }
                    }
                }

                Label {
                    text: playerManager.hasMedia ? playerManager.durationText : "00:00:00"
                    color: "white"
                    font.pointSize: 10
                    font.family: "Consolas"
                }
            }
        }
    }

    // ── Media info dialog ──
    MediaInfoDialog {
        id: mediaInfoDialog
        manager: playerManager
    }

    // ── Auto-open media when pushed with a path ──
    Component.onCompleted: {
        if (mediaSource !== "") {
            playerManager.openMedia(mediaSource);
        }
    }
}
