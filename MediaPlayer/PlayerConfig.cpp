#include "PlayerConfig.h"
#include <algorithm>
#include <QSettings>

PlayerConfig::PlayerConfig(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_volume = settings.value("player/volume", m_volume).toInt();
    m_volume = std::clamp(m_volume, 0, 100);
    m_muted = settings.value("player/muted", m_muted).toBool();
    m_renderMode = static_cast<VideoRenderMode>(
        settings.value("player/renderMode", static_cast<int>(m_renderMode)).toInt());
    m_swsFilter = static_cast<SwsFilterMode>(
        settings.value("player/swsFilter", static_cast<int>(m_swsFilter)).toInt());
    m_realtimeSeekPreview = settings.value("player/realtimeSeekPreview", m_realtimeSeekPreview).toBool();
    m_decodeBackend = static_cast<VideoDecodeBackend>(
        settings.value("player/decodeBackend", static_cast<int>(m_decodeBackend)).toInt());
    m_preferZeroCopy = settings.value("player/preferZeroCopy", m_preferZeroCopy).toBool();
    m_allowHwFallback = settings.value("player/allowHwFallback", m_allowHwFallback).toBool();
    m_videoFlipX = settings.value("player/videoFlipX", m_videoFlipX).toBool();
    m_videoFlipY = settings.value("player/videoFlipY", m_videoFlipY).toBool();
    m_lockAspectRatio = settings.value("player/lockAspectRatio", m_lockAspectRatio).toBool();
}

// ── Audio ──────────────────────────────────────────────────

int PlayerConfig::volume() const
{
    return m_volume;
}

void PlayerConfig::setVolume(int vol)
{
    vol = std::clamp(vol, 0, 100);
    if (m_volume == vol) return;
    m_volume = vol;
    QSettings().setValue("player/volume", m_volume);
    emit volumeChanged();
}

bool PlayerConfig::isMuted() const
{
    return m_muted;
}

void PlayerConfig::setMuted(bool muted)
{
    if (m_muted == muted) return;
    m_muted = muted;
    QSettings().setValue("player/muted", m_muted);
    emit mutedChanged();
}

qreal PlayerConfig::effectiveVolume() const
{
    if (m_muted) return 0.0;
    return static_cast<qreal>(m_volume) / 100.0;
}

// ── Video render mode ──────────────────────────────────────

VideoRenderMode PlayerConfig::renderMode() const
{
    return m_renderMode;
}

void PlayerConfig::setRenderMode(VideoRenderMode mode)
{
    if (m_renderMode == mode) return;
    m_renderMode = mode;
    QSettings().setValue("player/renderMode", static_cast<int>(m_renderMode));
    emit renderModeChanged();
}

int PlayerConfig::renderModeInt() const
{
    return static_cast<int>(m_renderMode);
}

void PlayerConfig::setRenderModeInt(int mode)
{
    setRenderMode(static_cast<VideoRenderMode>(mode));
}

// ── Sws filter ─────────────────────────────────────────────

SwsFilterMode PlayerConfig::swsFilter() const
{
    return m_swsFilter;
}

void PlayerConfig::setSwsFilter(SwsFilterMode filter)
{
    if (m_swsFilter == filter) return;
    m_swsFilter = filter;
    QSettings().setValue("player/swsFilter", static_cast<int>(m_swsFilter));
    emit swsFilterChanged();
}

int PlayerConfig::swsFilterInt() const
{
    return static_cast<int>(m_swsFilter);
}

void PlayerConfig::setSwsFilterInt(int filter)
{
    setSwsFilter(static_cast<SwsFilterMode>(filter));
}

bool PlayerConfig::realtimeSeekPreview() const
{
    return m_realtimeSeekPreview;
}

void PlayerConfig::setRealtimeSeekPreview(bool enabled)
{
    if (m_realtimeSeekPreview == enabled) return;
    m_realtimeSeekPreview = enabled;
    QSettings().setValue("player/realtimeSeekPreview", m_realtimeSeekPreview);
    emit realtimeSeekPreviewChanged();
}

VideoDecodeBackend PlayerConfig::decodeBackend() const
{
    return m_decodeBackend;
}

void PlayerConfig::setDecodeBackend(VideoDecodeBackend backend)
{
    if (m_decodeBackend == backend) return;
    m_decodeBackend = backend;
    QSettings().setValue("player/decodeBackend", static_cast<int>(m_decodeBackend));
    emit decodeBackendChanged();
}

int PlayerConfig::decodeBackendInt() const
{
    return static_cast<int>(m_decodeBackend);
}

void PlayerConfig::setDecodeBackendInt(int backend)
{
    setDecodeBackend(static_cast<VideoDecodeBackend>(backend));
}

bool PlayerConfig::preferZeroCopy() const
{
    return m_preferZeroCopy;
}

void PlayerConfig::setPreferZeroCopy(bool enabled)
{
    if (m_preferZeroCopy == enabled) return;
    m_preferZeroCopy = enabled;
    QSettings().setValue("player/preferZeroCopy", m_preferZeroCopy);
    emit preferZeroCopyChanged();
}

bool PlayerConfig::allowHwFallback() const
{
    return m_allowHwFallback;
}

void PlayerConfig::setAllowHwFallback(bool enabled)
{
    if (m_allowHwFallback == enabled) return;
    m_allowHwFallback = enabled;
    QSettings().setValue("player/allowHwFallback", m_allowHwFallback);
    emit allowHwFallbackChanged();
}

bool PlayerConfig::videoFlipX() const
{
    return m_videoFlipX;
}

void PlayerConfig::setVideoFlipX(bool enabled)
{
    if (m_videoFlipX == enabled) return;
    m_videoFlipX = enabled;
    QSettings().setValue("player/videoFlipX", m_videoFlipX);
    emit videoFlipXChanged();
}

bool PlayerConfig::videoFlipY() const
{
    return m_videoFlipY;
}

void PlayerConfig::setVideoFlipY(bool enabled)
{
    if (m_videoFlipY == enabled) return;
    m_videoFlipY = enabled;
    QSettings().setValue("player/videoFlipY", m_videoFlipY);
    emit videoFlipYChanged();
}

bool PlayerConfig::lockAspectRatio() const
{
    return m_lockAspectRatio;
}

void PlayerConfig::setLockAspectRatio(bool enabled)
{
    if (m_lockAspectRatio == enabled) return;
    m_lockAspectRatio = enabled;
    QSettings().setValue("player/lockAspectRatio", m_lockAspectRatio);
    emit lockAspectRatioChanged();
}
