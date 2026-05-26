/**********************************************************************************************
*
*   raymapvid v0.2.0 - Video decoding library for RayMap (FFmpeg-based)
*
*   CHANGES v0.2.0:
*       - Fix VP9 alpha channel decoding (WebM BlockAdditions / yuva420p)
*       - Removed broken dual-stream alpha approach
*       - Runtime pixel format detection (adaptive sws_scale)
*       - Removed manual alpha merge (sws handles YUVA->RGBA natively)
*       - Fixed texture format mismatch when hasAlpha is wrong at load time
*
*   VP9 ALPHA NOTES:
*       WebM VP9 with alpha_mode=1 encodes alpha as a Matroska BlockAddition.
*       FFmpeg's matroska demuxer + libvpx-vp9 decoder handles this automatically:
*         - If libvpx supports it: frame->format = AV_PIX_FMT_YUVA420P, data[3] = alpha plane
*         - Fallback: frame->format = AV_PIX_FMT_YUV420P, no alpha plane (transparent = 255)
*       We detect the real pixel format on the FIRST decoded frame, not at codec open time.
*       sws_scale from YUVA420P to RGBA fills the alpha channel correctly.
*
*   DEPENDENCIES:
*       - raylib 5.0+
*       - FFmpeg 4.4+ (libavcodec, libavformat, libavutil, libswscale)
*
*   LICENSE: zlib/libpng
*   Copyright (c) 2026 grerfou
*
**********************************************************************************************/

#ifndef RAYMAPVID_H
#define RAYMAPVID_H

#include "raylib.h"
#include <stdbool.h>

#ifndef RMVAPI
    #define RMVAPI extern
#endif

//--------------------------------------------------------------------------------------------
// Opaque handle
//--------------------------------------------------------------------------------------------
typedef struct RMV_Video RMV_Video;

//--------------------------------------------------------------------------------------------
// Enums
//--------------------------------------------------------------------------------------------

typedef enum {
    RMV_HWACCEL_NONE = 0,
    RMV_HWACCEL_AUTO,
    RMV_HWACCEL_NVDEC,
    RMV_HWACCEL_VAAPI,
    RMV_HWACCEL_VIDEOTOOLBOX,
    RMV_HWACCEL_D3D11,
    RMV_HWACCEL_DXVA2
} RMV_HWAccelType;

typedef enum {
    RMV_STATE_STOPPED = 0,
    RMV_STATE_PLAYING,
    RMV_STATE_PAUSED,
    RMV_STATE_ERROR
} RMV_PlaybackState;

//--------------------------------------------------------------------------------------------
// Public Structures
//--------------------------------------------------------------------------------------------

typedef struct {
    int width;
    int height;
    float duration;
    float fps;
    const char *codec;
    const char *format;
    bool hasAudio;
    bool hasAlpha;
    RMV_HWAccelType hwaccel;
} RMV_VideoInfo;

//--------------------------------------------------------------------------------------------
// API
//--------------------------------------------------------------------------------------------

RMVAPI RMV_Video *RMV_LoadVideo(const char *filepath);
RMVAPI void       RMV_UnloadVideo(RMV_Video *video);

RMVAPI RMV_VideoInfo      RMV_GetVideoInfo(const RMV_Video *video);
RMVAPI Texture2D          RMV_GetVideoTexture(const RMV_Video *video);

RMVAPI void RMV_UpdateVideo(RMV_Video *video, float deltaTime);
RMVAPI void RMV_PlayVideo(RMV_Video *video);
RMVAPI void RMV_PauseVideo(RMV_Video *video);
RMVAPI void RMV_StopVideo(RMV_Video *video);
RMVAPI void RMV_ToggleVideoPause(RMV_Video *video);

RMVAPI RMV_PlaybackState RMV_GetVideoState(const RMV_Video *video);
RMVAPI bool              RMV_IsVideoPlaying(const RMV_Video *video);
RMVAPI bool              RMV_IsVideoLoaded(const RMV_Video *video);

