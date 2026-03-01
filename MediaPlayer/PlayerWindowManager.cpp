#include "PlayerWindowManager.h"
#include "FrameHandler.h"
#include <QFileInfo>
#include <QUrl>
#include <QDebug>
#include <QVideoSink>

PlayerWindowManager::PlayerWindowManager(QObject *parent)
    : QObject(parent)
    , m_frameHandler(new FrameHandler(this))   // owned as child QObject
    , m_config(new PlayerConfig(this))         // owned as child QObject
{
    m_codec.setFrameHandler(m_frameHandler);

    // Position timer: fires every 200 ms while playing to update UI
    m_positionTimer.setInterval(200);
    connect(&m_positionTimer, &QTimer::timeout, this, &PlayerWindowManager::onPositionTimer);

    // Forward volume changes to the audio sink in real-time
    connect(m_config, &PlayerConfig::volumeChanged, this, [this]() {
        m_frameHandler->setVolume(m_config->effectiveVolume());
    });
    connect(m_config, &PlayerConfig::mutedChanged, this, [this]() {
        m_frameHandler->setVolume(m_config->effectiveVolume());
    });
}

PlayerWindowManager::~PlayerWindowManager()
{
    stop();
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

void PlayerWindowManager::openMedia(const QString &path)
{
    // Normalise: QML's drop gives "file:///..." – use QUrl for cross-platform conversion
    QString localPath = QUrl(path).toLocalFile();
    if (localPath.isEmpty()) {
        localPath = path;  // fallback: already a plain path
    }

    QFileInfo fi(localPath);
    if (!fi.exists() || !fi.isFile()) {
        qWarning() << "File does not exist:" << localPath;
        emit mediaOpenFailed(localPath);
        return;
    }

    // Stop any previous playback
    stop();

    // Run the heavy FFmpeg open on a background thread so the UI
    // (page transition animation, etc.) stays responsive.
    openMediaAsync(localPath);
}

void PlayerWindowManager::openMediaAsync(const QString &localPath)
{
    // Capture path by value; the lambda runs on a throwaway QThread.
    QThread *worker = QThread::create([this, localPath]() {
        m_codec.setFilePath(localPath);
        bool ok = m_codec.open();

        // Bounce back to the main / GUI thread for UI updates + play()
        QMetaObject::invokeMethod(this, [this, ok, localPath]() {
            if (!ok) {
                qWarning() << "Failed to open media:" << localPath;
                emit mediaOpenFailed(localPath);
                return;
            }

            qDebug() << "Opened media:" << localPath
                     << "resolution:" << m_codec.videoResolution()
                     << "duration:" << m_codec.durationSeconds() << "s"
                     << "video:" << m_codec.videoCodecName()
                     << "audio:" << m_codec.audioCodecName();

            emit mediaChanged();

            // Auto-play after successful open
            play();
        }, Qt::QueuedConnection);
    });

    // Auto-delete the QThread when it finishes
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void PlayerWindowManager::closeMedia()
{
    stop();
    m_codec.close();
    emit mediaChanged();
}

// ── Playback control ─────────────────────────────────────────────────

void PlayerWindowManager::play()
{
    if (!m_codec.isOpen()) return;

    // Ensure the video sink is wired before starting threads
    if (m_videoSink)
        m_frameHandler->setVideoSink(m_videoSink);

    // Apply current volume setting
    m_frameHandler->setVolume(m_config->effectiveVolume());

    m_codec.play();
    m_positionTimer.start();
    emit playingChanged();
}

void PlayerWindowManager::pause()
{
    m_codec.pause();
    m_positionTimer.stop();
    emit playingChanged();
}

void PlayerWindowManager::stop()
{
    m_codec.stop();
    m_positionTimer.stop();
    m_position = 0.0;
    emit positionChanged();
    emit playingChanged();
}

void PlayerWindowManager::togglePlayPause()
{
    if (m_codec.status() == AVPlayerStatus::Playing) {
        pause();
    } else {
        play();
    }
}

// ── Video sink ───────────────────────────────────────────────────

QVideoSink *PlayerWindowManager::videoSink() const
{
    return m_videoSink;
}

void PlayerWindowManager::setVideoSink(QVideoSink *sink)
{
    if (m_videoSink == sink) return;
    m_videoSink = sink;
    m_frameHandler->setVideoSink(sink);
    emit videoSinkChanged();
}

// ── Playing state ─────────────────────────────────────────────────

bool PlayerWindowManager::isPlaying() const
{
    return m_codec.status() == AVPlayerStatus::Playing;
}

// ── Position ─────────────────────────────────────────────────────

double PlayerWindowManager::position() const
{
    return m_position;
}

QString PlayerWindowManager::positionText() const
{
    int total = static_cast<int>(m_position);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

// ── Position timer ───────────────────────────────────────────────

void PlayerWindowManager::onPositionTimer()
{
    // Update position from the audio clock
    double pos = m_frameHandler->audioClock();
    if (pos != m_position) {
        m_position = pos;
        emit positionChanged();
    }

    // Detect playback end: demux reached EOF and audio output has drained
    AVPlayerStatus st = m_codec.status();
    if (st == AVPlayerStatus::EndOfFile) {
        // Give a brief moment for final frames, then signal completion
        m_positionTimer.stop();
        m_codec.stop();
        m_position = 0.0;
        emit positionChanged();
        emit playingChanged();
        emit playbackFinished();
    }
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

PlayerConfig *PlayerWindowManager::config() const
{
    return m_config;
}
