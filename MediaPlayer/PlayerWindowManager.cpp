#include "PlayerWindowManager.h"
#include <QFileInfo>
#include <QDebug>

PlayerWindowManager::PlayerWindowManager(QObject *parent)
    : QObject(parent)
{
}

// ── Drop enabled ───────────────────────────────────────────

bool PlayerWindowManager::dropEnabled() const
{
    return m_dropEnabled;
}

void PlayerWindowManager::setDropEnabled(bool enabled)
{
    if (m_dropEnabled == enabled) return;
    m_dropEnabled = enabled;
    emit dropEnabledChanged();
}

// ── Open / close ───────────────────────────────────────────

bool PlayerWindowManager::openMedia(const QString &path)
{
    // Normalise: QML's drop gives "file:///C:/..." – strip scheme
    QString localPath = path;
    if (localPath.startsWith("file:///")) {
        localPath = localPath.mid(8); // "C:/..."
    }

    QFileInfo fi(localPath);
    if (!fi.exists() || !fi.isFile()) {
        qWarning() << "File does not exist:" << localPath;
        emit mediaOpenFailed(localPath);
        return false;
    }

    m_codec.setFilePath(localPath);
    if (!m_codec.open()) {
        qWarning() << "Failed to open media:" << localPath;
        emit mediaOpenFailed(localPath);
        return false;
    }

    qDebug() << "Opened media:" << localPath
             << "resolution:" << m_codec.videoResolution()
             << "duration:" << m_codec.durationSeconds() << "s"
             << "video:" << m_codec.videoCodecName()
             << "audio:" << m_codec.audioCodecName();

    emit mediaChanged();
    return true;
}

void PlayerWindowManager::closeMedia()
{
    m_codec.close();
    emit mediaChanged();
}

// ── Property getters ───────────────────────────────────────

bool PlayerWindowManager::hasMedia() const
{
    return m_codec.isOpen();
}

QString PlayerWindowManager::filePath() const
{
    return m_codec.filePath();
}

int PlayerWindowManager::videoWidth() const
{
    return m_codec.videoResolution().width();
}

int PlayerWindowManager::videoHeight() const
{
    return m_codec.videoResolution().height();
}

double PlayerWindowManager::duration() const
{
    return m_codec.durationSeconds();
}

double PlayerWindowManager::bitRate() const
{
    return m_codec.bitRateKbps();
}

QString PlayerWindowManager::videoCodec() const
{
    return m_codec.videoCodecName();
}

QString PlayerWindowManager::audioCodec() const
{
    return m_codec.audioCodecName();
}

double PlayerWindowManager::frameRate() const
{
    return m_codec.videoFrameRate();
}

int PlayerWindowManager::sampleRate() const
{
    return m_codec.audioSampleRate();
}

int PlayerWindowManager::audioChannels() const
{
    return m_codec.audioChannels();
}

QString PlayerWindowManager::durationText() const
{
    double secs = m_codec.durationSeconds();
    int h = static_cast<int>(secs) / 3600;
    int m = (static_cast<int>(secs) % 3600) / 60;
    int s = static_cast<int>(secs) % 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

QString PlayerWindowManager::resolutionText() const
{
    QSize res = m_codec.videoResolution();
    if (res.isEmpty()) return "-";
    return QString("%1×%2").arg(res.width()).arg(res.height());
}
