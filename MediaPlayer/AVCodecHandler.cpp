#include "AVCodecHandler.h"
#include "FrameHandler.h"
#include <QDebug>
#include <chrono>
#include <limits>

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

namespace {
inline const char *safePixFmtName(AVPixelFormat fmt)
{
    const char *name = av_get_pix_fmt_name(fmt);
    return name ? name : "unknown";
}
}

AVCodecHandler::AVCodecHandler() = default;

AVCodecHandler::~AVCodecHandler()
{
    stop();
    close();
}

// ── File path ──────────────────────────────────────────────

void AVCodecHandler::setFilePath(const QString &path)
{
    if (m_formatCtx) {
        close();
    }
    m_filePath = path;
}

QString AVCodecHandler::filePath() const
{
    return m_filePath;
}

// ── Lifecycle ──────────────────────────────────────────────

bool AVCodecHandler::open()
{
    if (m_filePath.isEmpty()) {
        qWarning() << "AVCodecHandler::open() - file path is empty";
        return false;
    }

    close(); // ensure clean state

    // Open format context
    AVFormatContext *rawCtx = nullptr;
    int ret = avformat_open_input(&rawCtx, m_filePath.toUtf8().constData(),
                                   nullptr, nullptr);
    if (ret < 0 || !rawCtx) {
        char err[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, err, sizeof(err));
        qWarning() << "avformat_open_input failed:" << err;
        return false;
    }
    m_formatCtx = rawCtx;

    // Limit stream analysis to reduce UI freeze on main thread
    m_formatCtx->probesize       = 1000000;   // 1 MB max probe data
    m_formatCtx->max_analyze_duration = 500000; // 0.5 s max analysis

    // Read stream info
    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        qWarning() << "avformat_find_stream_info failed";
        close();
        return false;
    }

    // Find best video & audio streams
    m_videoStreamIdx = av_find_best_stream(m_formatCtx, AVMEDIA_TYPE_VIDEO,
                                            -1, -1, nullptr, 0);
    m_audioStreamIdx = av_find_best_stream(m_formatCtx, AVMEDIA_TYPE_AUDIO,
                                            -1, -1, nullptr, 0);

    // Open video codec
    if (m_videoStreamIdx >= 0) {
        if (!openCodec(m_videoStreamIdx, &m_videoCodecCtx)) {
            qWarning() << "Failed to open video codec";
        }
    }

    // Open audio codec
    if (m_audioStreamIdx >= 0) {
        if (!openCodec(m_audioStreamIdx, &m_audioCodecCtx)) {
            qWarning() << "Failed to open audio codec";
        }
    }

    if (!m_videoCodecCtx && !m_audioCodecCtx) {
        qWarning() << "No decodable video or audio stream found";
        close();
        return false;
    }

    m_seekTargetUs = -1;
    m_waitKeyFrameAfterSeek = false;

    return true;
}

void AVCodecHandler::close()
{
    if (m_videoCodecCtx) {
        avcodec_free_context(&m_videoCodecCtx);
        m_videoCodecCtx = nullptr;
    }
    if (m_audioCodecCtx) {
        avcodec_free_context(&m_audioCodecCtx);
        m_audioCodecCtx = nullptr;
    }
    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
    }
    if (m_hwDeviceCtx) {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }
    m_videoStreamIdx = -1;
    m_audioStreamIdx = -1;
    m_seekTargetUs = -1;
    m_waitKeyFrameAfterSeek = false;
    m_hwPixFmt = AV_PIX_FMT_NONE;
    m_hwDecodeActive = false;
    m_decodeRuntimeStatus = "Software";
}

bool AVCodecHandler::isOpen() const
{
    return m_formatCtx != nullptr;
}

// ── Stream metadata ────────────────────────────────────────

int AVCodecHandler::videoStreamIndex() const { return m_videoStreamIdx; }
int AVCodecHandler::audioStreamIndex() const { return m_audioStreamIdx; }

QSize AVCodecHandler::videoResolution() const
{
    if (m_videoCodecCtx) {
        return QSize(m_videoCodecCtx->width, m_videoCodecCtx->height);
    }
    return QSize();
}

double AVCodecHandler::durationSeconds() const
{
    if (m_formatCtx && m_formatCtx->duration != AV_NOPTS_VALUE) {
        return static_cast<double>(m_formatCtx->duration) / AV_TIME_BASE;
    }
    return 0.0;
}

