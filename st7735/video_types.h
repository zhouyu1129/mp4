//
// 视频播放器类型定义和接口
// 支持从SD卡流式播放AVI视频（MJPEG和Raw RGB编码）
//

#ifndef SD_AND_LCD2_VIDEO_TYPES_H
#define SD_AND_LCD2_VIDEO_TYPES_H

#include "ff.h"
#include "tjpgd.h"

#ifdef __cplusplus
extern "C" {
#else
#include <stdint.h>
#endif

#define VIDEO_MAX_FRAMES 100000
#define VIDEO_TJPGDEC_WORKSPACE 11000

typedef enum {
    VIDEO_FORMAT_UNKNOWN = 0,
    VIDEO_FORMAT_MJPEG,
    VIDEO_FORMAT_RAW_RGB565,
    VIDEO_FORMAT_RAW_RGB565_LE,
    VIDEO_FORMAT_RAW_RGB565_BE,
    VIDEO_FORMAT_RAW_RGB888
} VideoFormat;

typedef enum {
    VIDEO_CODEC_UNKNOWN = 0,
    VIDEO_CODEC_MJPG,
    VIDEO_CODEC_RAW
} VideoCodec;

typedef struct {
    char filename[64];
    uint16_t width;
    uint16_t height;
    uint16_t fps;
    uint32_t total_frames;
    uint32_t duration_ms;
    VideoFormat format;
    VideoCodec codec;
    uint32_t file_size;
    uint32_t movi_offset;
    uint32_t frame_size;
    bool has_index;
} VideoInfo;

typedef struct VideoHandle* VideoHandle_t;

typedef enum {
    VIDEO_SUCCESS = 0,
    VIDEO_ERROR_FILE_NOT_FOUND,
    VIDEO_ERROR_FILE_OPEN,
    VIDEO_ERROR_FILE_READ,
    VIDEO_ERROR_INVALID_FORMAT,
    VIDEO_ERROR_MEMORY_ALLOC,
    VIDEO_ERROR_INVALID_PARAM,
    VIDEO_ERROR_UNSUPPORTED_FORMAT,
    VIDEO_ERROR_DECODE_FAILED,
    VIDEO_ERROR_NOT_OPEN,
    VIDEO_ERROR_END_OF_VIDEO,
    VIDEO_ERROR_PLAYBACK_ERROR
} VideoError;

typedef enum {
    VIDEO_STATE_IDLE = 0,
    VIDEO_STATE_PLAYING,
    VIDEO_STATE_PAUSED,
    VIDEO_STATE_ENDED
} VideoState;

typedef enum {
    VIDEO_PLAY_MODE_BLOCKING = 0,
    VIDEO_PLAY_MODE_POLLING
} VideoPlayMode;

typedef void (*VideoPlayCallback)(VideoHandle_t handle, uint32_t frame_num, void* user_data);

VideoError VIDEO_Init();
void VIDEO_Deinit();

VideoError VIDEO_Open(const char* filename, VideoHandle_t* handle);
void VIDEO_Close(VideoHandle_t handle);

VideoError VIDEO_GetInfo(VideoHandle_t handle, VideoInfo* info);
VideoError VIDEO_ParseInfo(const char* filename, VideoInfo* info);

VideoError VIDEO_Play(VideoHandle_t handle, uint16_t x, uint16_t y, VideoPlayMode mode);
VideoError VIDEO_PlayWithCallback(VideoHandle_t handle, uint16_t x, uint16_t y,
                                  VideoPlayCallback callback, void* user_data);

VideoError VIDEO_Poll(VideoHandle_t handle);
bool VIDEO_NeedsRender(VideoHandle_t handle);

VideoError VIDEO_Pause(VideoHandle_t handle);
VideoError VIDEO_Resume(VideoHandle_t handle);
VideoError VIDEO_Stop(VideoHandle_t handle);
VideoError VIDEO_ResetTime(VideoHandle_t handle);
VideoError VIDEO_Seek(VideoHandle_t handle, uint32_t frame_num);
VideoError VIDEO_SeekTime(VideoHandle_t handle, uint32_t time_ms);

VideoState VIDEO_GetState(VideoHandle_t handle);
uint32_t VIDEO_GetCurrentFrame(VideoHandle_t handle);
uint32_t VIDEO_GetElapsedTime(VideoHandle_t handle);

uint32_t VIDEO_GetFramesSkipped(VideoHandle_t handle);
uint32_t VIDEO_GetFramesRendered(VideoHandle_t handle);
float VIDEO_GetAverageFps(VideoHandle_t handle);

bool VIDEO_IsSupportedFormat(const char* filename);
const char* VIDEO_GetErrorString(VideoError error);
VideoError VIDEO_GetLastError();

#ifdef __cplusplus

class VideoPlayer {
private:
    VideoHandle_t handle;
    
public:
    VideoPlayer() : handle(nullptr) {}
    
    explicit VideoPlayer(const char* filename) : handle(nullptr) {
        Open(filename);
    }
    
    ~VideoPlayer() {
        Close();
    }
    
    bool Open(const char* filename) {
        Close();
        return VIDEO_Open(filename, &handle) == VIDEO_SUCCESS;
    }
    
    void Close() {
        if (handle) {
            VIDEO_Close(handle);
            handle = nullptr;
        }
    }
    
    bool IsOpen() const {
        return handle != nullptr;
    }
    
    bool GetInfo(VideoInfo* info) const {
        if (!handle) return false;
        return VIDEO_GetInfo(handle, info) == VIDEO_SUCCESS;
    }
    
    bool Play(uint16_t x, uint16_t y, VideoPlayMode mode = VIDEO_PLAY_MODE_BLOCKING) const {
        if (!handle) return false;
        return VIDEO_Play(handle, x, y, mode) == VIDEO_SUCCESS;
    }
    
    bool PlayWithCallback(uint16_t x, uint16_t y, VideoPlayCallback callback, void* user_data = nullptr) const {
        if (!handle) return false;
        return VIDEO_PlayWithCallback(handle, x, y, callback, user_data) == VIDEO_SUCCESS;
    }
    
    bool Poll() const {
        if (!handle) return false;
        return VIDEO_Poll(handle) == VIDEO_SUCCESS;
    }
    
    bool NeedsRender() const {
        if (!handle) return false;
        return VIDEO_NeedsRender(handle);
    }
    
    bool Pause() const {
        if (!handle) return false;
        return VIDEO_Pause(handle) == VIDEO_SUCCESS;
    }
    
    bool Resume() const {
        if (!handle) return false;
        return VIDEO_Resume(handle) == VIDEO_SUCCESS;
    }
    
    bool Stop() const {
        if (!handle) return false;
        return VIDEO_Stop(handle) == VIDEO_SUCCESS;
    }
    
    bool ResetTime() const {
        if (!handle) return false;
        return VIDEO_ResetTime(handle) == VIDEO_SUCCESS;
    }
    
    bool Seek(uint32_t frame_num) const {
        if (!handle) return false;
        return VIDEO_Seek(handle, frame_num) == VIDEO_SUCCESS;
    }
    
    bool SeekTime(uint32_t time_ms) const {
        if (!handle) return false;
        return VIDEO_SeekTime(handle, time_ms) == VIDEO_SUCCESS;
    }
    
    VideoState GetState() const {
        if (!handle) return VIDEO_STATE_IDLE;
        return VIDEO_GetState(handle);
    }
    
    uint32_t GetCurrentFrame() const {
        if (!handle) return 0;
        return VIDEO_GetCurrentFrame(handle);
    }
    
    uint32_t GetElapsedTime() const {
        if (!handle) return 0;
        return VIDEO_GetElapsedTime(handle);
    }
    
    uint32_t GetFramesSkipped() const {
        if (!handle) return 0;
        return VIDEO_GetFramesSkipped(handle);
    }
    
    uint32_t GetFramesRendered() const {
        if (!handle) return 0;
        return VIDEO_GetFramesRendered(handle);
    }
    
    float GetAverageFps() const {
        if (!handle) return 0.0f;
        return VIDEO_GetAverageFps(handle);
    }
    
    static VideoError GetLastError() {
        return VIDEO_GetLastError();
    }
    
    static const char* GetErrorString() {
        return VIDEO_GetErrorString(GetLastError());
    }
    
    static bool ParseInfo(const char* filename, VideoInfo* info) {
        return VIDEO_ParseInfo(filename, info) == VIDEO_SUCCESS;
    }
    
    static bool IsSupportedFormat(const char* filename) {
        return VIDEO_IsSupportedFormat(filename);
    }
    
    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;
    
    VideoPlayer(VideoPlayer&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    
    VideoPlayer& operator=(VideoPlayer&& other) noexcept {
        if (this != &other) {
            Close();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
};

#endif // __cplusplus

#ifdef __cplusplus
}
#endif

#endif // SD_AND_LCD2_VIDEO_TYPES_H
