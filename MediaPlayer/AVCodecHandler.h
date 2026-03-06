#pragma once

#include <QObject>
#include <QString>
#include <QSize>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "AVPlayerStatus.h"
#include "PacketQueue.h"

class FrameHandler;

// FFmpeg headers (C library)
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
}

/// @brief Manages FFmpeg demuxing + codec context + decode threads for a single
///        media file.  NOT a QML_ELEMENT — used internally by PlayerWindowManager.
class AVCodecHandler
{
public:
    AVCodecHandler();
    ~AVCodecHandler();

    // Non-copyable
    AVCodecHandler(const AVCodecHandler &) = delete;
    AVCodecHandler &operator=(const AVCodecHandler &) = delete;

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

    // ── Playback control ──
    /// Allocate queues / threads and begin playback.
    void play();

    /// Pause playback (threads stay alive but block on a condition variable).
    void pause();

    /// Resume from paused state.
    void resume();

    /// Stop playback: abort queues, join threads, flush state.
    void stop();

    /// Seek to the given timestamp (seconds).
    /// If currently playing/paused, playback is restarted from target position.
    bool seek(double seconds);

    // ── Decode backend options ──
    void setDecodeBackend(VideoDecodeBackend backend);
    void setAllowHwFallback(bool allow);
    QString decodeRuntimeStatus() const;

    AVPlayerStatus status() const;

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

    // ── Frame handler ──
    /// Set the FrameHandler to receive decoded frames. Must be set before play().
    /// AVCodecHandler does NOT take ownership.
    void setFrameHandler(FrameHandler *handler);

    // ── Raw access ──
    AVFormatContext *formatContext() const;
    AVCodecContext  *videoCodecContext() const;
    AVCodecContext  *audioCodecContext() const;

private:
    bool openCodec(int streamIndex, AVCodecContext **outCtx);
    bool performSeekInternal(int64_t targetTs);
    bool decodePreviewFrameFromCurrentPos();
    bool trySetupHardwareDecode(const AVCodec *codec, AVCodecContext *ctx);
    static enum AVPixelFormat getHardwareFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);

    // ── Thread entry points (run on worker threads) ──
    void demuxLoop();        ///< Read packets from container → push into queues
    void videoDecodeLoop();  ///< Pop video packets → decode → produce AVFrame
    void audioDecodeLoop();  ///< Pop audio packets → decode → produce PCM

    // ── Thread helpers ──
    void startThreads();
    void joinThreads();

    // ── Pause support ──
    /// Call in each loop iteration; blocks while m_paused is true.
    void waitIfPaused();

    // ── Members: file / codec ──
    QString m_filePath;
    AVFormatContext *m_formatCtx = nullptr;
    AVCodecContext *m_videoCodecCtx = nullptr;
    AVCodecContext *m_audioCodecCtx = nullptr;
    int m_videoStreamIdx = -1;
    int m_audioStreamIdx = -1;

    // ── Members: packet queues ──
    PacketQueue m_videoQueue{128};
    PacketQueue m_audioQueue{64};

    // ── Members: threads ──
    std::thread m_demuxThread;
    std::thread m_videoDecodeThread;
    std::thread m_audioDecodeThread;

    // ── Members: state ──
    std::atomic<AVPlayerStatus> m_status{AVPlayerStatus::Stopped};
    std::atomic<bool> m_abortRequested{false};
    std::atomic<int>  m_activeDecodeThreads{0}; ///< counts running decode threads
    std::atomic<int64_t> m_seekTargetUs{-1};    ///< pending seek target (microseconds), -1 when inactive
    std::atomic<bool> m_waitKeyFrameAfterSeek{false};
    std::atomic<bool> m_logFirstFrameAfterSeek{false};

    // ── Decode backend options ──
    VideoDecodeBackend m_decodeBackend = VideoDecodeBackend::Software;
    bool               m_allowHwFallback = true;

    // ── Hardware decode state ──
    AVBufferRef       *m_hwDeviceCtx = nullptr;
    AVPixelFormat      m_hwPixFmt = AV_PIX_FMT_NONE;
    bool               m_hwDecodeActive = false;
    QString            m_decodeRuntimeStatus = "Software";

    // ── Members: frame handler ──
    FrameHandler *m_frameHandler = nullptr;

    // ── Members: codec operation sync ──
    std::mutex              m_codecMutex;       ///< guards avcodec send/receive/flush operations
    std::mutex              m_formatMutex;      ///< guards avformat read/seek operations

    // ── Members: pause mechanism ──
    std::mutex              m_pauseMutex;
    std::condition_variable m_pauseCond;
    bool                    m_paused = false;
};
