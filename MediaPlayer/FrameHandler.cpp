#include "FrameHandler.h"

#include <QDebug>
#include <QVideoSink>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QAudioSink>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QThread>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

// ════════════════════════════════════════════════════════════
//  Construction / Destruction
// ════════════════════════════════════════════════════════════

FrameHandler::FrameHandler(QObject *parent)
    : QObject(parent)
{
}

FrameHandler::~FrameHandler()
{
    cleanup();
}

// ════════════════════════════════════════════════════════════
//  Video – public API
// ════════════════════════════════════════════════════════════

void FrameHandler::setVideoRenderMode(VideoRenderMode mode)
{
    m_renderMode = mode;
}

VideoRenderMode FrameHandler::videoRenderMode() const
{
    return m_renderMode;
}

void FrameHandler::setSwsFilter(SwsFilterMode filter)
{
    m_swsFilter = filter;
}

SwsFilterMode FrameHandler::swsFilter() const
{
    return m_swsFilter;
}

bool FrameHandler::initVideo(int srcWidth, int srcHeight, AVPixelFormat srcFmt)
{
    cleanupVideo();

    m_srcWidth  = srcWidth;
    m_srcHeight = srcHeight;
    m_srcPixFmt = srcFmt;
    m_is10bit   = is10BitFormat(srcFmt);

    if (m_is10bit) {
        qDebug() << "FrameHandler::initVideo – detected 10-bit source:"
                 << av_get_pix_fmt_name(srcFmt);
    }

    if (m_renderMode == VideoRenderMode::QVideoSink) {
        // Choose the best destination pixel format:
        //   • 8-bit YUV420P source → skip sws entirely (QVideoSink accepts YUV)
        //   • 10-bit source         → convert to P010 (preserves 10-bit precision)
        //   • everything else       → convert to RGBA
        AVPixelFormat dstFmt = AV_PIX_FMT_RGBA;   // fallback

        if (srcFmt == AV_PIX_FMT_YUV420P) {
            // No sws needed — we’ll pass YUV planes directly via QVideoFrame
            return true;
        } else if (m_is10bit) {
            dstFmt = AV_PIX_FMT_P010LE;           // preserve 10-bit
        }

        m_swsCtx = sws_getContext(
            srcWidth, srcHeight, srcFmt,
            srcWidth, srcHeight, dstFmt,
            toSwsFlags(m_swsFilter), nullptr, nullptr, nullptr);

        if (!m_swsCtx) {
            qWarning() << "FrameHandler::initVideo – sws_getContext failed";
            return false;
        }
    }
    // OpenGLTexture path: no sws needed – raw YUV planes are uploaded directly.

    return true;
}

void FrameHandler::cleanupVideo()
{
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    m_srcWidth  = 0;
    m_srcHeight = 0;
    m_srcPixFmt = AV_PIX_FMT_NONE;
}