RMVAPI void RMV_SetVideoLoop(RMV_Video *video, bool loop);
RMVAPI void RMV_SetVideoSpeed(RMV_Video *video, float speed);

#endif // RAYMAPVID_H

/***********************************************************************************
*
*   RAYMAPVID IMPLEMENTATION
*
************************************************************************************/

#if defined(RAYMAP_IMPLEMENTATION) && !defined(RAYMAPVID_IMPLEMENTATION)
    #define RAYMAPVID_IMPLEMENTATION
#endif

#if defined(RAYMAPVID_IMPLEMENTATION)

#undef RMVAPI
#define RMVAPI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

//--------------------------------------------------------------------------------------------
// Memory macros
//--------------------------------------------------------------------------------------------
#ifndef RMVMALLOC
    #define RMVMALLOC(size)    malloc(size)
#endif
#ifndef RMVCALLOC
    #define RMVCALLOC(n, size) calloc(n, size)
#endif
#ifndef RMVFREE
    #define RMVFREE(ptr)       free(ptr)
#endif

//--------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------
#define RMV_MIN_DIMENSION 16
#define RMV_MAX_DIMENSION 16384
#define RMV_SWS_FLAGS     (SWS_BILINEAR | SWS_FULL_CHR_H_INT)

//--------------------------------------------------------------------------------------------
// Internal structure
//--------------------------------------------------------------------------------------------

struct RMV_Video {
    // FFmpeg
    AVFormatContext *formatCtx;
    AVCodecContext  *codecCtx;
    const AVCodec   *codec;
    AVFrame         *frame;
    AVFrame         *frameRGB;
    AVPacket        *packet;
    struct SwsContext *swsCtx;

    int videoStreamIndex;

    // Raylib
    Texture2D  texture;
    uint8_t   *rgbBuffer;
    int        rgbBufferSize;
    bool       textureCreated;

    // Metadata
    int         width;
    int         height;
    float       duration;
    float       fps;
    const char *codecName;
    const char *formatName;
    bool        hasAudio;

    // Alpha — discovered at first frame decode, NOT at load time
    // VP9 alpha_mode=1 in WebM: alpha arrives via Matroska BlockAdditions.
    // libvpx-vp9 exposes this as AV_PIX_FMT_YUVA420P on the decoded frame.
    // We cannot know this at avcodec_open2 time (pix_fmt reports yuv420p).
    bool hasAlpha;              // hint from container metadata
    bool alphaConfirmed;        // true once we've seen the real frame->format
    enum AVPixelFormat dstFmt;  // AV_PIX_FMT_RGBA or AV_PIX_FMT_RGB24, set after first frame

    // Playback
    RMV_PlaybackState state;
    float  currentTime;
    bool   loop;
    float  frameAccumulator;
    float  speed;
    bool   isLoaded;
};

//--------------------------------------------------------------------------------------------
// Internal helpers
//--------------------------------------------------------------------------------------------

static const char *rmv_FFmpegError(int code) {
    static char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(code, buf, sizeof(buf));
    return buf;
}

static bool rmv_IsAlphaPixFmt(enum AVPixelFormat fmt) {
    switch (fmt) {
        case AV_PIX_FMT_YUVA420P:
        case AV_PIX_FMT_YUVA422P:
        case AV_PIX_FMT_YUVA444P:
        case AV_PIX_FMT_RGBA:
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_ARGB:
        case AV_PIX_FMT_ABGR:
            return true;
        default:
            return false;
    }
}

