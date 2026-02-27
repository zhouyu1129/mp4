//
// 视频播放器类型实现
// 支持从SD卡流式播放AVI视频（MJPEG和Raw RGB编码）
//

#include "video_types.h"
#include "st7735.h"
#include "fatfs.h"
#include <cstring>
#include <cstdlib>

extern "C" {
#include "tjpgd.h"
}

extern SPI_HandleTypeDef ST7735_SPI_PORT;

#define AVI_RIFF_ID    0x46464952
#define AVI_AVI_ID     0x20495641
#define AVI_LIST_ID    0x5453494C
#define AVI_HDRL_ID    0x6C726468
#define AVI_MOVI_ID    0x69766F6D
#define AVI_STRL_ID    0x6C727473
#define AVI_AVIH_ID    0x68697661
#define AVI_STRH_ID    0x68727473
#define AVI_STRF_ID    0x66727473
#define AVI_STRN_ID    0x6E727473
#define AVI_VIDS_ID    0x73646976
#define AVI_AUDS_ID    0x73647561
#define AVI_00DC_ID    0x63643030
#define AVI_01DC_ID    0x63643031
#define AVI_00WB_ID    0x62773030
#define AVI_JUNK_ID    0x4B4E554A
#define AVI_IDX1_ID    0x31786469

#define MJPG_FOURCC    0x47504A4D
#define RAW_FOURCC     0x20324D52

#pragma pack(push, 1)
typedef struct {
    uint32_t dwMicroSecPerFrame;
    uint32_t dwMaxBytesPerSec;
    uint32_t dwPaddingGranularity;
    uint32_t dwFlags;
    uint32_t dwTotalFrames;
    uint32_t dwInitialFrames;
    uint32_t dwStreams;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwWidth;
    uint32_t dwHeight;
    uint32_t dwReserved[4];
} AVIMainHeader;

typedef struct {
    uint32_t fccType;
    uint32_t fccHandler;
    uint32_t dwFlags;
    uint16_t wPriority;
    uint16_t wLanguage;
    uint32_t dwInitialFrames;
    uint32_t dwScale;
    uint32_t dwRate;
    uint32_t dwStart;
    uint32_t dwLength;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwQuality;
    uint32_t dwSampleSize;
    uint16_t rcFrame_left;
    uint16_t rcFrame_top;
    uint16_t rcFrame_right;
    uint16_t rcFrame_bottom;
} AVIStreamHeader;