void FrameHandler::processVideoFrame(AVFrame *frame)
{
    if (!frame || m_audioAbort) {
        if (m_audioAbort) qDebug() << "FrameHandler::processVideoFrame – aborted";
        return;
    }

    if (m_renderMode == VideoRenderMode::QVideoSink) {
        // ── Software path: deliver to QVideoSink ──

        // Fast path: YUV420P 8-bit can be sent directly without sws_scale
        if (m_srcPixFmt == AV_PIX_FMT_YUV420P && !m_swsCtx) {
            QVideoFrameFormat fmt(QSize(m_srcWidth, m_srcHeight),
                                  QVideoFrameFormat::Format_YUV420P);
            QVideoFrame videoFrame(fmt);
            if (!videoFrame.map(QVideoFrame::WriteOnly)) return;

            // Copy Y, U, V planes
            const int ySize  = m_srcWidth * m_srcHeight;
            const int uvSize = (m_srcWidth / 2) * (m_srcHeight / 2);
            // Y plane
            for (int y = 0; y < m_srcHeight; ++y)
                memcpy(videoFrame.bits(0) + y * videoFrame.bytesPerLine(0),
                       frame->data[0] + y * frame->linesize[0], m_srcWidth);
            // U plane
            for (int y = 0; y < m_srcHeight / 2; ++y)
                memcpy(videoFrame.bits(1) + y * videoFrame.bytesPerLine(1),
                       frame->data[1] + y * frame->linesize[1], m_srcWidth / 2);
            // V plane
            for (int y = 0; y < m_srcHeight / 2; ++y)
                memcpy(videoFrame.bits(2) + y * videoFrame.bytesPerLine(2),
                       frame->data[2] + y * frame->linesize[2], m_srcWidth / 2);

            videoFrame.unmap();
            {
                std::lock_guard<std::mutex> lock(m_videoSinkMutex);
                if (m_videoSink) m_videoSink->setVideoFrame(videoFrame);
            }
            return;
        }

        if (!m_swsCtx) return;

        if (m_is10bit) {
            // 10-bit path: sws converts to P010LE → QVideoFrame Format_P010
            QVideoFrameFormat fmt(QSize(m_srcWidth, m_srcHeight),
                                  QVideoFrameFormat::Format_P010);
            QVideoFrame videoFrame(fmt);
            if (!videoFrame.map(QVideoFrame::WriteOnly)) return;

            // P010 has 2 planes: Y (16-bit per pixel), UV interleaved (16-bit)
            uint8_t *dstData[4]     = { videoFrame.bits(0), videoFrame.bits(1),
                                        nullptr, nullptr };
            int      dstLinesize[4] = { videoFrame.bytesPerLine(0),
                                        videoFrame.bytesPerLine(1), 0, 0 };

            sws_scale(m_swsCtx,
                      frame->data, frame->linesize,
                      0, m_srcHeight,
                      dstData, dstLinesize);

            videoFrame.unmap();
            {
                std::lock_guard<std::mutex> lock(m_videoSinkMutex);
                if (m_videoSink) m_videoSink->setVideoFrame(videoFrame);
            }
        } else {
            // 8-bit non-YUV420P path: sws converts to RGBA
            QVideoFrameFormat fmt(QSize(m_srcWidth, m_srcHeight),
                                  QVideoFrameFormat::Format_RGBA8888);
            QVideoFrame videoFrame(fmt);
            if (!videoFrame.map(QVideoFrame::WriteOnly)) return;

            uint8_t *dstData[4]     = { videoFrame.bits(0), nullptr, nullptr, nullptr };
            int      dstLinesize[4] = { videoFrame.bytesPerLine(0), 0, 0, 0 };

            sws_scale(m_swsCtx,
                      frame->data, frame->linesize,
                      0, m_srcHeight,
                      dstData, dstLinesize);

            videoFrame.unmap();
            {
                std::lock_guard<std::mutex> lock(m_videoSinkMutex);
                if (m_videoSink) m_videoSink->setVideoFrame(videoFrame);
            }
        }

    } else {
        // ── OpenGL path: emit raw frame as QImage (fallback until GL node exists) ──

        if (!m_swsCtx) {
            m_swsCtx = sws_getContext(
                m_srcWidth, m_srcHeight, m_srcPixFmt,
                m_srcWidth, m_srcHeight, AV_PIX_FMT_RGBA,
                toSwsFlags(m_swsFilter), nullptr, nullptr, nullptr);
        }
        if (!m_swsCtx) return;

        QImage img(m_srcWidth, m_srcHeight, QImage::Format_RGBA8888);
        uint8_t *dstData[4]     = { img.bits(), nullptr, nullptr, nullptr };
        int      dstLinesize[4] = { static_cast<int>(img.bytesPerLine()), 0, 0, 0 };

        sws_scale(m_swsCtx,
                  frame->data, frame->linesize,
                  0, m_srcHeight,
                  dstData, dstLinesize);

        emit videoFrameReady(img);
    }
}

void FrameHandler::setVideoSink(QVideoSink *sink)
{
    std::lock_guard<std::mutex> lock(m_videoSinkMutex);
    if (m_videoSink == sink) return;
    if (m_videoSink) {
        disconnect(m_videoSink, &QObject::destroyed, this, nullptr);
    }
    m_videoSink = sink;
    if (sink) {
        connect(sink, &QObject::destroyed, this, [this]() {
            qDebug() << "FrameHandler: QVideoSink destroyed, clearing pointer";
            std::lock_guard<std::mutex> guard(m_videoSinkMutex);
            m_videoSink = nullptr;
        });
    }
}