// Allocate or reallocate the RGB/RGBA buffer and swsCtx for the given source pixel format.
// Called once after the first frame is decoded (when we know the real pix_fmt).
// Returns true on success.
static bool rmv_ReallocForFormat(RMV_Video *video, enum AVPixelFormat srcFmt) {

    // Determine destination format
    enum AVPixelFormat newDst = rmv_IsAlphaPixFmt(srcFmt)
        ? AV_PIX_FMT_RGBA
        : AV_PIX_FMT_RGB24;

    bool fmtChanged = (newDst != video->dstFmt);
    video->dstFmt  = newDst;
    video->hasAlpha = (newDst == AV_PIX_FMT_RGBA);

    TraceLog(LOG_INFO, "RAYMAPVID: Frame pix_fmt=%s -> dst=%s hasAlpha=%d",
             av_get_pix_fmt_name(srcFmt),
             av_get_pix_fmt_name(newDst),
             video->hasAlpha);

    if (!fmtChanged && video->swsCtx && video->rgbBuffer) {
        return true; // Nothing to do
    }

    // Free old swsCtx
    if (video->swsCtx) {
        sws_freeContext(video->swsCtx);
        video->swsCtx = NULL;
    }

    // Create new swsCtx
    video->swsCtx = sws_getContext(
        video->width, video->height, srcFmt,
        video->width, video->height, newDst,
        RMV_SWS_FLAGS, NULL, NULL, NULL
    );
    if (!video->swsCtx) {
        TraceLog(LOG_ERROR, "RAYMAPVID: sws_getContext failed for src=%s dst=%s",
                 av_get_pix_fmt_name(srcFmt), av_get_pix_fmt_name(newDst));
        return false;
    }

    // Reallocate buffer
    int newSize = av_image_get_buffer_size(newDst, video->width, video->height, 1);
    if (newSize <= 0) {
        TraceLog(LOG_ERROR, "RAYMAPVID: Invalid buffer size %d", newSize);
        return false;
    }

    if (video->rgbBuffer) {
        av_free(video->rgbBuffer);
        video->rgbBuffer = NULL;
    }

    video->rgbBuffer = (uint8_t *)av_malloc(newSize);
    if (!video->rgbBuffer) {
        TraceLog(LOG_ERROR, "RAYMAPVID: Buffer alloc failed (%d bytes)", newSize);
        return false;
    }
    video->rgbBufferSize = newSize;

    // Wire buffer into frameRGB
    av_image_fill_arrays(
        video->frameRGB->data,
        video->frameRGB->linesize,
        video->rgbBuffer,
        newDst,
        video->width, video->height, 1
    );

    TraceLog(LOG_INFO, "RAYMAPVID: Buffer reallocated (%d bytes, %s)",
             newSize, av_get_pix_fmt_name(newDst));
    return true;
}

// Create or recreate the raylib texture with the current format.
// Destroys the old texture if the format changed.
static bool rmv_CreateOrUpdateTexture(RMV_Video *video) {
    if (!video || !video->rgbBuffer) return false;

    PixelFormat rlFmt = (video->dstFmt == AV_PIX_FMT_RGBA)
        ? PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
        : PIXELFORMAT_UNCOMPRESSED_R8G8B8;

    // If texture already exists with correct format, keep it
    if (video->textureCreated) {
        // Check format consistency via expected bytes per pixel
        int bpp = (video->dstFmt == AV_PIX_FMT_RGBA) ? 4 : 3;
        int expectedSize = video->width * video->height * bpp;
        if (expectedSize == video->rgbBufferSize) {
            return true; // Format unchanged, texture still valid
        }
        // Format changed (e.g. we discovered alpha after first frame)
        UnloadTexture(video->texture);
        video->texture.id = 0;
        video->textureCreated = false;
        TraceLog(LOG_INFO, "RAYMAPVID: Texture format changed, recreating");
    }

    Image img = {
        .data    = video->rgbBuffer,
        .width   = video->width,
        .height  = video->height,
        .mipmaps = 1,
        .format  = rlFmt
    };

    video->texture = LoadTextureFromImage(img);
    if (video->texture.id == 0) {
        TraceLog(LOG_ERROR, "RAYMAPVID: LoadTextureFromImage failed");
        return false;
    }

    video->textureCreated = true;
    TraceLog(LOG_INFO, "RAYMAPVID: Texture created %dx%d fmt=%s",
             video->width, video->height,
             (rlFmt == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) ? "RGBA" : "RGB");
    return true;
}