double AVCodecHandler::bitRateKbps() const
{
    if (m_formatCtx && m_formatCtx->bit_rate > 0) {
        return static_cast<double>(m_formatCtx->bit_rate) / 1000.0;
    }
    return 0.0;
}

QString AVCodecHandler::videoCodecName() const
{
    if (m_videoCodecCtx && m_videoCodecCtx->codec) {
        return QString::fromUtf8(m_videoCodecCtx->codec->name);
    }
    return {};
}

QString AVCodecHandler::audioCodecName() const
{
    if (m_audioCodecCtx && m_audioCodecCtx->codec) {
        return QString::fromUtf8(m_audioCodecCtx->codec->name);
    }
    return {};
}

double AVCodecHandler::videoFrameRate() const
{
    if (m_formatCtx && m_videoStreamIdx >= 0) {
        AVRational fr = m_formatCtx->streams[m_videoStreamIdx]->avg_frame_rate;
        if (fr.den > 0) {
            return static_cast<double>(fr.num) / fr.den;
        }
    }
    return 0.0;
}

int AVCodecHandler::audioSampleRate() const
{
    if (m_audioCodecCtx) {
        return m_audioCodecCtx->sample_rate;
    }
    return 0;
}

int AVCodecHandler::audioChannels() const
{
    if (m_audioCodecCtx) {
        return m_audioCodecCtx->ch_layout.nb_channels;
    }
    return 0;
}

// ── Raw access ─────────────────────────────────────────────

AVFormatContext *AVCodecHandler::formatContext() const
{
    return m_formatCtx;
}

AVCodecContext *AVCodecHandler::videoCodecContext() const
{
    return m_videoCodecCtx;
}

AVCodecContext *AVCodecHandler::audioCodecContext() const
{
    return m_audioCodecCtx;
}

// ── Private ────────────────────────────────────────────────

// ── Playback control ───────────────────────────────────────

