#include "RtxVsrClient.h"

#include <QFileInfo>
#include <cstring>

namespace {
const char *kDefaultBridgeDllName = "rtx_hdr_vsr_bridge.dll";
}

RtxVsrClient::RtxVsrClient() = default;

RtxVsrClient::~RtxVsrClient()
{
    shutdown();
    // Do NOT call unload() here.
    // The NGX runtime may still have background threads running after
    // rtx_vsr_destroy returns.  Unloading the DLL would rip the code
    // pages out from under those threads, causing a crash in
    // _beginthreadex/_endthreadex.  The OS will reclaim the DLL at
    // process exit.
}

bool RtxVsrClient::load(const QString &dllPath)
{
    if (isLoaded()) {
        return true;
    }

    clearError();

    const QString path = dllPath.isEmpty() ? QString::fromLatin1(kDefaultBridgeDllName) : dllPath;
    m_lib.setFileName(path);

    if (!m_lib.load()) {
        setError(QStringLiteral("Failed to load bridge DLL: %1").arg(m_lib.errorString()));
        return false;
    }

    m_fnCreate       = reinterpret_cast<FnCreate>(m_lib.resolve("rtx_vsr_create"));
    m_fnDestroy      = reinterpret_cast<FnDestroy>(m_lib.resolve("rtx_vsr_destroy"));
    m_fnSetQuality   = reinterpret_cast<FnSetQuality>(m_lib.resolve("rtx_vsr_set_quality"));
    m_fnReset        = reinterpret_cast<FnReset>(m_lib.resolve("rtx_vsr_reset"));
    m_fnProcessCpu   = reinterpret_cast<FnProcessCpu>(m_lib.resolve("rtx_vsr_process_cpu"));
    m_fnStatusString = reinterpret_cast<FnStatusString>(m_lib.resolve("rtx_vsr_status_string"));
    m_fnLastError    = reinterpret_cast<FnLastError>(m_lib.resolve("rtx_vsr_last_error"));

    const bool requiredOk = m_fnCreate && m_fnDestroy && m_fnProcessCpu;
    if (!requiredOk) {
        setError(QStringLiteral("Bridge DLL missing required symbols"));
        unload();
        return false;
    }

    return true;
}

void RtxVsrClient::unload()
{
    if (m_lib.isLoaded()) {
        m_lib.unload();
    }

    m_fnCreate       = nullptr;
    m_fnDestroy      = nullptr;
    m_fnSetQuality   = nullptr;
    m_fnReset        = nullptr;
    m_fnProcessCpu   = nullptr;
    m_fnStatusString = nullptr;
    m_fnLastError    = nullptr;
}

bool RtxVsrClient::isLoaded() const
{
    return m_lib.isLoaded();
}

bool RtxVsrClient::isInitialized() const
{
    return m_initialized && m_handle != nullptr;
}

bool RtxVsrClient::initialize(int inputWidth,
                               int inputHeight,
                               int outputWidth,
                               int outputHeight,
                               int quality,
                               int pixelFormat,
                               int adapterIndex)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    clearError();

    if (!isLoaded() && !load()) {
        return false;
    }

    if (inputWidth <= 0 || inputHeight <= 0 || outputWidth <= 0 || outputHeight <= 0) {
        setError(QStringLiteral("Invalid frame dimensions for VSR init"));
        return false;
    }

    // Build CreateParams matching rtx_hdr_vsr_bridge.h layout exactly
    CreateParams params{};
    params.inputWidth      = static_cast<unsigned int>(inputWidth);
    params.inputHeight     = static_cast<unsigned int>(inputHeight);
    params.outputWidth     = static_cast<unsigned int>(outputWidth);
    params.outputHeight    = static_cast<unsigned int>(outputHeight);
    params.pixelFormat     = static_cast<PixelFormat>(pixelFormat);
    params.initialQuality  = static_cast<Quality>(quality);
    params.enableHdr       = 0;
    params.hdrContrast     = 100;
    params.hdrSaturation   = 100;
    params.hdrMiddleGray   = 50;
    params.hdrMaxLuminance = 1000;
    params.adapterIndex    = adapterIndex;

    if (m_handle) {
        if (!m_fnReset) {
            setError(QStringLiteral("Bridge DLL does not expose rtx_vsr_reset"));
            return false;
        }

        const Status st = m_fnReset(m_handle, &params);
        if (st != Status::Ok) {
            const char *statusMsg = m_fnStatusString ? m_fnStatusString(st) : nullptr;
            const char *detailMsg = m_fnLastError ? m_fnLastError() : nullptr;
            QString err = QStringLiteral("rtx_vsr_reset failed: %1")
                              .arg(statusMsg ? QString::fromUtf8(statusMsg) : QStringLiteral("unknown"));
            if (detailMsg && *detailMsg)
                err += QStringLiteral(" | %1").arg(QString::fromUtf8(detailMsg));
            setError(err);
            return false;
        }

        m_inWidth   = inputWidth;
        m_inHeight  = inputHeight;
        m_outWidth  = outputWidth;
        m_outHeight = outputHeight;
        m_initialized = true;
        qDebug() << "FrameHandler: VSR reset successful:"
                 << inputWidth << "x" << inputHeight
                 << "->" << outputWidth << "x" << outputHeight;
        return true;
    }

    Handle handle = nullptr;
    const Status st = m_fnCreate(&params, &handle);
    if (st != Status::Ok || !handle) {
        const char *statusMsg = m_fnStatusString ? m_fnStatusString(st) : nullptr;
        const char *detailMsg = m_fnLastError ? m_fnLastError() : nullptr;
        QString err = QStringLiteral("rtx_vsr_create failed: %1")
                          .arg(statusMsg ? QString::fromUtf8(statusMsg) : QStringLiteral("unknown"));
        if (detailMsg && *detailMsg)
            err += QStringLiteral(" | %1").arg(QString::fromUtf8(detailMsg));
        setError(err);
        return false;
    }

    m_handle    = handle;
    m_inWidth   = inputWidth;
    m_inHeight  = inputHeight;
    m_outWidth  = outputWidth;
    m_outHeight = outputHeight;
    m_initialized = true;
    qDebug() << "FrameHandler: VSR initialized successfully:"
             << inputWidth << "x" << inputHeight
             << "->" << outputWidth << "x" << outputHeight;
    return true;
}