// ════════════════════════════════════════════════════════════
//  Volume
// ════════════════════════════════════════════════════════════

void FrameHandler::setVolume(qreal linearGain)
{
    m_volume = qBound(qreal(0.0), linearGain, qreal(1.0));

    // Apply to the QAudioSink immediately if it exists.
    // QAudioSink::setVolume() is thread-safe in Qt 6.
    if (m_audioSink)
        m_audioSink->setVolume(m_volume.load());
}

// ════════════════════════════════════════════════════════════
//  Audio – public API
// ════════════════════════════════════════════════════════════

bool FrameHandler::initAudio(int srcSampleRate,
                              const AVChannelLayout &srcChLayout,
                              AVSampleFormat srcSampleFmt,
                              AVRational audioTimeBase)
{
    cleanupAudio();
    m_srcSampleRate = srcSampleRate;
    m_audioTimeBase = audioTimeBase;

    // Target layout: stereo
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, kOutChannels);

    // Allocate SwrContext
    int ret = swr_alloc_set_opts2(
        &m_swrCtx,
        &outLayout,      AV_SAMPLE_FMT_S16, kOutSampleRate,  // dst
        &srcChLayout,    srcSampleFmt,       srcSampleRate,   // src
        0, nullptr);

    if (ret < 0 || !m_swrCtx) {
        char err[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, err, sizeof(err));
        qWarning() << "FrameHandler::initAudio – swr_alloc_set_opts2 failed:" << err;
        return false;
    }

    if (swr_init(m_swrCtx) < 0) {
        qWarning() << "FrameHandler::initAudio – swr_init failed";
        swr_free(&m_swrCtx);
        return false;
    }

    m_audioClock = 0.0;
    m_audioAbort = false;

    // Create the audio output device eagerly on the main thread so that
    // processAudioFrame (called from the decode thread) never needs a
    // BlockingQueuedConnection back to the main thread — avoiding deadlock.
    if (!createAudioSinkImpl()) {
        qWarning() << "FrameHandler::initAudio – failed to create audio sink";
    }

    return true;
}

void FrameHandler::cleanupAudio()
{
    qDebug() << "FrameHandler::cleanupAudio – thread:" << QThread::currentThread()
             << "owner:" << this->thread();

    // Signal the write loop in processAudioFrame() to exit immediately
    m_audioAbort = true;

    // Destroy QAudioSink.  Always called from the main thread (after worker
    // threads have been joined), so no cross-thread bounce is needed.
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink.reset();
        m_audioIO = nullptr;
    }
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }
    m_audioClock = 0.0;
}