void AVCodecHandler::play()
{
    if (!isOpen()) {
        qWarning() << "AVCodecHandler::play() - not open";
        return;
    }
    const AVPlayerStatus oldStatus = m_status.load();
    if (m_status == AVPlayerStatus::Playing) return;

    if (m_status == AVPlayerStatus::Paused) {
        resume();
        return;
    }

    // Fresh start — join any leftover threads from a previous run (e.g. after EOF)
    joinThreads();

    m_abortRequested = false;
    m_paused = false;
    m_activeDecodeThreads = 0;
    m_videoQueue.restart();
    m_audioQueue.restart();

    // Restart from beginning only when replaying after EOF.
    // For normal play-after-seek, keep the current demux position.
    if (oldStatus == AVPlayerStatus::Stopped ||
        oldStatus == AVPlayerStatus::EndOfFile ||
        oldStatus == AVPlayerStatus::PlaybackDone) {
        std::lock_guard formatLock(m_formatMutex);
        av_seek_frame(m_formatCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
    }
    if (m_videoCodecCtx) avcodec_flush_buffers(m_videoCodecCtx);
    if (m_audioCodecCtx) avcodec_flush_buffers(m_audioCodecCtx);

    // Initialise FrameHandler contexts
    if (m_frameHandler) {
        if (m_videoCodecCtx) {
            AVPixelFormat renderPixFmt = m_videoCodecCtx->pix_fmt;
            if (m_hwDecodeActive && m_videoCodecCtx->sw_pix_fmt != AV_PIX_FMT_NONE) {
                renderPixFmt = m_videoCodecCtx->sw_pix_fmt;
            }

            m_frameHandler->initVideo(m_videoCodecCtx->width,
                                      m_videoCodecCtx->height,
                                      renderPixFmt);
        }
        if (m_audioCodecCtx) {
            AVRational audioTb = m_formatCtx->streams[m_audioStreamIdx]->time_base;
            m_frameHandler->initAudio(m_audioCodecCtx->sample_rate,
                                      m_audioCodecCtx->ch_layout,
                                      static_cast<AVSampleFormat>(m_audioCodecCtx->sample_fmt),
                                      audioTb);
        }
    }

    m_status = AVPlayerStatus::Playing;
    startThreads();
}

void AVCodecHandler::pause()
{
    if (m_status != AVPlayerStatus::Playing) return;

    if (m_frameHandler) {
        m_frameHandler->pauseAudioOutput();
    }

    {
        std::lock_guard lock(m_pauseMutex);
        m_paused = true;
    }
    m_status = AVPlayerStatus::Paused;
}

void AVCodecHandler::resume()
{
    if (m_status != AVPlayerStatus::Paused) return;

    {
        std::lock_guard lock(m_pauseMutex);
        m_paused = false;
    }

    if (m_frameHandler) {
        m_frameHandler->resumeAudioOutput();
    }

    m_pauseCond.notify_all();
    m_status = AVPlayerStatus::Playing;
}

void AVCodecHandler::stop()
{
    if (m_status == AVPlayerStatus::Stopped) return;

    qDebug() << "AVCodecHandler::stop() - begin";
    m_abortRequested = true;

    // Unblock paused threads first
    {
        std::lock_guard lock(m_pauseMutex);
        m_paused = false;
    }
    m_pauseCond.notify_all();

    // Abort the queues so blocked push/pop return immediately
    m_videoQueue.abort();
    m_audioQueue.abort();

    // Signal FrameHandler to bail out of any blocking write loops
    // *before* we join, so the threads can actually exit.
    if (m_frameHandler)
        m_frameHandler->requestAbort();

    joinThreads();

    m_videoQueue.flush();
    m_audioQueue.flush();

    if (m_frameHandler)
        m_frameHandler->cleanup();

    m_status = AVPlayerStatus::Stopped;
    m_seekTargetUs = -1;
    m_waitKeyFrameAfterSeek = false;
    qDebug() << "AVCodecHandler::stop() - done";
}

bool AVCodecHandler::seek(double seconds)
{
    if (!isOpen()) return false;

    const double duration = durationSeconds();
    if (duration > 0.0) {
        if (seconds < 0.0) seconds = 0.0;
        if (seconds > duration) seconds = duration;
    } else if (seconds < 0.0) {
        seconds = 0.0;
    }

    const int64_t targetTs = static_cast<int64_t>(seconds * AV_TIME_BASE);
    const AVPlayerStatus st = m_status.load();

    // Keep worker threads/resources alive and perform seek immediately.
    // This avoids thread recreation stalls during slider interaction.
    Q_UNUSED(st);
    m_seekTargetUs = targetTs;
    if (!performSeekInternal(targetTs)) {
        m_seekTargetUs = -1;
        return false;
    }

    // Always gate video output on a keyframe immediately after seek.
    // This avoids rendering corrupt predictive frames (notably HEVC on Linux)
    // before decoder references are fully rebuilt.
    m_waitKeyFrameAfterSeek = true;
    m_logFirstFrameAfterSeek = true;

    if (st == AVPlayerStatus::Paused && m_videoCodecCtx && m_frameHandler) {
        decodePreviewFrameFromCurrentPos();
    }

    m_seekTargetUs = -1;
    return true;
}

void AVCodecHandler::setDecodeBackend(VideoDecodeBackend backend)
{
    m_decodeBackend = backend;
}

void AVCodecHandler::setAllowHwFallback(bool allow)
{
    m_allowHwFallback = allow;
}

QString AVCodecHandler::decodeRuntimeStatus() const
{
    return m_decodeRuntimeStatus;
}

AVPlayerStatus AVCodecHandler::status() const
{
    return m_status.load();
}

// ── Thread helpers ─────────────────────────────────────────

void AVCodecHandler::startThreads()
{
    m_demuxThread       = std::thread(&AVCodecHandler::demuxLoop, this);
    if (m_videoCodecCtx)
        m_videoDecodeThread = std::thread(&AVCodecHandler::videoDecodeLoop, this);
    if (m_audioCodecCtx)
        m_audioDecodeThread = std::thread(&AVCodecHandler::audioDecodeLoop, this);
}

void AVCodecHandler::joinThreads()
{
    qDebug() << "AVCodecHandler::joinThreads - waiting...";
    if (m_demuxThread.joinable())       m_demuxThread.join();
    qDebug() << "AVCodecHandler::joinThreads - demux joined";
    if (m_videoDecodeThread.joinable()) m_videoDecodeThread.join();
    qDebug() << "AVCodecHandler::joinThreads - video joined";
    if (m_audioDecodeThread.joinable()) m_audioDecodeThread.join();
    qDebug() << "AVCodecHandler::joinThreads - all joined";
}

void AVCodecHandler::waitIfPaused()
{
    std::unique_lock lock(m_pauseMutex);
    m_pauseCond.wait(lock, [this] { return !m_paused || m_abortRequested; });
}

// ── Thread loops ───────────────────────────────────────────

void AVCodecHandler::demuxLoop()
{
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) return;

    while (!m_abortRequested) {
        waitIfPaused();
        if (m_abortRequested) break;

        int ret = 0;
        {
            std::lock_guard formatLock(m_formatMutex);
            ret = av_read_frame(m_formatCtx, pkt);
        }
        if (ret < 0) {
            // EOF or error — signal decoders that no more packets are coming
            m_status = AVPlayerStatus::EndOfFile;
            break;
        }

        if (pkt->stream_index == m_videoStreamIdx) {
            if (!m_videoQueue.push(pkt)) break;   // aborted
        } else if (pkt->stream_index == m_audioStreamIdx) {
            if (!m_audioQueue.push(pkt)) break;   // aborted
        }
        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);

    qDebug() << "AVCodecHandler::demuxLoop - exiting, signalling EOF to queues";

    // Signal decode threads that no more packets are coming.
    // Use signalEOF() instead of abort() so they can drain the remaining
    // packets before exiting.
    m_videoQueue.signalEOF();
    m_audioQueue.signalEOF();
}