static bool rmv_ValidateVideo(const RMV_Video *video, const char *fn) {
    if (!video) {
        TraceLog(LOG_WARNING, "RAYMAPVID: %s() NULL video", fn);
        return false;
    }
    if (!video->isLoaded) {
        TraceLog(LOG_WARNING, "RAYMAPVID: %s() unloaded video", fn);
        return false;
    }
    return true;
}

static void rmv_Cleanup(RMV_Video *video) {
    if (!video) return;
    video->isLoaded = false;

    if (video->rgbBuffer)  { av_free(video->rgbBuffer);           video->rgbBuffer  = NULL; }
    if (video->texture.id) { UnloadTexture(video->texture);       video->texture.id = 0;    }
    if (video->swsCtx)     { sws_freeContext(video->swsCtx);      video->swsCtx     = NULL; }
    if (video->frameRGB)   { av_frame_free(&video->frameRGB);                               }
    if (video->frame)      { av_frame_free(&video->frame);                                  }
    if (video->packet)     { av_packet_free(&video->packet);                                }
    if (video->codecCtx)   { avcodec_free_context(&video->codecCtx);                        }
    if (video->formatCtx)  { avformat_close_input(&video->formatCtx);                      }
}

//--------------------------------------------------------------------------------------------
// Public API — Load / Unload
//--------------------------------------------------------------------------------------------