typedef struct {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;

typedef struct {
    uint32_t ckid;
    uint32_t dwFlags;
    uint32_t dwChunkOffset;
    uint32_t dwChunkLength;
} AVIIndexEntry;
#pragma pack(pop)

typedef struct {
    uint32_t offset;
    uint32_t size;
    uint8_t frame_type;
} FrameIndex;

typedef struct VideoHandle {
    VideoInfo info;
    FIL file;
    bool is_open;
    VideoState state;
    VideoPlayMode play_mode;
    
    uint16_t display_x;
    uint16_t display_y;
    
    uint32_t current_frame;
    uint32_t start_time_ms;
    uint32_t last_frame_time_ms;
    uint32_t frame_duration_ms;
    uint16_t fps;
    uint32_t frame_duration_us;
    
    uint32_t frames_skipped;
    uint32_t frames_rendered;
    
    FrameIndex* frame_index;
    uint32_t frame_index_count;
    uint32_t frame_index_capacity;
    
    uint32_t current_chunk_offset;
    uint32_t next_chunk_offset;
    
    uint8_t* jpeg_workbuf;
    JDEC jdec;
    
    VideoPlayCallback callback;
    void* callback_user_data;
    
    bool needs_byte_swap;
} VideoHandle;

typedef struct {
    FIL* file;
    uint8_t* workbuf;
    uint16_t display_x;
    uint16_t display_y;
    uint16_t display_width;
    uint16_t display_height;
    uint8_t scale;
    uint32_t frame_end_offset;
} VideoJpegContext;

static VideoError g_last_error = VIDEO_SUCCESS;

static const char* error_strings[] = {
    "成功",
    "文件未找到",
    "文件打开失败",
    "文件读取失败",
    "无效的格式",
    "内存分配失败",
    "无效的参数",
    "不支持的格式",
    "解码失败",
    "视频未打开",
    "视频播放结束",
    "播放错误"
};

static uint32_t read_le32(FIL* file);
static VideoError parse_avi_header(VideoHandle_t handle);
static VideoError build_frame_index(VideoHandle_t handle);
static VideoError decode_and_display_frame_streaming(VideoHandle_t handle);
static VideoError decode_mjpeg_frame(VideoHandle_t handle, uint32_t offset, uint32_t size);
static VideoError decode_raw_frame(VideoHandle_t handle, uint32_t offset, uint32_t size);
static size_t video_jpeg_input_func(JDEC* jd, uint8_t* buf, size_t nbyte);
static int video_jpeg_output_func(JDEC* jd, void* bitmap, JRECT* rect);
static uint32_t get_tick_ms(void);
static bool detect_rgb565_endianness(VideoHandle_t handle);

VideoError VIDEO_Init() {
    g_last_error = VIDEO_SUCCESS;
    return VIDEO_SUCCESS;
}

void VIDEO_Deinit() {
}

VideoError VIDEO_Open(const char* filename, VideoHandle_t* handle) {
    if (!filename || !handle) {
        g_last_error = VIDEO_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    *handle = nullptr;
    
    VideoHandle* vh = (VideoHandle*)malloc(sizeof(VideoHandle));
    if (!vh) {
        g_last_error = VIDEO_ERROR_MEMORY_ALLOC;
        return g_last_error;
    }
    
    memset(vh, 0, sizeof(VideoHandle));
    strncpy(vh->info.filename, filename, sizeof(vh->info.filename) - 1);
    
    FRESULT res = f_open(&vh->file, filename, FA_READ);
    if (res != FR_OK) {
        free(vh);
        g_last_error = (res == FR_NO_FILE) ? VIDEO_ERROR_FILE_NOT_FOUND : VIDEO_ERROR_FILE_OPEN;
        return g_last_error;
    }
    
    vh->info.file_size = f_size(&vh->file);
    vh->is_open = true;
    vh->state = VIDEO_STATE_IDLE;
    
    VideoError error = parse_avi_header(vh);
    if (error != VIDEO_SUCCESS) {
        f_close(&vh->file);
        if (vh->frame_index) free(vh->frame_index);
        free(vh);
        g_last_error = error;
        return error;
    }
    
    error = build_frame_index(vh);
    if (error != VIDEO_SUCCESS) {
        f_close(&vh->file);
        if (vh->frame_index) free(vh->frame_index);
        free(vh);
        g_last_error = error;
        return error;
    }
    
    vh->jpeg_workbuf = (uint8_t*)malloc(VIDEO_TJPGDEC_WORKSPACE);
    if (!vh->jpeg_workbuf) {
        f_close(&vh->file);
        if (vh->frame_index) free(vh->frame_index);
        free(vh);
        g_last_error = VIDEO_ERROR_MEMORY_ALLOC;
        return g_last_error;
    }
    
    vh->frame_duration_ms = 1000 / vh->info.fps;
    if (vh->frame_duration_ms == 0) vh->frame_duration_ms = 33;
    vh->fps = vh->info.fps;
    vh->frame_duration_us = 1000000 / vh->info.fps;
    
    vh->frames_skipped = 0;
    vh->frames_rendered = 0;
    
    vh->needs_byte_swap = detect_rgb565_endianness(vh);
    
    *handle = vh;
    g_last_error = VIDEO_SUCCESS;
    return VIDEO_SUCCESS;
}

void VIDEO_Close(VideoHandle_t handle) {
    if (!handle) return;
    
    if (handle->is_open) {
        f_close(&handle->file);
    }
    
    if (handle->frame_index) {
        free(handle->frame_index);
    }
    
    if (handle->jpeg_workbuf) {
        free(handle->jpeg_workbuf);
    }
    
    free(handle);
}

VideoError VIDEO_GetInfo(VideoHandle_t handle, VideoInfo* info) {
    if (!handle || !info) {
        g_last_error = VIDEO_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    if (!handle->is_open) {
        g_last_error = VIDEO_ERROR_NOT_OPEN;
        return g_last_error;
    }
    
    *info = handle->info;
    g_last_error = VIDEO_SUCCESS;
    return VIDEO_SUCCESS;
}

VideoError VIDEO_ParseInfo(const char* filename, VideoInfo* info) {
    if (!filename || !info) {
        g_last_error = VIDEO_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    VideoHandle_t handle;
    VideoError error = VIDEO_Open(filename, &handle);
    if (error != VIDEO_SUCCESS) {
        return error;
    }
    
    error = VIDEO_GetInfo(handle, info);
    VIDEO_Close(handle);
    
    return error;
}

VideoError VIDEO_Play(VideoHandle_t handle, uint16_t x, uint16_t y, VideoPlayMode mode) {
    if (!handle || !handle->is_open) {
        g_last_error = VIDEO_ERROR_NOT_OPEN;
        return g_last_error;
    }
    
    if (x + handle->info.width > ST7735_WIDTH || y + handle->info.height > ST7735_HEIGHT) {
        g_last_error = VIDEO_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    handle->display_x = x;
    handle->display_y = y;
    handle->play_mode = mode;
    handle->current_frame = 0;
    handle->start_time_ms = get_tick_ms();
    handle->last_frame_time_ms = handle->start_time_ms;
    handle->state = VIDEO_STATE_PLAYING;
    
    // 流式播放：定位到movi数据开始位置
    handle->current_chunk_offset = handle->info.movi_offset + 4;
    
    if (mode == VIDEO_PLAY_MODE_BLOCKING) {
        while (handle->state == VIDEO_STATE_PLAYING) {
            VideoError err = VIDEO_Poll(handle);
            if (err == VIDEO_ERROR_END_OF_VIDEO) {
                handle->state = VIDEO_STATE_ENDED;
                break;
            } else if (err != VIDEO_SUCCESS) {
                return err;
            }
        }
        g_last_error = VIDEO_SUCCESS;
        return VIDEO_SUCCESS;
    }
    
    g_last_error = VIDEO_SUCCESS;
    return VIDEO_SUCCESS;
}

VideoError VIDEO_PlayWithCallback(VideoHandle_t handle, uint16_t x, uint16_t y,
                                  VideoPlayCallback callback, void* user_data) {
    handle->callback = callback;
    handle->callback_user_data = user_data;
    return VIDEO_Play(handle, x, y, VIDEO_PLAY_MODE_BLOCKING);
}

VideoError VIDEO_Poll(VideoHandle_t handle) {
    if (!handle || !handle->is_open) {
        g_last_error = VIDEO_ERROR_NOT_OPEN;
        return g_last_error;
    }
    
    if (handle->state != VIDEO_STATE_PLAYING) {
        if (handle->state == VIDEO_STATE_ENDED) {
            g_last_error = VIDEO_ERROR_END_OF_VIDEO;
            return VIDEO_ERROR_END_OF_VIDEO;
        }
        g_last_error = VIDEO_SUCCESS;
        return VIDEO_SUCCESS;
    }
    
    uint32_t current_time = get_tick_ms();
    uint32_t elapsed_since_start_ms = current_time - handle->start_time_ms;
    
    // 使用微秒精度计算预期帧，避免整数除法精度损失
    uint32_t elapsed_since_start_us = elapsed_since_start_ms * 1000;
    uint32_t expected_frame = elapsed_since_start_us / handle->frame_duration_us;
    
    if (expected_frame >= handle->info.total_frames) {
        handle->state = VIDEO_STATE_ENDED;
        g_last_error = VIDEO_ERROR_END_OF_VIDEO;
        return VIDEO_ERROR_END_OF_VIDEO;
    }
    
    // 轮询模式：检查是否到了播放当前帧的时间
    // 当前帧应该在 current_frame * frame_duration_us 到 (current_frame+1) * frame_duration_us 之间播放
    if (handle->play_mode == VIDEO_PLAY_MODE_POLLING) {
        uint32_t current_frame_start_us = handle->current_frame * handle->frame_duration_us;
        if (elapsed_since_start_us < current_frame_start_us) {
            // 还没到当前帧的播放时间
            g_last_error = VIDEO_SUCCESS;
            return VIDEO_SUCCESS;
        }
    }
    
    // 如果落后于预期帧，跳过中间的帧
    if (expected_frame > handle->current_frame) {
        uint32_t frames_to_skip = expected_frame - handle->current_frame;
        handle->frames_skipped += frames_to_skip;
        
        // 跳过帧（流式播放需要跳过对应的数据块）
        for (uint32_t i = 0; i < frames_to_skip; i++) {
            FIL* file = &handle->file;
            FRESULT res = f_lseek(file, handle->current_chunk_offset);
            if (res != FR_OK) {
                handle->state = VIDEO_STATE_ENDED;
                g_last_error = VIDEO_ERROR_FILE_READ;
                return VIDEO_ERROR_FILE_READ;
            }
            
            read_le32(file);
            uint32_t chunk_size = read_le32(file);
            
            if (chunk_size & 1) chunk_size++;
            handle->current_chunk_offset = f_tell(file) + chunk_size;
        }
        
        handle->current_frame = expected_frame;
    }
    
    VideoError error = decode_and_display_frame_streaming(handle);
    if (error != VIDEO_SUCCESS) {
        handle->state = VIDEO_STATE_ENDED;
        g_last_error = error;
        return error;
    }
    
    handle->frames_rendered++;
    handle->current_frame++;
    
    if (handle->callback) {
        handle->callback(handle, handle->current_frame, handle->callback_user_data);
    }
    
    if (handle->current_frame >= handle->info.total_frames) {
        handle->state = VIDEO_STATE_ENDED;
        g_last_error = VIDEO_ERROR_END_OF_VIDEO;
        return VIDEO_ERROR_END_OF_VIDEO;
    }
    
    // 独占模式：等待下一帧时间
    if (handle->play_mode == VIDEO_PLAY_MODE_BLOCKING) {
        uint32_t next_frame_start_us = handle->current_frame * handle->frame_duration_us;
        uint32_t next_frame_time_ms = handle->start_time_ms + (next_frame_start_us / 1000);
        current_time = get_tick_ms();
        if (current_time < next_frame_time_ms) {
            while (get_tick_ms() < next_frame_time_ms) {
                __NOP();
            }
        }
    }
    
    g_last_error = VIDEO_SUCCESS;
    return VIDEO_SUCCESS;
}

bool VIDEO_NeedsRender(VideoHandle_t handle) {
    if (!handle || !handle->is_open || handle->state != VIDEO_STATE_PLAYING) {
        return false;
    }
    
    uint32_t current_time = get_tick_ms();
    uint32_t elapsed_since_start_ms = current_time - handle->start_time_ms;
    uint32_t elapsed_since_start_us = elapsed_since_start_ms * 1000;
    
    uint32_t current_frame_start_us = handle->current_frame * handle->frame_duration_us;
    
    return elapsed_since_start_us >= current_frame_start_us;
}

VideoError VIDEO_Pause(VideoHandle_t handle) {
    if (!handle || !handle->is_open) {
        g_last_error = VIDEO_ERROR_NOT_OPEN;
        return g_last_error;
    }
    
    if (handle->state == VIDEO_STATE_PLAYING) {
        handle->state = VIDEO_STATE_PAUSED;
    }
    
    g_last_error = VIDEO_SUCCESS;
    return VIDEO_SUCCESS;
}

VideoError VIDEO_Resume(VideoHandle_t handle) {
    if (!handle || !handle->is_open) {
        g_last_error = VIDEO_ERROR_NOT_OPEN;
        return g_last_error;
    }
    
    if (handle->state == VIDEO_STATE_PAUSED) {
        handle->state = VIDEO_STATE_PLAYING;
        uint32_t elapsed_us = handle->current_frame * handle->frame_duration_us;
        handle->start_time_ms = get_tick_ms() - (elapsed_us / 1000);
        handle->last_frame_time_ms = get_tick_ms();
    }
    
    g_last_error = VIDEO_SUCCESS;
    return VIDEO_SUCCESS;
}

VideoError VIDEO_Stop(VideoHandle_t handle) {
    if (!handle || !handle->is_open) {
        g_last_error = VIDEO_ERROR_NOT_OPEN;
        return g_last_error;
    }
    
    handle->state = VIDEO_STATE_IDLE;
    handle->current_frame = 0;
    
    g_last_error = VIDEO_SUCCESS;
    return VIDEO_SUCCESS;
}

VideoError VIDEO_ResetTime(VideoHandle_t handle) {
    if (!handle || !handle->is_open) {
        g_last_error = VIDEO_ERROR_NOT_OPEN;
        return g_last_error;
    }
    
    uint32_t elapsed_us = handle->current_frame * handle->frame_duration_us;
    handle->start_time_ms = get_tick_ms() - (elapsed_us / 1000);
    handle->last_frame_time_ms = get_tick_ms();
    
    g_last_error = VIDEO_SUCCESS;
    return VIDEO_SUCCESS;
}

VideoError VIDEO_Seek(VideoHandle_t handle, uint32_t frame_num) {
    if (!handle || !handle->is_open) {
        g_last_error = VIDEO_ERROR_NOT_OPEN;
        return g_last_error;
    }
    
    if (frame_num >= handle->info.total_frames) {
        frame_num = handle->info.total_frames - 1;
    }
    
    handle->current_frame = frame_num;
    uint32_t elapsed_us = frame_num * handle->frame_duration_us;
    handle->start_time_ms = get_tick_ms() - (elapsed_us / 1000);
    handle->last_frame_time_ms = get_tick_ms();
    
    // 流式播放：重新定位到movi开始位置，然后跳过前面的帧
    handle->current_chunk_offset = handle->info.movi_offset + 4;
    
    // 跳过前面的帧（简化实现，只支持从开头播放）
    // 在实际应用中，可能需要更复杂的seek逻辑
    
    g_last_error = VIDEO_SUCCESS;
    return VIDEO_SUCCESS;
}

VideoError VIDEO_SeekTime(VideoHandle_t handle, uint32_t time_ms) {
    if (!handle || !handle->is_open) {
        g_last_error = VIDEO_ERROR_NOT_OPEN;
        return g_last_error;
    }
    
    uint32_t time_us = time_ms * 1000;
    uint32_t frame_num = time_us / handle->frame_duration_us;
    return VIDEO_Seek(handle, frame_num);
}

VideoState VIDEO_GetState(VideoHandle_t handle) {
    if (!handle) return VIDEO_STATE_IDLE;
    return handle->state;
}

uint32_t VIDEO_GetCurrentFrame(VideoHandle_t handle) {
    if (!handle) return 0;
    return handle->current_frame;
}

uint32_t VIDEO_GetElapsedTime(VideoHandle_t handle) {
    if (!handle || !handle->is_open) return 0;
    uint32_t elapsed_us = handle->current_frame * handle->frame_duration_us;
    return elapsed_us / 1000;
}

uint32_t VIDEO_GetFramesSkipped(VideoHandle_t handle) {
    if (!handle) return 0;
    return handle->frames_skipped;
}

uint32_t VIDEO_GetFramesRendered(VideoHandle_t handle) {
    if (!handle) return 0;
    return handle->frames_rendered;
}

float VIDEO_GetAverageFps(VideoHandle_t handle) {
    if (!handle || !handle->is_open) return 0.0f;
    
    uint32_t current_time = get_tick_ms();
    uint32_t elapsed_ms = current_time - handle->start_time_ms;
    
    if (elapsed_ms == 0) return 0.0f;
    
    return (handle->frames_rendered * 1000.0f) / elapsed_ms;
}

bool VIDEO_IsSupportedFormat(const char* filename) {
    if (!filename) return false;
    
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    
    if (strcasecmp(ext, ".avi") == 0) {
        return true;
    }
    
    return false;
}

const char* VIDEO_GetErrorString(VideoError error) {
    if (error < 0 || error >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "未知错误";
    }
    return error_strings[error];
}

VideoError VIDEO_GetLastError() {
    return g_last_error;
}

static uint32_t read_le32(FIL* file) {
    uint8_t buf[4];
    UINT br;
    f_read(file, buf, 4, &br);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

static VideoError parse_avi_header(VideoHandle_t handle) {
    FIL* file = &handle->file;
    
    uint32_t riff_id = read_le32(file);
    if (riff_id != AVI_RIFF_ID) {
        return VIDEO_ERROR_INVALID_FORMAT;
    }
    
    read_le32(file);
    
    uint32_t avi_id = read_le32(file);
    if (avi_id != AVI_AVI_ID) {
        return VIDEO_ERROR_INVALID_FORMAT;
    }
    
    bool found_hdrl = false;
    bool found_movi = false;
    bool found_video_stream = false;
    
    uint32_t file_size = handle->info.file_size;
    
    while (f_tell(file) < file_size - 8 && !found_movi) {
        uint32_t chunk_id = read_le32(file);
        uint32_t chunk_size = read_le32(file);
        uint32_t chunk_data_start = f_tell(file);
        uint32_t next_chunk = chunk_data_start + chunk_size;
        if (chunk_size & 1) next_chunk++;
        
        if (chunk_id == AVI_LIST_ID) {
            uint32_t list_type = read_le32(file);
            
            if (list_type == AVI_HDRL_ID) {
                found_hdrl = true;
                
                uint32_t hdrl_end = chunk_data_start + chunk_size;
                while (f_tell(file) < hdrl_end) {
                    uint32_t sub_id = read_le32(file);
                    uint32_t sub_size = read_le32(file);
                    uint32_t sub_start = f_tell(file);
                    uint32_t sub_end = sub_start + sub_size;
                    if (sub_size & 1) sub_end++;
                    
                    if (sub_id == AVI_AVIH_ID) {
                        AVIMainHeader avih;
                        UINT br;
                        f_read(file, &avih, sizeof(AVIMainHeader), &br);
                        
                        handle->info.width = avih.dwWidth;
                        handle->info.height = (uint16_t)abs((int32_t)avih.dwHeight);
                        handle->info.total_frames = avih.dwTotalFrames;
                        
                        if (avih.dwMicroSecPerFrame > 0) {
                            handle->info.fps = 1000000 / avih.dwMicroSecPerFrame;
                            if (handle->info.fps == 0) handle->info.fps = 30;
                        } else {
                            handle->info.fps = 30;
                        }
                        
                        handle->info.duration_ms = (handle->info.total_frames * 1000) / handle->info.fps;
                    }
                    else if (sub_id == AVI_LIST_ID) {
                        uint32_t strl_type = read_le32(file);
                        
                        if (strl_type == AVI_STRL_ID) {
                            uint32_t strl_end = sub_start + sub_size;
                            
                            while (f_tell(file) < strl_end) {
                                uint32_t str_chunk_id = read_le32(file);
                                uint32_t str_chunk_size = read_le32(file);
                                uint32_t str_chunk_start = f_tell(file);
                                uint32_t str_chunk_end = str_chunk_start + str_chunk_size;
                                if (str_chunk_size & 1) str_chunk_end++;
                                
                                if (str_chunk_id == AVI_STRH_ID) {
                                    AVIStreamHeader strh;
                                    UINT br;
                                    f_read(file, &strh, sizeof(AVIStreamHeader), &br);
                                    
                                    if (strh.fccType == AVI_VIDS_ID) {
                                        found_video_stream = true;
                                        
                                        uint8_t* handler = (uint8_t*)&strh.fccHandler;
                                        if (strh.fccHandler == MJPG_FOURCC ||
                                            (handler[0] == 'M' && handler[1] == 'J' && 
                                             handler[2] == 'P' && handler[3] == 'G') ||
                                            (handler[0] == 'm' && handler[1] == 'j' && 
                                             handler[2] == 'p' && handler[3] == 'g')) {
                                            handle->info.codec = VIDEO_CODEC_MJPG;
                                            handle->info.format = VIDEO_FORMAT_MJPEG;
                                        } else if (strh.fccHandler == 0 || strh.fccHandler == RAW_FOURCC) {
                                            handle->info.codec = VIDEO_CODEC_RAW;
                                            handle->info.format = VIDEO_FORMAT_RAW_RGB565;
                                        } else {
                                            handle->info.codec = VIDEO_CODEC_RAW;
                                            handle->info.format = VIDEO_FORMAT_RAW_RGB565;
                                        }
                                    }
                                }
                                else if (str_chunk_id == AVI_STRF_ID && found_video_stream) {
                                    BITMAPINFOHEADER bmih;
                                    UINT br;
                                    f_read(file, &bmih, sizeof(BITMAPINFOHEADER), &br);
                                    
                                    // 只有RAW编码才根据biBitCount设置格式
                                    // MJPEG编码已经设置了正确的格式，不要覆盖
                                    if (handle->info.codec == VIDEO_CODEC_RAW) {
                                        if (bmih.biBitCount == 16) {
                                            handle->info.format = VIDEO_FORMAT_RAW_RGB565;
                                        } else if (bmih.biBitCount == 24) {
                                            handle->info.format = VIDEO_FORMAT_RAW_RGB888;
                                        }
                                    }
                                    
                                    // 使用BITMAPINFOHEADER中的尺寸（可能更准确）
                                    handle->info.width = (uint16_t)abs(bmih.biWidth);
                                    handle->info.height = (uint16_t)abs(bmih.biHeight);
                                }
                                
                                f_lseek(file, str_chunk_end);
                            }
                        }
                    }
                    
                    f_lseek(file, sub_end);
                }
            }
            else if (list_type == AVI_MOVI_ID) {
                found_movi = true;
                handle->info.movi_offset = chunk_data_start;
            }
        }
        else if (chunk_id == AVI_IDX1_ID) {
            handle->info.has_index = true;
        }
        
        f_lseek(file, next_chunk);
    }
    
    if (!found_hdrl) {
        return VIDEO_ERROR_INVALID_FORMAT;
    }
    
    if (!found_video_stream) {
        handle->info.codec = VIDEO_CODEC_RAW;
        handle->info.format = VIDEO_FORMAT_RAW_RGB565;
    }
    
    if (!found_movi) {
        return VIDEO_ERROR_INVALID_FORMAT;
    }
    
    return VIDEO_SUCCESS;
}

static VideoError build_frame_index(VideoHandle_t handle) {
    FIL* file = &handle->file;
    
    // 流式播放：不构建完整帧索引，只计算总帧数
    handle->frame_index = nullptr;
    handle->frame_index_count = 0;
    
    f_lseek(file, handle->info.movi_offset + 4);
    
    uint32_t movi_end = handle->info.file_size;
    uint32_t frame_count = 0;
    
    while (f_tell(file) < movi_end - 8) {
        uint32_t chunk_id = read_le32(file);
        uint32_t chunk_size = read_le32(file);
        uint32_t chunk_offset = f_tell(file);
        
        uint8_t* id_bytes = (uint8_t*)&chunk_id;
        
        bool is_video_chunk = false;
        if ((id_bytes[2] == 'd' && id_bytes[3] == 'c') ||
            (id_bytes[2] == 'd' && id_bytes[3] == 'b')) {
            is_video_chunk = true;
        }
        
        if (is_video_chunk && chunk_size > 0) {
            frame_count++;
        }
        
        if (chunk_size & 1) chunk_size++;
        f_lseek(file, chunk_offset + chunk_size);
    }
    
    if (frame_count == 0) {
        return VIDEO_ERROR_INVALID_FORMAT;
    }
    
    handle->info.total_frames = frame_count;
    
    return VIDEO_SUCCESS;
}

static bool detect_rgb565_endianness(VideoHandle_t handle) {
    if (handle->info.codec != VIDEO_CODEC_RAW) {
        return false;
    }
    
    if (handle->info.format == VIDEO_FORMAT_RAW_RGB565_BE) {
        return false;
    }
    
    if (handle->info.format == VIDEO_FORMAT_RAW_RGB565_LE) {
        return true;
    }
    
    const char* filename = handle->info.filename;
    const char* p = filename;
    while (*p) {
        if ((p[0] == '5' || p[0] == '5') &&
            (p[1] == '6' || p[1] == '6') &&
            (p[2] == '5' || p[2] == '5')) {
            
            if ((p[3] == 'b' || p[3] == 'B') &&
                (p[4] == 'e' || p[4] == 'E')) {
                return false;
            }
            if ((p[3] == 'l' || p[3] == 'L') &&
                (p[4] == 'e' || p[4] == 'E')) {
                return true;
            }
        }
        p++;
    }
    
    FIL* file = &handle->file;
    uint32_t saved_pos = f_tell(file);
    
    FRESULT res = f_lseek(file, handle->info.movi_offset);
    if (res != FR_OK) {
        f_lseek(file, saved_pos);
        return true;
    }
    
    int max_frames_to_check = 100;
    int frames_checked = 0;
    
    while (frames_checked < max_frames_to_check && f_tell(file) < handle->info.file_size - 8) {
        uint32_t chunk_id = read_le32(file);
        uint32_t chunk_size = read_le32(file);
        
        uint8_t* id_bytes = (uint8_t*)&chunk_id;
        bool is_video_chunk = ((id_bytes[2] == 'd' && id_bytes[3] == 'c') ||
                              (id_bytes[2] == 'd' && id_bytes[3] == 'b'));
        
        if (!is_video_chunk || chunk_size < 32) {
            if (chunk_size & 1) chunk_size++;
            f_lseek(file, f_tell(file) + chunk_size);
            continue;
        }
        
        uint32_t sample_offsets[] = {
            0,
            chunk_size / 4,
            chunk_size / 2,
            chunk_size * 3 / 4
        };
        
        for (int offset_idx = 0; offset_idx < 4; offset_idx++) {
            uint32_t offset = sample_offsets[offset_idx];
            if (offset + 32 > chunk_size) continue;
            
            res = f_lseek(file, f_tell(file));
            uint32_t frame_data_start = f_tell(file);
            res = f_lseek(file, frame_data_start + offset);
            if (res != FR_OK) continue;
            
            uint8_t sample_pixels[32];
            UINT br;
            res = f_read(file, sample_pixels, 32, &br);
            if (res != FR_OK || br < 32) continue;
            
            int non_zero_count = 0;
            for (int i = 0; i < 32; i++) {
                if (sample_pixels[i] != 0 && sample_pixels[i] != 0xFF) {
                    non_zero_count++;
                }
            }
            
            if (non_zero_count < 8) continue;
            
            int le_valid_count = 0;
            int be_valid_count = 0;
            
            for (int i = 0; i < 16; i++) {
                uint16_t le_pixel = sample_pixels[i * 2] | (sample_pixels[i * 2 + 1] << 8);
                uint16_t be_pixel = (sample_pixels[i * 2] << 8) | sample_pixels[i * 2 + 1];
                
                uint8_t le_r = (le_pixel >> 11) & 0x1F;
                uint8_t le_g = (le_pixel >> 5) & 0x3F;
                uint8_t le_b = le_pixel & 0x1F;
                
                uint8_t be_r = (be_pixel >> 11) & 0x1F;
                uint8_t be_g = (be_pixel >> 5) & 0x3F;
                uint8_t be_b = be_pixel & 0x1F;
                
                if (le_r <= 31 && le_g <= 63 && le_b <= 31) {
                    le_valid_count++;
                }
                if (be_r <= 31 && be_g <= 63 && be_b <= 31) {
                    be_valid_count++;
                }
            }
            
            if (le_valid_count != be_valid_count) {
                f_lseek(file, saved_pos);
                return be_valid_count > le_valid_count ? false : true;
            }
        }
        
        frames_checked++;
        if (chunk_size & 1) chunk_size++;
        f_lseek(file, f_tell(file) + chunk_size);
    }
    
    f_lseek(file, saved_pos);
    return true;
}

static VideoError decode_and_display_frame_streaming(VideoHandle_t handle) {
    FIL* file = &handle->file;
    
    // 定位到当前帧位置
    FRESULT res = f_lseek(file, handle->current_chunk_offset);
    if (res != FR_OK) {
        return VIDEO_ERROR_FILE_READ;
    }
    
    // 读取当前帧的chunk信息
    uint32_t chunk_id = read_le32(file);
    uint32_t chunk_size = read_le32(file);
    uint32_t frame_offset = f_tell(file);
    
    // 检查是否为视频帧
    uint8_t* id_bytes = (uint8_t*)&chunk_id;
    bool is_video_chunk = ((id_bytes[2] == 'd' && id_bytes[3] == 'c') ||
                          (id_bytes[2] == 'd' && id_bytes[3] == 'b'));
    
    if (!is_video_chunk || chunk_size == 0) {
        return VIDEO_ERROR_INVALID_FORMAT;
    }
    
    // 解码并显示当前帧
    VideoError error;
    if (handle->info.codec == VIDEO_CODEC_MJPG) {
        error = decode_mjpeg_frame(handle, frame_offset, chunk_size);
    } else {
        error = decode_raw_frame(handle, frame_offset, chunk_size);
    }
    
    if (error != VIDEO_SUCCESS) {
        return error;
    }
    
    // 更新下一帧的位置
    if (chunk_size & 1) chunk_size++;
    handle->current_chunk_offset = frame_offset + chunk_size;
    
    return VIDEO_SUCCESS;
}

static VideoError decode_mjpeg_frame(VideoHandle_t handle, uint32_t offset, uint32_t size) {
    FIL* file = &handle->file;
    
    FRESULT res = f_lseek(file, offset);
    if (res != FR_OK) {
        return VIDEO_ERROR_FILE_READ;
    }
    
    VideoJpegContext ctx;
    ctx.file = file;
    ctx.workbuf = handle->jpeg_workbuf;
    ctx.display_x = handle->display_x;
    ctx.display_y = handle->display_y;
    ctx.display_width = handle->info.width;
    ctx.display_height = handle->info.height;
    ctx.frame_end_offset = offset + size;
    
    JDEC jdec;
    JRESULT jres = jd_prepare(&jdec, video_jpeg_input_func, handle->jpeg_workbuf, VIDEO_TJPGDEC_WORKSPACE, &ctx);
    if (jres != JDR_OK) {
        return VIDEO_ERROR_DECODE_FAILED;
    }
    
    uint16_t scale_factor = 1;
    uint8_t scale = 0;
    while (jdec.width / scale_factor > ST7735_WIDTH || jdec.height / scale_factor > ST7735_HEIGHT) {
        scale++;
        scale_factor <<= 1;
        if (scale > 3) break;
    }
    
    ctx.display_width = (jdec.width + scale_factor - 1) / scale_factor;
    ctx.display_height = (jdec.height + scale_factor - 1) / scale_factor;
    
    jres = jd_decomp(&jdec, video_jpeg_output_func, scale);
    if (jres != JDR_OK) {
        return VIDEO_ERROR_DECODE_FAILED;
    }
    
    return VIDEO_SUCCESS;
}

static VideoError decode_raw_frame(VideoHandle_t handle, uint32_t offset, uint32_t size) {
    FIL* file = &handle->file;
    
    FRESULT res = f_lseek(file, offset);
    if (res != FR_OK) {
        return VIDEO_ERROR_FILE_READ;
    }
    
    uint16_t width = handle->info.width;
    uint16_t height = handle->info.height;
    uint16_t display_x = handle->display_x;
    uint16_t display_y = handle->display_y;
    bool need_swap = handle->needs_byte_swap;
    
    ST7735_Select();
    ST7735_SetAddressWindow(display_x, display_y, display_x + width - 1, display_y + height - 1);
    ST7735_DC_HIGH();
    
    uint16_t row_buffer_size = width * 2;
    
    uint8_t* buffer_a = (uint8_t*)malloc(row_buffer_size);
    uint8_t* buffer_b = (uint8_t*)malloc(row_buffer_size);
    if (!buffer_a || !buffer_b) {
        if (buffer_a) free(buffer_a);
        if (buffer_b) free(buffer_b);
        ST7735_Unselect();
        return VIDEO_ERROR_MEMORY_ALLOC;
    }
    
    UINT br;
    res = f_read(file, buffer_a, row_buffer_size, &br);
    if (res != FR_OK || br != row_buffer_size) {
        free(buffer_a);
        free(buffer_b);
        ST7735_Unselect();
        return VIDEO_ERROR_FILE_READ;
    }
    
    if (need_swap) {
        for (uint16_t x = 0; x < width; x++) {
            uint8_t tmp = buffer_a[x * 2];
            buffer_a[x * 2] = buffer_a[x * 2 + 1];
            buffer_a[x * 2 + 1] = tmp;
        }
    }
    
    for (uint16_t y = 0; y < height; y++) {
        uint8_t* current_buf = (y % 2 == 0) ? buffer_a : buffer_b;
        uint8_t* next_buf = (y % 2 == 0) ? buffer_b : buffer_a;
        
        HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, current_buf, row_buffer_size);
        
        if (y < height - 1) {
            res = f_read(file, next_buf, row_buffer_size, &br);
            if (res == FR_OK && br == row_buffer_size) {
                if (need_swap) {
                    for (uint16_t x = 0; x < width; x++) {
                        uint8_t tmp = next_buf[x * 2];
                        next_buf[x * 2] = next_buf[x * 2 + 1];
                        next_buf[x * 2 + 1] = tmp;
                    }
                }
            }
        }
        
        while (HAL_SPI_GetState(&ST7735_SPI_PORT) != HAL_SPI_STATE_READY);
        while (__HAL_SPI_GET_FLAG(&ST7735_SPI_PORT, SPI_FLAG_BSY));
    }
    
    free(buffer_a);
    free(buffer_b);
    ST7735_Unselect();
    
    return VIDEO_SUCCESS;
}

static size_t video_jpeg_input_func(JDEC* jd, uint8_t* buf, size_t nbyte) {
    VideoJpegContext* ctx = (VideoJpegContext*)jd->device;
    
    uint32_t current_offset = f_tell(ctx->file);
    if (current_offset >= ctx->frame_end_offset) {
        return 0;
    }
    
    uint32_t remaining = ctx->frame_end_offset - current_offset;
    size_t bytes_to_read = (nbyte > remaining) ? remaining : nbyte;
    
    UINT bytes_read;
    FRESULT res = f_read(ctx->file, buf, bytes_to_read, &bytes_read);
    if (res != FR_OK) {
        return 0;
    }
    return bytes_read;
}

static int video_jpeg_output_func(JDEC* jd, void* bitmap, JRECT* rect) {
    VideoJpegContext* ctx = (VideoJpegContext*)jd->device;
    
    uint16_t x = ctx->display_x + rect->left;
    uint16_t y = ctx->display_y + rect->top;
    uint16_t w = rect->right - rect->left + 1;
    uint16_t h = rect->bottom - rect->top + 1;
    
    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    ST7735_DC_HIGH();
    
    uint16_t* src = (uint16_t*)bitmap;
    uint32_t pixel_count = w * h;
    
    uint16_t* temp_buffer = (uint16_t*)malloc(pixel_count * sizeof(uint16_t));
    if (!temp_buffer) {
        ST7735_Unselect();
        return 0;
    }
    
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint16_t pixel = src[i];
        temp_buffer[i] = ((pixel & 0xFF00) >> 8) | ((pixel & 0xFF) << 8);
    }
    
    // 使用DMA传输
    HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, (uint8_t*)temp_buffer, pixel_count * sizeof(uint16_t));
    while (HAL_SPI_GetState(&ST7735_SPI_PORT) != HAL_SPI_STATE_READY);
    while (__HAL_SPI_GET_FLAG(&ST7735_SPI_PORT, SPI_FLAG_BSY));
    
    free(temp_buffer);
    ST7735_Unselect();
    
    return 1;
}

static uint32_t get_tick_ms(void) {
    extern uint32_t HAL_GetTick(void);
    return HAL_GetTick();
}
