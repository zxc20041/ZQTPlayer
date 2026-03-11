/*
 * rtx_hdr_vsr_bridge.h
 *
 * Pure-C ABI bridge for NVIDIA RTX Video SDK (VSR + TrueHDR).
 * Compiled with MSVC as a DLL, loaded by MinGW-built player via QLibrary.
 *
 * All exported symbols use extern "C" __declspec(dllexport) with __cdecl
 * calling convention so that any C-compatible loader can resolve them.
 */

#ifndef RTX_HDR_VSR_BRIDGE_H
#define RTX_HDR_VSR_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RTX_BRIDGE_EXPORTS
#define RTX_BRIDGE_API __declspec(dllexport)
#else
#define RTX_BRIDGE_API __declspec(dllimport)
#endif

/* ------------------------------------------------------------------ */
/*  Status codes                                                       */
/* ------------------------------------------------------------------ */
typedef enum RtxBridgeStatus
{
    RTX_BRIDGE_OK                   =  0,
    RTX_BRIDGE_ERR_INVALID_ARG      = -1,
    RTX_BRIDGE_ERR_D3D11_DEVICE     = -2,
    RTX_BRIDGE_ERR_NGX_INIT         = -3,
    RTX_BRIDGE_ERR_NGX_PARAMS       = -4,
    RTX_BRIDGE_ERR_VSR_UNAVAILABLE  = -5,
    RTX_BRIDGE_ERR_HDR_UNAVAILABLE  = -6,
    RTX_BRIDGE_ERR_CREATE_FEATURE   = -7,
    RTX_BRIDGE_ERR_TEXTURE          = -8,
    RTX_BRIDGE_ERR_EVALUATE         = -9,
    RTX_BRIDGE_ERR_MAP              = -10,
    RTX_BRIDGE_ERR_NOT_INITIALIZED  = -11,
    RTX_BRIDGE_ERR_UNKNOWN          = -99,
} RtxBridgeStatus;

/* ------------------------------------------------------------------ */
/*  VSR quality levels  (mirrors NVSDK_NGX_VSR_QualityLevel)           */
/* ------------------------------------------------------------------ */
typedef enum RtxVsrQuality
{
    RTX_VSR_QUALITY_BICUBIC = 0,
    RTX_VSR_QUALITY_LOW     = 1,
    RTX_VSR_QUALITY_MEDIUM  = 2,
    RTX_VSR_QUALITY_HIGH    = 3,
    RTX_VSR_QUALITY_ULTRA   = 4,
} RtxVsrQuality;

/* ------------------------------------------------------------------ */
/*  Pixel format of the CPU-side input frame                           */
/* ------------------------------------------------------------------ */
typedef enum RtxPixelFormat
{
    RTX_PIXEL_FMT_NV12  = 0,   /* 8-bit  4:2:0  */
    RTX_PIXEL_FMT_P010  = 1,   /* 10-bit 4:2:0  */
    RTX_PIXEL_FMT_BGRA  = 2,   /* 8-bit  BGRA   */
} RtxPixelFormat;

/* ------------------------------------------------------------------ */
/*  Opaque handle (pointer to internal struct)                         */
/* ------------------------------------------------------------------ */
typedef struct RtxBridgeContext RtxBridgeContext;

/* ------------------------------------------------------------------ */
/*  Creation parameters                                                */
/* ------------------------------------------------------------------ */
typedef struct RtxBridgeCreateParams
{
    unsigned int    inputWidth;
    unsigned int    inputHeight;
    unsigned int    outputWidth;
    unsigned int    outputHeight;
    RtxPixelFormat  pixelFormat;
    RtxVsrQuality   initialQuality;
    int             enableHdr;          /* non-zero = also create TrueHDR feature */
    unsigned int    hdrContrast;        /* 0-200, default 100 */
    unsigned int    hdrSaturation;      /* 0-200, default 100 */
    unsigned int    hdrMiddleGray;      /* 10-100, default 50 */
    unsigned int    hdrMaxLuminance;    /* 400-2000, default 1000 */
    int             adapterIndex;       /* -1 = default adapter */
} RtxBridgeCreateParams;

/* ------------------------------------------------------------------ */
/*  Exported functions                                                 */
/* ------------------------------------------------------------------ */

/*
 * rtx_vsr_create
 *   Create DX11 device + NGX VSR (and optionally TrueHDR) pipeline.
 *   Returns RTX_BRIDGE_OK on success; *ppCtx receives the opaque handle.
 */
RTX_BRIDGE_API RtxBridgeStatus __cdecl rtx_vsr_create(
    const RtxBridgeCreateParams* params,
    RtxBridgeContext**           ppCtx);

/*
 * rtx_vsr_destroy
 *   Release all GPU resources and free the handle.
 */
RTX_BRIDGE_API void __cdecl rtx_vsr_destroy(
    RtxBridgeContext* ctx);

/*
 * rtx_vsr_process_cpu
 *   Upload a CPU-side frame (NV12 / P010 / BGRA) ¡ú run VSR (+ optional HDR)
 *   ¡ú download the BGRA result back to CPU.
 *
 *   inputData      : pointer to pixel data (NV12: Y+UV packed, P010: same layout 16-bit)
 *   inputPitch     : row pitch in bytes of the input buffer (0 = tightly packed)
 *   outputBGRA     : caller-allocated buffer, outputWidth * outputHeight * 4 bytes
 *   outputPitch    : row pitch of output buffer (0 = tightly packed = outWidth*4)
 */
RTX_BRIDGE_API RtxBridgeStatus __cdecl rtx_vsr_process_cpu(
    RtxBridgeContext*   ctx,
    const void*         inputData,
    unsigned int        inputPitch,
    void*               outputBGRA,
    unsigned int        outputPitch);

/*
 * rtx_vsr_set_quality
 *   Change VSR quality level at runtime (takes effect on next process call).
 */
RTX_BRIDGE_API RtxBridgeStatus __cdecl rtx_vsr_set_quality(
    RtxBridgeContext*   ctx,
    RtxVsrQuality       quality);

/*
 * rtx_vsr_reset
 *   Re-create the pipeline for a new resolution without destroying the handle.
 */
RTX_BRIDGE_API RtxBridgeStatus __cdecl rtx_vsr_reset(
    RtxBridgeContext*               ctx,
    const RtxBridgeCreateParams*    params);

/*
 * rtx_vsr_status_string
 *   Convert a RtxBridgeStatus to a human-readable string.
 */
RTX_BRIDGE_API const char* __cdecl rtx_vsr_status_string(
    RtxBridgeStatus status);

/*
 * rtx_vsr_last_error
 *   Return the last detailed error message (thread-local, null-terminated).
 */
RTX_BRIDGE_API const char* __cdecl rtx_vsr_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* RTX_HDR_VSR_BRIDGE_H */
