#include "PlayerConfig.h"
#include <algorithm>

PlayerConfig::PlayerConfig(QObject *parent)
    : QObject(parent)
{
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