RMVAPI RMV_Video *RMV_LoadVideo(const char *filepath) {
    if (!filepath) {
        TraceLog(LOG_ERROR, "RAYMAPVID: NULL filepath");
        return NULL;
    }

    RMV_Video *video = (RMV_Video *)RMVCALLOC(1, sizeof(RMV_Video));
    if (!video) {
        TraceLog(LOG_ERROR, "RAYMAPVID: Alloc failed");
        return NULL;
    }

    // Open container
    if (avformat_open_input(&video->formatCtx, filepath, NULL, NULL) != 0) {
        TraceLog(LOG_ERROR, "RAYMAPVID: Cannot open '%s'", filepath);
        RMVFREE(video); return NULL;
    }
    if (avformat_find_stream_info(video->formatCtx, NULL) < 0) {
        TraceLog(LOG_ERROR, "RAYMAPVID: find_stream_info failed");
        rmv_Cleanup(video); RMVFREE(video); return NULL;
    }

    // Find first video stream
    video->videoStreamIndex = -1;
    for (unsigned i = 0; i < video->formatCtx->nb_streams; i++) {
        if (video->formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video->videoStreamIndex = (int)i;
            break;
        }
    }
    if (video->videoStreamIndex < 0) {
        TraceLog(LOG_ERROR, "RAYMAPVID: No video stream");
        rmv_Cleanup(video); RMVFREE(video); return NULL;
    }

    AVCodecParameters *par = video->formatCtx->streams[video->videoStreamIndex]->codecpar;

    // Find and open decoder
    video->codec = avcodec_find_decoder(par->codec_id);
    if (!video->codec) {
        TraceLog(LOG_ERROR, "RAYMAPVID: No decoder for codec_id=%d", par->codec_id);
        rmv_Cleanup(video); RMVFREE(video); return NULL;
    }
    video->codecCtx = avcodec_alloc_context3(video->codec);
    if (!video->codecCtx) {
        TraceLog(LOG_ERROR, "RAYMAPVID: avcodec_alloc_context3 failed");
        rmv_Cleanup(video); RMVFREE(video); return NULL;
    }
    if (avcodec_parameters_to_context(video->codecCtx, par) < 0) {
        TraceLog(LOG_ERROR, "RAYMAPVID: parameters_to_context failed");
        rmv_Cleanup(video); RMVFREE(video); return NULL;
    }
    if (avcodec_open2(video->codecCtx, video->codec, NULL) < 0) {
        TraceLog(LOG_ERROR, "RAYMAPVID: avcodec_open2 failed");
        rmv_Cleanup(video); RMVFREE(video); return NULL;
    }

    // Dimensions
    video->width  = video->codecCtx->width;
    video->height = video->codecCtx->height;

    if (video->width  < RMV_MIN_DIMENSION || video->width  > RMV_MAX_DIMENSION ||
        video->height < RMV_MIN_DIMENSION || video->height > RMV_MAX_DIMENSION) {
        TraceLog(LOG_ERROR, "RAYMAPVID: Invalid dimensions %dx%d", video->width, video->height);
        rmv_Cleanup(video); RMVFREE(video); return NULL;
    }

    // Metadata
    AVRational fr = video->formatCtx->streams[video->videoStreamIndex]->r_frame_rate;
    video->fps = (fr.den > 0) ? (float)av_q2d(fr) : 25.0f;

    int64_t dur = video->formatCtx->duration;
    video->duration = (dur != AV_NOPTS_VALUE) ? (float)dur / AV_TIME_BASE : 0.0f;

    video->codecName  = avcodec_get_name(par->codec_id);
    video->formatName = video->formatCtx->iformat->name;

    // Audio detection
    for (unsigned i = 0; i < video->formatCtx->nb_streams; i++) {
        if (video->formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            video->hasAudio = true; break;
        }
    }

    // Alpha hint from container metadata (VP9 WebM)
    // IMPORTANT: This is just a hint. The REAL determination happens on the first
    // decoded frame by inspecting frame->format. Do NOT allocate RGBA buffers here.
    AVDictionaryEntry *alphaTag = av_dict_get(
        video->formatCtx->streams[video->videoStreamIndex]->metadata,
        "alpha_mode", NULL, 0);
    video->hasAlpha     = (alphaTag && strcmp(alphaTag->value, "1") == 0);
    video->alphaConfirmed = false;

    // Initial dst format assumption (will be corrected at first frame)
    video->dstFmt = video->hasAlpha ? AV_PIX_FMT_RGBA : AV_PIX_FMT_RGB24;

    TraceLog(LOG_INFO, "RAYMAPVID: Opened '%s' [%dx%d %.2ffps codec=%s alpha_hint=%d]",
             filepath, video->width, video->height, video->fps,
             video->codecName, video->hasAlpha);

    // Allocate frames and packet
    video->frame    = av_frame_alloc();
    video->frameRGB = av_frame_alloc();
    video->packet   = av_packet_alloc();
    if (!video->frame || !video->frameRGB || !video->packet) {
        TraceLog(LOG_ERROR, "RAYMAPVID: av_frame/packet alloc failed");
        rmv_Cleanup(video); RMVFREE(video); return NULL;
    }

    // Allocate initial buffer based on hint (may be reallocated after first frame)
    enum AVPixelFormat initPix = video->codecCtx->pix_fmt;
    if (initPix == AV_PIX_FMT_NONE) {
        // Codec didn't set pix_fmt at open time (common for VP9) — use hint
        initPix = video->hasAlpha ? AV_PIX_FMT_YUVA420P : AV_PIX_FMT_YUV420P;
    }

    if (!rmv_ReallocForFormat(video, initPix)) {
        rmv_Cleanup(video); RMVFREE(video); return NULL;
    }

    // State
    video->state           = RMV_STATE_STOPPED;
    video->currentTime     = 0.0f;
    video->loop            = false;
    video->frameAccumulator = 0.0f;
    video->speed           = 1.0f;
    video->isLoaded        = true;

    TraceLog(LOG_INFO, "RAYMAPVID: Loaded OK (buffer=%d bytes, dstFmt=%s)",
             video->rgbBufferSize, av_get_pix_fmt_name(video->dstFmt));
    return video;
}

RMVAPI void RMV_UnloadVideo(RMV_Video *video) {
    if (!video) return;
    rmv_Cleanup(video);
    RMVFREE(video);
    TraceLog(LOG_INFO, "RAYMAPVID: Video unloaded");
}

//--------------------------------------------------------------------------------------------
// Public API — Metadata / Texture
//--------------------------------------------------------------------------------------------

