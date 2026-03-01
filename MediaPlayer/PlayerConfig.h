#pragma once

#include <QObject>
#include <QtQml/qqml.h>
#include "AVPlayerStatus.h"

/// @brief QML-visible singleton that holds all player configuration.
///        All settings are exposed as Q_PROPERTYs so QML can bind/read/write
///        them directly.
class PlayerConfig : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // ── Audio ──
    /// Volume in 0–100 (linear percentage).
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)

    /// Mute state (independent of volume value).
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY mutedChanged)

    // ── Video rendering ──
    Q_PROPERTY(int renderMode READ renderModeInt WRITE setRenderModeInt NOTIFY renderModeChanged)

    // ── Scaling filter ──
    Q_PROPERTY(int swsFilter READ swsFilterInt WRITE setSwsFilterInt NOTIFY swsFilterChanged)

public:
    explicit PlayerConfig(QObject *parent = nullptr);

    // ── Audio ──
    int  volume() const;
    void setVolume(int vol);

    bool isMuted() const;
    void setMuted(bool muted);

    /// Effective gain applied to PCM samples: 0.0 – 1.0.
    /// Returns 0.0 when muted, otherwise volume / 100.
    qreal effectiveVolume() const;

    // ── Video render mode ──
    VideoRenderMode renderMode() const;
    void setRenderMode(VideoRenderMode mode);

    // int wrappers for QML (enums not directly editable from JS)
    int  renderModeInt() const;
    void setRenderModeInt(int mode);

    // ── Sws filter ──
    SwsFilterMode swsFilter() const;
    void setSwsFilter(SwsFilterMode filter);

    int  swsFilterInt() const;
    void setSwsFilterInt(int filter);

signals:
    void volumeChanged();
    void mutedChanged();
    void renderModeChanged();
    void swsFilterChanged();

private:
    int              m_volume     = 80;
    bool             m_muted      = false;
    VideoRenderMode  m_renderMode = VideoRenderMode::QVideoSink;
    SwsFilterMode    m_swsFilter  = SwsFilterMode::Bilinear;
};