void RtxVsrClient::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_handle && m_fnDestroy) {
        m_fnDestroy(m_handle);
    }
    m_handle      = nullptr;
    m_initialized = false;
    m_inWidth     = 0;
    m_inHeight    = 0;
    m_outWidth    = 0;
    m_outHeight   = 0;
    qDebug() << "FrameHandler: VSR shutdown complete";
}

bool RtxVsrClient::setQuality(int quality)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    clearError();

    if (!isInitialized()) {
        setError(QStringLiteral("VSR handle is not initialized"));
        return false;
    }
    if (!m_fnSetQuality) {
        setError(QStringLiteral("Bridge DLL does not expose rtx_vsr_set_quality"));
        return false;
    }

    const Status st = m_fnSetQuality(m_handle, static_cast<Quality>(quality));
    if (st != Status::Ok) {
        const char *msg = m_fnStatusString ? m_fnStatusString(st) : nullptr;
        setError(QStringLiteral("rtx_vsr_set_quality failed: %1")
                     .arg(msg ? QString::fromUtf8(msg) : QStringLiteral("unknown")));
        return false;
    }

    return true;
}

bool RtxVsrClient::processFrameToBgra(const AVFrame *inFrame,
                                       uint8_t *outBgra,
                                       int outStride,
                                       int outWidth,
                                       int outHeight)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    clearError();

    if (!isInitialized()) {
        setError(QStringLiteral("VSR handle is not initialized"));
        return false;
    }
    if (!inFrame || !outBgra || outStride <= 0 || outWidth <= 0 || outHeight <= 0) {
        setError(QStringLiteral("Invalid frame/buffer arguments"));
        return false;
    }

    const AVPixelFormat ffFmt = static_cast<AVPixelFormat>(inFrame->format);
    if (!supportsInputFormat(ffFmt)) {
        setError(QStringLiteral("Unsupported input format for VSR client"));
        return false;
    }

    // The bridge DLL expects a contiguous NV12/P010 buffer:
    //   Y plane (height rows × inputPitch) immediately followed by
    //   UV plane (height/2 rows × inputPitch).
    // AVFrame may have separate data[0] (Y) and data[1] (UV) pointers.
    // If they are already contiguous we can pass data[0] directly;
    // otherwise we copy into a temp buffer.

    const int yStride  = inFrame->linesize[0];
    const int uvStride = inFrame->linesize[1];
    const int h        = inFrame->height;
    const int ySize    = yStride * h;
    const int uvSize   = uvStride * (h / 2);

    const uint8_t *inputPtr = nullptr;
    unsigned int inputPitch  = static_cast<unsigned int>(yStride);
    std::vector<uint8_t> tmpBuf;

    const bool contiguous = (inFrame->data[1] == inFrame->data[0] + ySize)
                            && (yStride == uvStride);

    if (contiguous) {
        inputPtr = inFrame->data[0];
    } else {
        // Pack Y + UV into a contiguous buffer
        tmpBuf.resize(static_cast<size_t>(ySize + uvSize));
        std::memcpy(tmpBuf.data(), inFrame->data[0], ySize);
        std::memcpy(tmpBuf.data() + ySize, inFrame->data[1], uvSize);
        inputPtr  = tmpBuf.data();
        inputPitch = static_cast<unsigned int>(yStride);
    }

    const Status st = m_fnProcessCpu(m_handle,
                                      inputPtr, inputPitch,
                                      outBgra, static_cast<unsigned int>(outStride));
    if (st != Status::Ok) {
        const char *statusMsg = m_fnStatusString ? m_fnStatusString(st) : nullptr;
        const char *detailMsg = m_fnLastError ? m_fnLastError() : nullptr;

        QString msg = QStringLiteral("rtx_vsr_process_cpu failed: %1")
                          .arg(statusMsg ? QString::fromUtf8(statusMsg) : QStringLiteral("unknown"));
        if (detailMsg && *detailMsg) {
            msg += QStringLiteral(" (%1)").arg(QString::fromUtf8(detailMsg));
        }
        setError(msg);
        return false;
    }

    return true;
}

QString RtxVsrClient::lastError() const
{
    return m_lastError;
}

bool RtxVsrClient::supportsInputFormat(AVPixelFormat fmt)
{
    return fmt == AV_PIX_FMT_NV12 || fmt == AV_PIX_FMT_P010LE;
}

void RtxVsrClient::clearError()
{
    m_lastError.clear();
}

void RtxVsrClient::setError(const QString &msg)
{
    m_lastError = msg;
}