void AVCodecHandler::videoDecodeLoop()
{
    m_activeDecodeThreads.fetch_add(1);
    AVPacket *pkt = nullptr;
    AVFrame  *frame = av_frame_alloc();
    if (!frame) { m_activeDecodeThreads.fetch_sub(1); return; }

    while (!m_abortRequested) {
        waitIfPaused();
        if (m_abortRequested) break;

        if (!m_videoQueue.pop(&pkt)) break;      // aborted or EOF

        int ret = 0;
        {
            std::lock_guard lock(m_codecMutex);
            ret = avcodec_send_packet(m_videoCodecCtx, pkt);
        }
        av_packet_free(&pkt);
        if (ret < 0) continue;

        while (ret >= 0 && !m_abortRequested) {
            {
                std::lock_guard lock(m_codecMutex);
                ret = avcodec_receive_frame(m_videoCodecCtx, frame);
            }
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) break;

            AVFrame *renderFrame = frame;
            AVFrame *swFrame = nullptr;

            if (m_hwDecodeActive && frame->format == m_hwPixFmt) {
                swFrame = av_frame_alloc();
                if (!swFrame) {
                    av_frame_unref(frame);
                    continue;
                }

                if (av_hwframe_transfer_data(swFrame, frame, 0) < 0) {
                    av_frame_free(&swFrame);
                    av_frame_unref(frame);
                    continue;
                }

                swFrame->pts = frame->pts;
                swFrame->pkt_dts = frame->pkt_dts;
                swFrame->best_effort_timestamp = frame->best_effort_timestamp;
                swFrame->flags = frame->flags;
                renderFrame = swFrame;
            }

            if (m_waitKeyFrameAfterSeek.load() && !(renderFrame->flags & AV_FRAME_FLAG_KEY)) {
                if (swFrame) {
                    av_frame_free(&swFrame);
                }
                av_frame_unref(frame);
                continue;
            }

            if (m_waitKeyFrameAfterSeek.load() && (renderFrame->flags & AV_FRAME_FLAG_KEY)) {
                m_waitKeyFrameAfterSeek = false;
            }

            const int64_t seekTargetUs = m_seekTargetUs.load();
            if (seekTargetUs >= 0 && renderFrame->pts != AV_NOPTS_VALUE) {
                AVRational tb = m_formatCtx->streams[m_videoStreamIdx]->time_base;
                const int64_t frameUs = av_rescale_q(renderFrame->pts, tb, AVRational{1, AV_TIME_BASE});

                if (frameUs + 100000 < seekTargetUs) {
                    if (swFrame) {
                        av_frame_free(&swFrame);
                    }
                    av_frame_unref(frame);
                    continue;
                }

                if (!m_audioCodecCtx) {
                    int64_t expected = seekTargetUs;
                    m_seekTargetUs.compare_exchange_strong(expected, -1);
                }
            }

            // ── PTS-based sync: pace video frames to audio clock ──
            if (m_frameHandler && renderFrame->pts != AV_NOPTS_VALUE) {
                AVRational tb = m_formatCtx->streams[m_videoStreamIdx]->time_base;
                double videoPts = renderFrame->pts * av_q2d(tb);

                // Sync to audio clock when audio stream is present
                if (m_audioCodecCtx) {
                    double clock = m_frameHandler->audioClock();
                    if (clock > 0.0) {
                        double diff = videoPts - clock;
                        if (diff > 0.005) {
                            // Video ahead of audio → interruptible sleep to sync
                            auto us = static_cast<int64_t>(diff * 1e6);
                            if (us > 0 && us < 5000000) { // safety cap 5 s
                                std::unique_lock lock(m_pauseMutex);
                                m_pauseCond.wait_for(lock, std::chrono::microseconds(us),
                                    [this] { return m_abortRequested.load(); });
                            }
                        } else if (diff < -0.05) {
                            // Video behind >50 ms → drop frame
                            av_frame_unref(frame);
                            continue;
                        }
                    }
                }
            }

            if (m_frameHandler)
                m_frameHandler->processVideoFrame(renderFrame);

            bool expected = true;
            if (m_logFirstFrameAfterSeek.compare_exchange_strong(expected, false)) {
                const AVPixelFormat outFmt = static_cast<AVPixelFormat>(renderFrame->format);
                qDebug() << "Seek first frame -> fmt:" << safePixFmtName(outFmt)
                         << "hwActive:" << m_hwDecodeActive
                         << "key:" << bool(renderFrame->flags & AV_FRAME_FLAG_KEY)
                         << "pts:" << renderFrame->pts;
            }

            if (swFrame) {
                av_frame_free(&swFrame);
            }

            av_frame_unref(frame);
        }
    }

    // Flush decoder (drain buffered frames) — skip if abort was requested
    if (!m_abortRequested) {
        {
            std::lock_guard lock(m_codecMutex);
            avcodec_send_packet(m_videoCodecCtx, nullptr);
        }
        while (!m_abortRequested) {
            int ret = 0;
            {
                std::lock_guard lock(m_codecMutex);
                ret = avcodec_receive_frame(m_videoCodecCtx, frame);
            }
            if (ret != 0) break;

            AVFrame *renderFrame = frame;
            AVFrame *swFrame = nullptr;

            if (m_hwDecodeActive && frame->format == m_hwPixFmt) {
                swFrame = av_frame_alloc();
                if (!swFrame) {
                    av_frame_unref(frame);
                    continue;
                }

                if (av_hwframe_transfer_data(swFrame, frame, 0) < 0) {
                    av_frame_free(&swFrame);
                    av_frame_unref(frame);
                    continue;
                }

                swFrame->pts = frame->pts;
                swFrame->pkt_dts = frame->pkt_dts;
                swFrame->best_effort_timestamp = frame->best_effort_timestamp;
                swFrame->flags = frame->flags;
                renderFrame = swFrame;
            }

            if (m_frameHandler)
                m_frameHandler->processVideoFrame(renderFrame);

            bool expected = true;
            if (m_logFirstFrameAfterSeek.compare_exchange_strong(expected, false)) {
                const AVPixelFormat outFmt = static_cast<AVPixelFormat>(renderFrame->format);
                qDebug() << "Seek first frame -> fmt:" << safePixFmtName(outFmt)
                         << "hwActive:" << m_hwDecodeActive
                         << "key:" << bool(renderFrame->flags & AV_FRAME_FLAG_KEY)
                         << "pts:" << renderFrame->pts;
            }

            if (swFrame) {
                av_frame_free(&swFrame);
            }
            av_frame_unref(frame);
        }
    }

    qDebug() << "AVCodecHandler::videoDecodeLoop - exiting";
    av_frame_free(&frame);
    if (m_activeDecodeThreads.fetch_sub(1) == 1) {
        // Last decode thread to finish → signal PlaybackDone
        AVPlayerStatus expected = AVPlayerStatus::EndOfFile;
        m_status.compare_exchange_strong(expected, AVPlayerStatus::PlaybackDone);
        qDebug() << "AVCodecHandler: all decode threads done → PlaybackDone";
    }
}