void FrameHandler::processAudioFrame(AVFrame *frame)
{
    if (!frame || !m_swrCtx) return;
    if (m_audioAbort) {
        qDebug() << "FrameHandler::processAudioFrame – aborted";
        return;
    }

    // Audio sink is created eagerly in initAudio() on the main thread.
    // If it doesn't exist here, we simply cannot output audio.
    if (!m_audioSink || !m_audioIO) return;

    // Calculate output sample count
    int outSamples = swr_get_out_samples(m_swrCtx, frame->nb_samples);
    if (outSamples <= 0) return;

    // Allocate output buffer: S16 interleaved, stereo → 2 bytes × 2 channels per sample
    const int bytesPerSample = kOutChannels * 2;  // sizeof(int16_t) * channels
    QByteArray buffer(outSamples * bytesPerSample, Qt::Uninitialized);

    uint8_t *outBuf = reinterpret_cast<uint8_t *>(buffer.data());
    int converted = swr_convert(m_swrCtx,
                                &outBuf, outSamples,
                                const_cast<const uint8_t **>(frame->data),
                                frame->nb_samples);
    if (converted <= 0) return;

    int totalBytes = converted * bytesPerSample;

    // ── Back-pressure write: pace the decode thread to real-time ──
    // QAudioSink in push mode does not block on write(); it simply buffers
    // the data.  Without back-pressure the entire file would be decoded and
    // buffered in milliseconds, making audioClock jump to the end instantly.
    // We check bytesFree() and sleep when the device buffer is full, so
    // the decode thread naturally runs at ≈1× playback speed.
    if (m_audioIO && m_audioSink) {
        const char *src = buffer.constData();
        int remaining = totalBytes;
        while (remaining > 0 && !m_audioAbort) {
            int freeBytes = m_audioSink->bytesFree();
            if (freeBytes <= 0) {
                // Buffer full — wait ~5 ms for the device to drain some data
                QThread::usleep(5000);
                continue;
            }
            int toWrite = std::min(remaining, freeBytes);
            qint64 written = m_audioIO->write(src, toWrite);
            if (written <= 0) break;
            src       += written;
            remaining -= static_cast<int>(written);
        }
    }

    // ── Update audio clock ──
    // Use the frame PTS directly. With back-pressure the decode thread
    // is paced to ≈1× real-time, so PTS advances smoothly.
    // At EOF the PTS equals the true media duration — no buffer-lag error.
    if (frame->pts != AV_NOPTS_VALUE) {
        double pts = static_cast<double>(frame->pts) * av_q2d(m_audioTimeBase);
        m_audioClock = pts + static_cast<double>(converted) / kOutSampleRate;
    } else {
        m_audioClock = m_audioClock.load()
                       + static_cast<double>(converted) / kOutSampleRate;
    }

    emit audioClockUpdated(m_audioClock.load());
}

double FrameHandler::audioClock() const
{
    return m_audioClock.load();
}

// ════════════════════════════════════════════════════════════
//  Lifecycle
// ════════════════════════════════════════════════════════════

void FrameHandler::requestAbort()
{
    m_audioAbort = true;
}

void FrameHandler::cleanup()
{
    cleanupAudio();
    cleanupVideo();
}

// ════════════════════════════════════════════════════════════
//  Private
// ════════════════════════════════════════════════════════════

bool FrameHandler::ensureAudioSink()
{
    if (m_audioSink) return true;

    // QAudioSink must be created on a thread that owns this QObject
    // (normally the main / GUI thread).  If we are on a different thread,
    // bounce via BlockingQueuedConnection so the creation happens on the
    // correct thread while we wait for the result.
    if (QThread::currentThread() != this->thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, &ok]() {
            ok = createAudioSinkImpl();
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    return createAudioSinkImpl();
}

bool FrameHandler::createAudioSinkImpl()
{
    if (m_audioSink) return true;

    QAudioFormat format;
    format.setSampleRate(kOutSampleRate);
    format.setChannelCount(kOutChannels);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice defaultDev = QMediaDevices::defaultAudioOutput();
    if (!defaultDev.isFormatSupported(format)) {
        qWarning() << "FrameHandler::createAudioSinkImpl – default device does not "
                       "support S16 / 44100 / stereo";
        return false;
    }

    m_audioSink = std::make_unique<QAudioSink>(defaultDev, format, this);
    m_audioSink->setVolume(m_volume.load());
    m_audioIO   = m_audioSink->start();

    if (!m_audioIO) {
        qWarning() << "FrameHandler::createAudioSinkImpl – QAudioSink::start() failed";
        m_audioSink.reset();
        return false;
    }

    return true;
}

int FrameHandler::toSwsFlags(SwsFilterMode mode)
{
    switch (mode) {
    case SwsFilterMode::Point:          return SWS_POINT;
    case SwsFilterMode::FastBilinear:   return SWS_FAST_BILINEAR;
    case SwsFilterMode::Bilinear:       return SWS_BILINEAR;
    case SwsFilterMode::Bicubic:        return SWS_BICUBIC;
    case SwsFilterMode::Lanczos:        return SWS_LANCZOS;
    }
    return SWS_BILINEAR;
}

bool FrameHandler::is10BitFormat(AVPixelFormat fmt)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(fmt);
    if (!desc) return false;
    // Check the bit depth of the first component
    return desc->comp[0].depth > 8;
}
