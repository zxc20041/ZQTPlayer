#pragma once

#include <cstdint>

/// @brief Playback states for the media player engine.
enum class AVPlayerStatus : uint8_t {
    Stopped,        ///< No file loaded or playback terminated
    Loading,        ///< File opened, threads being created
    Playing,        ///< Active playback
    Paused,         ///< Playback paused, threads alive but waiting
    EndOfFile,      ///< Demux reached EOF, decode threads still draining
    PlaybackDone,   ///< All decode threads finished, ready to stop
};

/// @brief Video rendering backend selection.
enum class VideoRenderMode : uint8_t {
    QVideoSink,     ///< Software path: sws_scale → QVideoFrame → QVideoSink
    OpenGLTexture,  ///< GPU path: upload YUV planes as GL textures, shader does conversion
};

/// @brief Decoder backend selection (reserved for future HW decode integration).
enum class VideoDecodeBackend : uint8_t {
    Software,       ///< Always use software decoding
    AutoHardware,   ///< Try hardware decode first, fallback to software
    D3D11VA,        ///< Windows D3D11VA hardware decode
    VAAPI,          ///< Linux VAAPI hardware decode
    VideoToolbox,   ///< macOS VideoToolbox hardware decode
};

/// @brief Scaling / pixel-format conversion filter quality.
///        Maps directly to the SWS_* flags used by sws_getContext().
enum class SwsFilterMode : uint8_t {
    Point,          ///< Nearest-neighbour – fastest, lowest quality
    FastBilinear,   ///< Fast bilinear – good for real-time preview
    Bilinear,       ///< Bilinear – balanced default
    Bicubic,        ///< Bicubic – sharper, moderate cost
    Lanczos,        ///< Lanczos – highest quality, highest cost
};