void AVCodecHandler::audioDecodeLoop()
{
    m_activeDecodeThreads.fetch_add(1);
    AVPacket *pkt = nullptr;
    AVFrame  *frame = av_frame_alloc();
    if (!frame) { m_activeDecodeThreads.fetch_sub(1); return; }

    while (!m_abortRequested) {
        waitIfPaused();
        if (m_abortRequested) break;

        if (!m_audioQueue.pop(&pkt)) break;      // aborted or EOF

        int ret = 0;
        {
            std::lock_guard lock(m_codecMutex);
            ret = avcodec_send_packet(m_audioCodecCtx, pkt);
        }
        av_packet_free(&pkt);
        if (ret < 0) continue;

        while (ret >= 0 && !m_abortRequested) {
            {
                std::lock_guard lock(m_codecMutex);
                ret = avcodec_receive_frame(m_audioCodecCtx, frame);
            }
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) break;

            if (m_waitKeyFrameAfterSeek.load()) {
                av_frame_unref(frame);
                continue;
            }

            const int64_t seekTargetUs = m_seekTargetUs.load();
            if (seekTargetUs >= 0 && frame->pts != AV_NOPTS_VALUE) {
                AVRational tb = m_formatCtx->streams[m_audioStreamIdx]->time_base;
                const int64_t frameStartUs = av_rescale_q(frame->pts, tb, AVRational{1, AV_TIME_BASE});
                const int sampleRate = frame->sample_rate > 0 ? frame->sample_rate : m_audioCodecCtx->sample_rate;
                const int64_t frameDurUs = sampleRate > 0
                    ? av_rescale_q(frame->nb_samples, AVRational{1, sampleRate}, AVRational{1, AV_TIME_BASE})
                    : 0;
                const int64_t frameEndUs = frameStartUs + frameDurUs;

                if (frameEndUs + 100000 < seekTargetUs) {
                    av_frame_unref(frame);
                    continue;
                }

                int64_t expected = seekTargetUs;
                m_seekTargetUs.compare_exchange_strong(expected, -1);
            }

            if (m_frameHandler)
                m_frameHandler->processAudioFrame(frame);

            av_frame_unref(frame);
        }
    }

    // Flush decoder — skip if abort was requested
    if (!m_abortRequested) {
        {
            std::lock_guard lock(m_codecMutex);
            avcodec_send_packet(m_audioCodecCtx, nullptr);
        }
        while (!m_abortRequested) {
            int ret = 0;
            {
                std::lock_guard lock(m_codecMutex);
                ret = avcodec_receive_frame(m_audioCodecCtx, frame);
            }
            if (ret != 0) break;
            if (m_frameHandler)
                m_frameHandler->processAudioFrame(frame);
            av_frame_unref(frame);
        }
    }

    qDebug() << "AVCodecHandler::audioDecodeLoop - exiting";
    av_frame_free(&frame);
    if (m_activeDecodeThreads.fetch_sub(1) == 1) {
        // Last decode thread to finish → signal PlaybackDone
        AVPlayerStatus expected = AVPlayerStatus::EndOfFile;
        m_status.compare_exchange_strong(expected, AVPlayerStatus::PlaybackDone);
        qDebug() << "AVCodecHandler: all decode threads done → PlaybackDone";
    }
}

