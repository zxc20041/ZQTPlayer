#pragma once

#include <QObject>
#include <QString>
#include <QSize>
#include <QtQml/qqml.h>
#include <memory>
#include "AVCodecHandler.h"

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

public:
    explicit PlayerWindowManager(QObject *parent = nullptr);

    // ── Drop enabled ──
    bool dropEnabled() const;
    void setDropEnabled(bool enabled);

    // ── Open / close media ──
    /// Called from QML when a file is dropped or chosen.
    /// Returns true if the file was successfully opened.
    Q_INVOKABLE bool openMedia(const QString &path);

    /// Close current media and clear metadata.
    Q_INVOKABLE void closeMedia();

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

private:
    bool m_dropEnabled = true;
    AVCodecHandler m_codec;
};
