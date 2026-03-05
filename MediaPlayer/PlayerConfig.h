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

    // ── Seek preview ──
    /// Enable throttled real-time seek while dragging the progress slider.
    Q_PROPERTY(bool realtimeSeekPreview READ realtimeSeekPreview WRITE setRealtimeSeekPreview NOTIFY realtimeSeekPreviewChanged)

    // ── Decode backend (future HW decode) ──
    Q_PROPERTY(int decodeBackend READ decodeBackendInt WRITE setDecodeBackendInt NOTIFY decodeBackendChanged)

    // ── Hardware decode options (future use) ──
    Q_PROPERTY(bool preferZeroCopy READ preferZeroCopy WRITE setPreferZeroCopy NOTIFY preferZeroCopyChanged)
    Q_PROPERTY(bool allowHwFallback READ allowHwFallback WRITE setAllowHwFallback NOTIFY allowHwFallbackChanged)

    // ── OpenGL presentation options ──
    Q_PROPERTY(bool videoFlipX READ videoFlipX WRITE setVideoFlipX NOTIFY videoFlipXChanged)
    Q_PROPERTY(bool videoFlipY READ videoFlipY WRITE setVideoFlipY NOTIFY videoFlipYChanged)
    Q_PROPERTY(bool lockAspectRatio READ lockAspectRatio WRITE setLockAspectRatio NOTIFY lockAspectRatioChanged)

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

    // ── Seek preview ──
    bool realtimeSeekPreview() const;
    void setRealtimeSeekPreview(bool enabled);

    // ── Decode backend ──
    VideoDecodeBackend decodeBackend() const;
    void setDecodeBackend(VideoDecodeBackend backend);
    int  decodeBackendInt() const;
    void setDecodeBackendInt(int backend);

    // ── Hardware decode options ──
    bool preferZeroCopy() const;
    void setPreferZeroCopy(bool enabled);

    bool allowHwFallback() const;
    void setAllowHwFallback(bool enabled);

    bool videoFlipX() const;
    void setVideoFlipX(bool enabled);

    bool videoFlipY() const;
    void setVideoFlipY(bool enabled);

    bool lockAspectRatio() const;
    void setLockAspectRatio(bool enabled);

signals:
    void volumeChanged();
    void mutedChanged();
    void renderModeChanged();
    void swsFilterChanged();
    void realtimeSeekPreviewChanged();
    void decodeBackendChanged();
    void preferZeroCopyChanged();
    void allowHwFallbackChanged();
    void videoFlipXChanged();
    void videoFlipYChanged();
    void lockAspectRatioChanged();

private:
    int              m_volume     = 80;
    bool             m_muted      = false;
    VideoRenderMode  m_renderMode = VideoRenderMode::QVideoSink;
    SwsFilterMode    m_swsFilter  = SwsFilterMode::Bilinear;
    bool             m_realtimeSeekPreview = true;
    VideoDecodeBackend m_decodeBackend = VideoDecodeBackend::Software;
    bool             m_preferZeroCopy = true;
    bool             m_allowHwFallback = true;
    bool             m_videoFlipX = false;
    bool             m_videoFlipY = false;
    bool             m_lockAspectRatio = true;
};
