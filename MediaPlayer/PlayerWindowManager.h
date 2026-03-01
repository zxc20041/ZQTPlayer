#pragma once

#include <QObject>
#include <QString>
#include <QSize>
#include <QTimer>
#include <QThread>
#include <QtQml/qqml.h>
#include <QVideoSink>
#include <memory>
#include "AVCodecHandler.h"
#include "PlayerConfig.h"

class FrameHandler;

/// @brief QML-visible controller that handles drag-drop file import
///        and exposes media metadata for the info panel.
class PlayerWindowManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // ── Drag-drop state ──
    Q_PROPERTY(bool dropEnabled READ dropEnabled WRITE setDropEnabled NOTIFY dropEnabledChanged)

    // ── Media state ──
    Q_PROPERTY(bool hasMedia   READ hasMedia   NOTIFY mediaChanged)
    Q_PROPERTY(QString filePath READ filePath   NOTIFY mediaChanged)

    // ── Media metadata (valid when hasMedia == true) ──
    Q_PROPERTY(int    videoWidth     READ videoWidth     NOTIFY mediaChanged)
    Q_PROPERTY(int    videoHeight    READ videoHeight    NOTIFY mediaChanged)
    Q_PROPERTY(double duration       READ duration       NOTIFY mediaChanged)
    Q_PROPERTY(double bitRate        READ bitRate        NOTIFY mediaChanged)
    Q_PROPERTY(QString videoCodec    READ videoCodec     NOTIFY mediaChanged)
    Q_PROPERTY(QString audioCodec    READ audioCodec     NOTIFY mediaChanged)
    Q_PROPERTY(double frameRate      READ frameRate      NOTIFY mediaChanged)
    Q_PROPERTY(int    sampleRate     READ sampleRate     NOTIFY mediaChanged)
    Q_PROPERTY(int    audioChannels  READ audioChannels  NOTIFY mediaChanged)
    Q_PROPERTY(QString durationText  READ durationText   NOTIFY mediaChanged)
    Q_PROPERTY(QString resolutionText READ resolutionText NOTIFY mediaChanged)

    // ── Playback ──
    Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(double position     READ position     NOTIFY positionChanged)
    Q_PROPERTY(QString positionText READ positionText NOTIFY positionChanged)

    // ── Config ──
    Q_PROPERTY(PlayerConfig* config READ config CONSTANT)

public:
    explicit PlayerWindowManager(QObject *parent = nullptr);
    ~PlayerWindowManager() override;

    // ── Drop enabled ──
    bool dropEnabled() const;
    void setDropEnabled(bool enabled);

    // ── Open / close media ──
    /// Called from QML when a file is dropped or chosen.
    /// Opens asynchronously; emits mediaChanged() + auto-plays on success.
    Q_INVOKABLE void openMedia(const QString &path);

    /// Close current media and clear metadata.
    Q_INVOKABLE void closeMedia();

    // ── Playback control ──
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePlayPause();

    // ── Video sink ──
    QVideoSink *videoSink() const;
    void setVideoSink(QVideoSink *sink);

    // ── Playing state ──
    bool isPlaying() const;

    // ── Position ──
    double  position() const;
    QString positionText() const;

    // ── Config ──
    PlayerConfig *config() const;

    // ── Property getters ──
    bool    hasMedia() const;
    QString filePath() const;
    int     videoWidth() const;
    int     videoHeight() const;
    double  duration() const;
    double  bitRate() const;
    QString videoCodec() const;
    QString audioCodec() const;
    double  frameRate() const;
    int     sampleRate() const;
    int     audioChannels() const;

    /// "HH:MM:SS" formatted duration
    QString durationText() const;

    /// "1920×1080" formatted resolution
    QString resolutionText() const;

signals:
    void dropEnabledChanged();
    void mediaChanged();
    void mediaOpenFailed(const QString &path);
    void videoSinkChanged();
    void playingChanged();
    void positionChanged();
    void playbackFinished();

private slots:
    void onPositionTimer();

private:
    bool m_dropEnabled = true;
    AVCodecHandler m_codec;
    FrameHandler  *m_frameHandler = nullptr;   // owned, child QObject
    QVideoSink    *m_videoSink    = nullptr;   // non-owning, from QML VideoOutput
    QTimer         m_positionTimer;
    double         m_position     = 0.0;
    PlayerConfig  *m_config       = nullptr;   // owned, child QObject

    /// Async helper: runs AVCodecHandler::open() off the main thread
    void openMediaAsync(const QString &localPath);
};
