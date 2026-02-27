#include "AVCodecHandler.h"
#include <QDebug>

AVCodecHandler::AVCodecHandler() = default;

AVCodecHandler::~AVCodecHandler()
{
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
        qWarning() << "AVCodecHandler::open() – file path is empty";
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
    m_formatCtx.reset(rawCtx);

    // Read stream info
    if (avformat_find_stream_info(m_formatCtx.get(), nullptr) < 0) {
        qWarning() << "avformat_find_stream_info failed";
        close();
        return false;
    }

    // Find best video & audio streams
    m_videoStreamIdx = av_find_best_stream(m_formatCtx.get(), AVMEDIA_TYPE_VIDEO,
                                            -1, -1, nullptr, 0);
    m_audioStreamIdx = av_find_best_stream(m_formatCtx.get(), AVMEDIA_TYPE_AUDIO,
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
    m_formatCtx.reset();
    m_videoStreamIdx = -1;
    m_audioStreamIdx = -1;
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
    return m_formatCtx.get();
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

    if (avcodec_open2(ctx, codec, nullptr) < 0) {
        avcodec_free_context(&ctx);
        return false;
    }

    *outCtx = ctx;
    return true;
}