// ── Frame handler ──────────────────────────────────────────

void AVCodecHandler::setFrameHandler(FrameHandler *handler)
{
    m_frameHandler = handler;
}

bool AVCodecHandler::performSeekInternal(int64_t targetTs)
{
    int ret = 0;
    {
        std::lock_guard formatLock(m_formatMutex);
        if (m_videoStreamIdx >= 0) {
            AVRational tb = m_formatCtx->streams[m_videoStreamIdx]->time_base;
            const int64_t streamTs = av_rescale_q(targetTs, AVRational{1, AV_TIME_BASE}, tb);
            ret = av_seek_frame(m_formatCtx, m_videoStreamIdx, streamTs, AVSEEK_FLAG_BACKWARD);
        } else {
            ret = av_seek_frame(m_formatCtx, -1, targetTs, AVSEEK_FLAG_BACKWARD);
        }

        if (ret < 0) {
            ret = avformat_seek_file(m_formatCtx, -1,
                                     std::numeric_limits<int64_t>::min(),
                                     targetTs,
                                     std::numeric_limits<int64_t>::max(),
                                     0);
        }
    }
    if (ret < 0) {
        char err[AV_ERROR_MAX_STRING_SIZE]{};
        av_strerror(ret, err, sizeof(err));
        qWarning() << "AVCodecHandler::performSeekInternal() failed:" << err;
        return false;
    }

    m_videoQueue.flush();
    m_audioQueue.flush();

    {
        std::lock_guard lock(m_codecMutex);
        if (m_videoCodecCtx) avcodec_flush_buffers(m_videoCodecCtx);
        if (m_audioCodecCtx) avcodec_flush_buffers(m_audioCodecCtx);
    }

    return true;
}

