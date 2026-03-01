#pragma once

#include <QObject>
#include <QSize>
#include <QImage>
#include <memory>
#include <atomic>
#include <mutex>

#include "AVPlayerStatus.h"

// FFmpeg (C library)
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

// Forward declarations — avoid heavy includes in the header
QT_BEGIN_NAMESPACE
class QVideoSink;
class QVideoFrame;
class QAudioSink;
class QIODevice;
class QAudioFormat;
QT_END_NAMESPACE

/// @brief Converts decoded AVFrames into renderable video frames and playable
///        audio samples.  Owns the SwsContext / SwrContext and the audio output
///        device.  Thread-safe: methods are called from the decode threads in
///        AVCodecHandler while the render target lives on the main/render thread.
class FrameHandler : public QObject
{
    Q_OBJECT

public:
    explicit FrameHandler(QObject *parent = nullptr);
    ~FrameHandler() override;

    // Non-copyable
    FrameHandler(const FrameHandler &) = delete;
    FrameHandler &operator=(const FrameHandler &) = delete;

    // ────────────────────────────────────────────────────────
    //  Video
    // ────────────────────────────────────────────────────────

    /// Set the rendering backend. Must be called before initVideo().
    void setVideoRenderMode(VideoRenderMode mode);
    VideoRenderMode videoRenderMode() const;

    /// Set the sws scaling filter. Must be called before initVideo().
    void setSwsFilter(SwsFilterMode filter);
    SwsFilterMode swsFilter() const;

    /// Initialise the sws scaler for the given source format.
    /// Call once after the video codec is opened.
    bool initVideo(int srcWidth, int srcHeight, AVPixelFormat srcFmt);

    /// Release sws resources.
    void cleanupVideo();

    /// Convert a decoded video AVFrame and deliver it to the render target.
    /// Called from the video decode thread.
    void processVideoFrame(AVFrame *frame);

    /// Set the QVideoSink target (QVideoSink mode). Ownership stays with caller.
    void setVideoSink(QVideoSink *sink);

    // ────────────────────────────────────────────────────────
    //  Volume
    // ────────────────────────────────────────────────────────

    /// Set the output volume (0.0 = silent, 1.0 = full). Thread-safe.
    void setVolume(qreal linearGain);

    // ────────────────────────────────────────────────────────
    //  Audio
    // ────────────────────────────────────────────────────────

    /// Initialise the swr resampler for the given source format.
    /// Call once after the audio codec is opened.
    /// @param srcSampleRate  e.g. 48000
    /// @param srcChLayout    source channel layout (from AVCodecContext)
    /// @param srcSampleFmt   source sample format (from AVCodecContext)
    /// @param audioTimeBase  stream time_base for PTS → seconds conversion
    bool initAudio(int srcSampleRate, const AVChannelLayout &srcChLayout,
                   AVSampleFormat srcSampleFmt, AVRational audioTimeBase);

    /// Release swr + audio output resources.
    void cleanupAudio();

    /// Resample a decoded audio AVFrame and write PCM to the audio device.
    /// Called from the audio decode thread.
    void processAudioFrame(AVFrame *frame);

    /// Current audio playback position in seconds (used as the master clock).
    double audioClock() const;

    // ────────────────────────────────────────────────────────
    //  Lifecycle helpers
    // ────────────────────────────────────────────────────────

    /// Signal all processing loops to bail out immediately.
    /// Call from the controlling thread *before* joining decode threads.
    void requestAbort();

    /// Release all resources (video + audio).
    void cleanup();

signals:
    /// Emitted (on decode thread) whenever a new QImage is ready.
    /// Connect with Qt::QueuedConnection to receive on the GUI thread.
    void videoFrameReady(const QImage &image);

    /// Emitted when the audio clock is updated (for A/V sync UI).
    void audioClockUpdated(double seconds);

private:
    // ── Video ──
    VideoRenderMode     m_renderMode  = VideoRenderMode::QVideoSink;
    SwsFilterMode       m_swsFilter   = SwsFilterMode::Bilinear;
    SwsContext         *m_swsCtx      = nullptr;
    int                 m_srcWidth    = 0;
    int                 m_srcHeight   = 0;
    AVPixelFormat       m_srcPixFmt   = AV_PIX_FMT_NONE;
    bool                m_is10bit     = false;   ///< true when source is >8-bit

    // QVideoSink path
    QVideoSink         *m_videoSink  = nullptr;
    std::mutex          m_videoSinkMutex;   ///< guards m_videoSink across threads

    // Volume (0.0 – 1.0 linear gain, applied to QAudioSink)
    std::atomic<qreal>  m_volume{0.8};

    // ── Audio ──
    SwrContext         *m_swrCtx     = nullptr;
    int                 m_srcSampleRate = 0;
    AVRational          m_audioTimeBase{0, 1};  ///< stream time_base for PTS

    // Audio output (created on first processAudioFrame call)
    std::unique_ptr<QAudioSink> m_audioSink;
    QIODevice          *m_audioIO    = nullptr;   // non-owning, from QAudioSink::start()

    // Audio clock
    std::atomic<double> m_audioClock{0.0};
    std::atomic<bool>   m_audioAbort{false};      ///< set by cleanupAudio() to unblock write loop

    // Target audio format constants
    static constexpr int    kOutSampleRate = 44100;
    static constexpr int    kOutChannels   = 2;

    // ── Helpers ──
    bool ensureAudioSink();
    bool createAudioSinkImpl();   ///< Must run on the main (GUI) thread
    static int  toSwsFlags(SwsFilterMode mode);
    static bool is10BitFormat(AVPixelFormat fmt);
};