RMVAPI RMV_VideoInfo RMV_GetVideoInfo(const RMV_Video *video) {
    if (!rmv_ValidateVideo(video, "RMV_GetVideoInfo")) return (RMV_VideoInfo){0};
    return (RMV_VideoInfo){
        .width    = video->width,
        .height   = video->height,
        .duration = video->duration,
        .fps      = video->fps,
        .codec    = video->codecName,
        .format   = video->formatName,
        .hasAudio = video->hasAudio,
        .hasAlpha = video->hasAlpha,
        .hwaccel  = RMV_HWACCEL_NONE
    };
}

RMVAPI Texture2D RMV_GetVideoTexture(const RMV_Video *video) {
    if (!rmv_ValidateVideo(video, "RMV_GetVideoTexture")) return (Texture2D){0};

    if (!video->textureCreated) {
        RMV_Video *v = (RMV_Video *)video;
        if (!rmv_CreateOrUpdateTexture(v)) return (Texture2D){0};
    }
    return video->texture;
}

//--------------------------------------------------------------------------------------------
// Public API — Update (frame decode)
//--------------------------------------------------------------------------------------------

RMVAPI void RMV_UpdateVideo(RMV_Video *video, float deltaTime) {
    if (!rmv_ValidateVideo(video, "RMV_UpdateVideo")) return;
    if (video->state != RMV_STATE_PLAYING) return;

    // Lazy texture creation
    if (!video->textureCreated) {
        if (!rmv_CreateOrUpdateTexture(video)) {
            video->state = RMV_STATE_ERROR;
            return;
        }
    }

    video->frameAccumulator += deltaTime * video->speed;
    video->currentTime      += deltaTime;

    float frameTime = (video->fps > 0.0f) ? (1.0f / video->fps) : (1.0f / 25.0f);

    while (video->frameAccumulator >= frameTime) {
        video->frameAccumulator -= frameTime;

        bool frameDecoded = false;

        while (!frameDecoded) {
            int ret = av_read_frame(video->formatCtx, video->packet);

            if (ret < 0) {
                if (ret == AVERROR_EOF) {
                    TraceLog(LOG_INFO, "RAYMAPVID: EOF");
                    if (video->loop) {
                        av_seek_frame(video->formatCtx, video->videoStreamIndex,
                                      0, AVSEEK_FLAG_BACKWARD);
                        avcodec_flush_buffers(video->codecCtx);
                        video->currentTime      = 0.0f;
                        video->frameAccumulator = 0.0f;
                        continue;
                    } else {
                        video->state            = RMV_STATE_STOPPED;
                        video->currentTime      = 0.0f;
                        video->frameAccumulator = 0.0f;
                        return;
                    }
                }
                TraceLog(LOG_ERROR, "RAYMAPVID: av_read_frame: %s", rmv_FFmpegError(ret));
                return;
            }

            if (video->packet->stream_index != video->videoStreamIndex) {
                av_packet_unref(video->packet);
                continue;
            }

            ret = avcodec_send_packet(video->codecCtx, video->packet);
            av_packet_unref(video->packet);

            if (ret < 0) {
                TraceLog(LOG_ERROR, "RAYMAPVID: send_packet: %s", rmv_FFmpegError(ret));
                return;
            }

            ret = avcodec_receive_frame(video->codecCtx, video->frame);

            if (ret == 0) {
                // ---------------------------------------------------------------
                // FIRST FRAME: confirm actual pixel format and reallocate if needed
                // ---------------------------------------------------------------
                if (!video->alphaConfirmed) {
                    video->alphaConfirmed = true;

                    if (video->frame->format != (int)video->dstFmt &&
                        video->frame->format != AV_PIX_FMT_NONE) {

                        enum AVPixelFormat realSrc = (enum AVPixelFormat)video->frame->format;
                        bool realAlpha = rmv_IsAlphaPixFmt(realSrc);

                        TraceLog(LOG_INFO, "RAYMAPVID: First frame pix_fmt=%s (hint was %s, hasAlpha=%d)",
                                 av_get_pix_fmt_name(realSrc),
                                 av_get_pix_fmt_name(video->codecCtx->pix_fmt),
                                 realAlpha);

                        // Reallocate swsCtx + buffer if format differs from initial guess
                        if (!rmv_ReallocForFormat(video, realSrc)) {
                            video->state = RMV_STATE_ERROR;
                            return;
                        }

                        // If alpha presence changed, recreate texture
                        if (video->hasAlpha != realAlpha || video->textureCreated) {
                            if (video->textureCreated) {
                                UnloadTexture(video->texture);
                                video->texture.id   = 0;
                                video->textureCreated = false;
                            }
                            if (!rmv_CreateOrUpdateTexture(video)) {
                                video->state = RMV_STATE_ERROR;
                                return;
                            }
                        }
                    }
                }

                // ---------------------------------------------------------------
                // Convert frame to RGB/RGBA
                // sws_scale handles: YUV420P->RGB24, YUVA420P->RGBA
                // When src=YUVA420P and dst=RGBA, the alpha plane is preserved.
                // ---------------------------------------------------------------
                sws_scale(
                    video->swsCtx,
                    (const uint8_t *const *)video->frame->data,
                    video->frame->linesize,
                    0, video->height,
                    video->frameRGB->data,
                    video->frameRGB->linesize
                );

                // Upload to GPU
                UpdateTexture(video->texture, video->rgbBuffer);

                frameDecoded = true;

            } else if (ret == AVERROR(EAGAIN)) {
                // Need more packets — continue reading
                continue;
            } else if (ret == AVERROR_EOF) {
                frameDecoded = true;
            } else {
                TraceLog(LOG_ERROR, "RAYMAPVID: receive_frame: %s", rmv_FFmpegError(ret));
                return;
            }
        }
    }
}

