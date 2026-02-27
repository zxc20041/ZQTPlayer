#pragma once

#include <QObject>
#include <QString>
#include <QSize>
#include <memory>

// FFmpeg headers (C library)
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

/// @brief Custom deleter for AVFormatContext (avformat_close_input needs a **)
struct AVFormatContextDeleter {
    void operator()(AVFormatContext *ctx) const {
        if (ctx) {
            avformat_close_input(&ctx);
        }
    }
};

/// @brief Manages FFmpeg demuxing + codec context for a single media file.
///        NOT a QML_ELEMENT — used internally by PlayerWindowManager.
class AVCodecHandler
{
public:
    AVCodecHandler();
    ~AVCodecHandler();

    // ── File path ──
    void setFilePath(const QString &path);
    QString filePath() const;

    // ── Lifecycle ──
    /// Open the file and initialise video & audio codec contexts.
    /// Returns true on success.
    bool open();

    /// Release all FFmpeg resources (codecs + format context).
    void close();

    bool isOpen() const;

    // ── Stream metadata (valid after open()) ──
    int videoStreamIndex() const;
    int audioStreamIndex() const;

    /// Video resolution (width × height) in pixels
    QSize videoResolution() const;

    /// Total duration in seconds (from container)
    double durationSeconds() const;

    /// Overall bit rate in kbps
    double bitRateKbps() const;

    /// Video codec name, e.g. "h264"
    QString videoCodecName() const;

    /// Audio codec name, e.g. "aac"
    QString audioCodecName() const;

    /// Video frame rate (fps)
    double videoFrameRate() const;

    /// Audio sample rate (Hz)
    int audioSampleRate() const;

    /// Audio channel count
    int audioChannels() const;

    // ── Raw access (for future decoding loop) ──
    AVFormatContext *formatContext() const;
    AVCodecContext  *videoCodecContext() const;
    AVCodecContext  *audioCodecContext() const;

private:
    bool openCodec(int streamIndex, AVCodecContext **outCtx);

    QString m_filePath;
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> m_formatCtx;
    AVCodecContext *m_videoCodecCtx = nullptr;
    AVCodecContext *m_audioCodecCtx = nullptr;
    int m_videoStreamIdx = -1;
    int m_audioStreamIdx = -1;
};