bool AVCodecHandler::decodePreviewFrameFromCurrentPos()
{
    AVPacket *pkt = av_packet_alloc();
    AVFrame  *frame = av_frame_alloc();
    if (!pkt || !frame) {
        if (pkt) av_packet_free(&pkt);
        if (frame) av_frame_free(&frame);
        return false;
    }

    bool gotFrame = false;
    const int64_t seekTargetUs = m_seekTargetUs.load();

    for (int i = 0; i < 400 && !m_abortRequested; ++i) {
        int ret = 0;
        {
            std::lock_guard formatLock(m_formatMutex);
            ret = av_read_frame(m_formatCtx, pkt);
        }
        if (ret < 0) break;

        if (pkt->stream_index != m_videoStreamIdx) {
            av_packet_unref(pkt);
            continue;
        }

        {
            std::lock_guard codecLock(m_codecMutex);
            ret = avcodec_send_packet(m_videoCodecCtx, pkt);
        }
        av_packet_unref(pkt);
        if (ret < 0) continue;

        while (!m_abortRequested) {
            {
                std::lock_guard codecLock(m_codecMutex);
                ret = avcodec_receive_frame(m_videoCodecCtx, frame);
            }

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) break;

            if (seekTargetUs >= 0 && frame->pts != AV_NOPTS_VALUE) {
                AVRational tb = m_formatCtx->streams[m_videoStreamIdx]->time_base;
                const int64_t frameUs = av_rescale_q(frame->pts, tb, AVRational{1, AV_TIME_BASE});
                if (frameUs + 100000 < seekTargetUs) {
                    av_frame_unref(frame);
                    continue;
                }
            }

            AVFrame *renderFrame = frame;
            AVFrame *swFrame = nullptr;
            if (m_hwDecodeActive && frame->format == m_hwPixFmt) {
                swFrame = av_frame_alloc();
                if (!swFrame) {
                    av_frame_unref(frame);
                    continue;
                }

                if (av_hwframe_transfer_data(swFrame, frame, 0) < 0) {
                    av_frame_free(&swFrame);
                    av_frame_unref(frame);
                    continue;
                }

                swFrame->pts = frame->pts;
                swFrame->pkt_dts = frame->pkt_dts;
                swFrame->best_effort_timestamp = frame->best_effort_timestamp;
                swFrame->flags = frame->flags;
                renderFrame = swFrame;
            }

            // Preview path: only output keyframes to avoid transient corruption
            // right after seek on long-GOP codecs (e.g. HEVC).
            if (!(renderFrame->flags & AV_FRAME_FLAG_KEY)) {
                if (swFrame) {
                    av_frame_free(&swFrame);
                }
                av_frame_unref(frame);
                continue;
            }

            m_waitKeyFrameAfterSeek = false;

            if (m_frameHandler)
                m_frameHandler->processVideoFrame(renderFrame);

            if (swFrame) {
                av_frame_free(&swFrame);
            }

            av_frame_unref(frame);
            gotFrame = true;
            break;
        }

        if (gotFrame) break;
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    return gotFrame;
}

// ── Private codec helper ───────────────────────────────────