//--------------------------------------------------------------------------------------------
// Public API — Playback control
//--------------------------------------------------------------------------------------------

RMVAPI void RMV_PlayVideo(RMV_Video *video) {
    if (!rmv_ValidateVideo(video, "RMV_PlayVideo")) return;
    video->state = RMV_STATE_PLAYING;
    TraceLog(LOG_INFO, "RAYMAPVID: Playing");
}

RMVAPI void RMV_PauseVideo(RMV_Video *video) {
    if (!rmv_ValidateVideo(video, "RMV_PauseVideo")) return;
    video->state = RMV_STATE_PAUSED;
    TraceLog(LOG_INFO, "RAYMAPVID: Paused");
}

RMVAPI void RMV_StopVideo(RMV_Video *video) {
    if (!rmv_ValidateVideo(video, "RMV_StopVideo")) return;
    video->state       = RMV_STATE_STOPPED;
    video->currentTime = 0.0f;
    TraceLog(LOG_INFO, "RAYMAPVID: Stopped");
}

RMVAPI void RMV_ToggleVideoPause(RMV_Video *video) {
    if (!rmv_ValidateVideo(video, "RMV_ToggleVideoPause")) return;
    if      (video->state == RMV_STATE_PLAYING) RMV_PauseVideo(video);
    else if (video->state == RMV_STATE_PAUSED)  RMV_PlayVideo(video);
}

RMVAPI RMV_PlaybackState RMV_GetVideoState(const RMV_Video *video) {
    if (!rmv_ValidateVideo(video, "RMV_GetVideoState")) return RMV_STATE_ERROR;
    return video->state;
}

RMVAPI bool RMV_IsVideoPlaying(const RMV_Video *video) {
    if (!rmv_ValidateVideo(video, "RMV_IsVideoPlaying")) return false;
    return (video->state == RMV_STATE_PLAYING);
}

RMVAPI bool RMV_IsVideoLoaded(const RMV_Video *video) {
    return (video != NULL && video->isLoaded);
}

RMVAPI void RMV_SetVideoLoop(RMV_Video *video, bool loop) {
    if (!rmv_ValidateVideo(video, "RMV_SetVideoLoop")) return;
    video->loop = loop;
}

RMVAPI void RMV_SetVideoSpeed(RMV_Video *video, float speed) {
    if (!rmv_ValidateVideo(video, "RMV_SetVideoSpeed")) return;
    if (speed < 0.0f) speed = 0.0f;
    video->speed = speed;
}

#endif // RAYMAPVID_IMPLEMENTATION
