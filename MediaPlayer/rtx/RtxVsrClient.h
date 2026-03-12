#pragma once

#include <QString>
#include <QLibrary>
#include <mutex>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

// Thin runtime wrapper for rtx_hdr_vsr_bridge.dll (C ABI).
// This class keeps all SDK symbols behind dynamic loading so the main app
// can run even when the bridge DLL is missing.
//
// ABI types mirror the definitions in rtx_hdr_vsr_bridge.h exactly.
class RtxVsrClient
{
public:
    RtxVsrClient();
    ~RtxVsrClient();

    RtxVsrClient(const RtxVsrClient &) = delete;
    RtxVsrClient &operator=(const RtxVsrClient &) = delete;

    bool load(const QString &dllPath = QString());
    void unload();

    bool isLoaded() const;
    bool isInitialized() const;

    int inWidth()  const { return m_inWidth; }
    int inHeight() const { return m_inHeight; }
    int outWidth()  const { return m_outWidth; }
    int outHeight() const { return m_outHeight; }

    bool initialize(int inputWidth,
                    int inputHeight,
                    int outputWidth,
                    int outputHeight,
                    int quality,
                    int pixelFormat = 0,
                    int adapterIndex = -1);

    void shutdown();

    bool setQuality(int quality);

    /// Process a decoded AVFrame (NV12 or P010) through VSR and write BGRA
    /// output into the caller-supplied buffer.
    bool processFrameToBgra(const AVFrame *inFrame,
                            uint8_t *outBgra,
                            int outStride,
                            int outWidth,
                            int outHeight);

    QString lastError() const;

    static bool supportsInputFormat(AVPixelFormat fmt);

private:
    void clearError();
    void setError(const QString &msg);

    // ── C ABI mirror types (must match rtx_hdr_vsr_bridge.h exactly) ──

    enum class Status : int {
        Ok                  =   0,
        ErrInvalidArg       =  -1,
        ErrD3d11Device      =  -2,
        ErrNgxInit          =  -3,
        ErrNgxParams        =  -4,
        ErrVsrUnavailable   =  -5,
        ErrHdrUnavailable   =  -6,
        ErrCreateFeature    =  -7,
        ErrTexture          =  -8,
        ErrEvaluate         =  -9,
        ErrMap              = -10,
        ErrNotInitialized   = -11,
        ErrUnknown          = -99,
    };

    enum class PixelFormat : int {
        Nv12 = 0,
        P010 = 1,
        Bgra = 2,
    };

    enum class Quality : int {
        Bicubic = 0,
        Low     = 1,
        Medium  = 2,
        High    = 3,
        Ultra   = 4,
    };

    struct CreateParams {
        unsigned int   inputWidth;
        unsigned int   inputHeight;
        unsigned int   outputWidth;
        unsigned int   outputHeight;
        PixelFormat    pixelFormat;
        Quality        initialQuality;
        int            enableHdr;
        unsigned int   hdrContrast;
        unsigned int   hdrSaturation;
        unsigned int   hdrMiddleGray;
        unsigned int   hdrMaxLuminance;
        int            adapterIndex;
    };

    using Handle = void *;

    // Function pointer types matching the DLL exports
    using FnCreate       = Status (__cdecl *)(const CreateParams *params, Handle *ppCtx);
    using FnDestroy      = void   (__cdecl *)(Handle ctx);
    using FnProcessCpu   = Status (__cdecl *)(Handle ctx,
                                              const void *inputData, unsigned int inputPitch,
                                              void *outputBGRA, unsigned int outputPitch);
    using FnSetQuality   = Status (__cdecl *)(Handle ctx, Quality quality);
    using FnReset        = Status (__cdecl *)(Handle ctx, const CreateParams *params);
    using FnStatusString = const char *(__cdecl *)(Status status);
    using FnLastError    = const char *(__cdecl *)(void);

private:
    QLibrary m_lib;

    FnCreate       m_fnCreate       = nullptr;
    FnDestroy      m_fnDestroy      = nullptr;
    FnSetQuality   m_fnSetQuality   = nullptr;
    FnReset        m_fnReset        = nullptr;
    FnProcessCpu   m_fnProcessCpu   = nullptr;
    FnStatusString m_fnStatusString = nullptr;
    FnLastError    m_fnLastError    = nullptr;

    Handle m_handle      = nullptr;
    bool   m_initialized = false;
    QString m_lastError;

    std::mutex m_mutex;   // guards m_handle access across threads

    // Remembered dimensions for the current init
    int m_inWidth   = 0;
    int m_inHeight  = 0;
    int m_outWidth  = 0;
    int m_outHeight = 0;
};