bool AVCodecHandler::openCodec(int streamIndex, AVCodecContext **outCtx)
{
    AVCodecParameters *par = m_formatCtx->streams[streamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(par->codec_id);
    if (!codec) {
        return false;
    }

    AVCodecContext *ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        return false;
    }

    if (avcodec_parameters_to_context(ctx, par) < 0) {
        avcodec_free_context(&ctx);
        return false;
    }

    if (streamIndex == m_videoStreamIdx) {
        m_hwDecodeActive = false;
        m_hwPixFmt = AV_PIX_FMT_NONE;

        if (m_decodeBackend != VideoDecodeBackend::Software) {
            const bool hwReady = trySetupHardwareDecode(codec, ctx);
            if (!hwReady && !m_allowHwFallback) {
                qWarning() << "Hardware decode requested without fallback, but init failed.";
                avcodec_free_context(&ctx);
                return false;
            }

            if (!hwReady) {
                m_decodeRuntimeStatus = "Software(Fallback)";
            }
        }
    }

    if (avcodec_open2(ctx, codec, nullptr) < 0) {
        avcodec_free_context(&ctx);
        return false;
    }

    *outCtx = ctx;
    return true;
}

bool AVCodecHandler::trySetupHardwareDecode(const AVCodec *codec, AVCodecContext *ctx)
{
    if (!codec || !ctx) return false;

    AVHWDeviceType candidates[6] = {
#ifdef _WIN32
        AV_HWDEVICE_TYPE_D3D11VA,
        AV_HWDEVICE_TYPE_DXVA2,
#endif
#ifdef __linux__
        AV_HWDEVICE_TYPE_VAAPI,
#endif
#ifdef __APPLE__
        AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
#endif
        AV_HWDEVICE_TYPE_NONE,
        AV_HWDEVICE_TYPE_NONE
    };

    if (m_decodeBackend == VideoDecodeBackend::D3D11VA) {
        candidates[0] = AV_HWDEVICE_TYPE_D3D11VA;
        candidates[1] = AV_HWDEVICE_TYPE_NONE;
    } else if (m_decodeBackend == VideoDecodeBackend::VAAPI) {
        candidates[0] = AV_HWDEVICE_TYPE_VAAPI;
        candidates[1] = AV_HWDEVICE_TYPE_NONE;
    } else if (m_decodeBackend == VideoDecodeBackend::VideoToolbox) {
        candidates[0] = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
        candidates[1] = AV_HWDEVICE_TYPE_NONE;
    }

    for (int ci = 0; ci < 6 && candidates[ci] != AV_HWDEVICE_TYPE_NONE; ++ci) {
        AVHWDeviceType devType = candidates[ci];

        AVPixelFormat hwPixFmt = AV_PIX_FMT_NONE;
        for (int i = 0;; ++i) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
            if (!config) break;
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
                config->device_type == devType) {
                hwPixFmt = config->pix_fmt;
                break;
            }
        }
        if (hwPixFmt == AV_PIX_FMT_NONE)
            continue;

        AVBufferRef *deviceRef = nullptr;
        if (av_hwdevice_ctx_create(&deviceRef, devType, nullptr, nullptr, 0) < 0 || !deviceRef)
            continue;

        if (m_hwDeviceCtx) {
            av_buffer_unref(&m_hwDeviceCtx);
        }
        m_hwDeviceCtx = deviceRef;
        m_hwPixFmt = hwPixFmt;

        ctx->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
        ctx->opaque = this;
        ctx->get_format = &AVCodecHandler::getHardwareFormat;
        m_hwDecodeActive = true;

        const char *devName = av_hwdevice_get_type_name(devType);
        const QString dev = devName ? QString::fromUtf8(devName) : QStringLiteral("HW");
        m_decodeRuntimeStatus = dev + QStringLiteral("->Transfer");

        qDebug() << "Hardware decode enabled:" << av_hwdevice_get_type_name(devType)
                 << "pix_fmt:" << av_get_pix_fmt_name(m_hwPixFmt);
        return true;
    }

    m_hwDecodeActive = false;
    m_hwPixFmt = AV_PIX_FMT_NONE;
    m_decodeRuntimeStatus = "Software";
    return false;
}

enum AVPixelFormat AVCodecHandler::getHardwareFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    auto *self = static_cast<AVCodecHandler *>(ctx->opaque);
    if (self && self->m_hwPixFmt != AV_PIX_FMT_NONE) {
        for (const enum AVPixelFormat *p = pix_fmts; p && *p != AV_PIX_FMT_NONE; ++p) {
            if (*p == self->m_hwPixFmt) {
                return *p;
            }
        }
    }

    return pix_fmts[0];
}
