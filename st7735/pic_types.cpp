//
// 动态图片类型实现
// 支持从SD卡动态加载和显示图片
//

#include "pic_types.h"
#include "st7735.h"
#include "fatfs.h"
#include <cstring>
#include <cmath>

// 外部声明
extern SPI_HandleTypeDef ST7735_SPI_PORT;

// JPEG解码相关结构
typedef struct {
    FIL* file;
    uint8_t* workbuf;
    uint16_t display_x;
    uint16_t display_y;
    uint16_t display_width;
    uint16_t display_height;
    uint8_t scale;
    uint16_t* pixel_data;      // 用于load_jpeg的目标缓冲区
} JpegContext;

// 全局变量
static PicError g_last_error = PIC_SUCCESS;

// BMP文件头结构（简化版）
typedef struct __attribute__((packed)) {
    uint16_t signature;         // "BM"
    uint32_t file_size;         // 文件大小
    uint16_t reserved1;         // 保留
    uint16_t reserved2;         // 保留
    uint32_t data_offset;       // 数据偏移量
    uint32_t header_size;       // 信息头大小
    int32_t width;              // 宽度
    int32_t height;             // 高度
    uint16_t planes;            // 颜色平面数
    uint16_t bits_per_pixel;    // 每像素位数
    uint32_t compression;       // 压缩方式
    uint32_t image_size;        // 图像数据大小
    int32_t x_pixels_per_meter; // 水平分辨率
    int32_t y_pixels_per_meter; // 垂直分辨率
    uint32_t colors_used;       // 使用的颜色数
    uint32_t colors_important;  // 重要颜色数
} BMPHeader;

// 内部函数声明
static PicError detect_image_format(const char* filename, PicFormat* format);
static PicError load_raw_565(PicHandle_t handle, FIL* file);
static PicError load_bmp(PicHandle_t handle, FIL* file);
static PicError load_jpeg(PicHandle_t handle, FIL* file);
static PicError decode_bmp_data(PicHandle_t handle, FIL* file, uint32_t data_offset, uint32_t row_size, uint16_t bits_per_pixel);
static PicError display_bmp_streaming(FIL* file, uint32_t data_offset, uint16_t bits_per_pixel,
                                     uint16_t img_width, uint16_t img_height,
                                     uint16_t display_x, uint16_t display_y,
                                     uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h);

// JPEG解码内部函数
static size_t jpeg_input_func(JDEC* jd, uint8_t* buf, size_t nbyte);
static int jpeg_output_func(JDEC* jd, void* bitmap, JRECT* rect);
static int jpeg_output_func_mem(JDEC* jd, void* bitmap, JRECT* rect);
static bool is_bmp_file(const uint8_t* header);
static uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b);

// 错误信息字符串
static const char* error_strings[] = {
    "成功",
    "文件未找到",
    "文件打开失败",
    "文件读取失败",
    "无效的格式",
    "内存分配失败",
    "无效的参数",
    "不支持的格式",
    "解码失败"
};

PicError PIC_Init(void) {
    g_last_error = PIC_SUCCESS;
    return PIC_SUCCESS;
}

void PIC_Deinit(void) {
    // 目前没有需要释放的全局资源
}

PicError PIC_LoadFromSD(const char* filename, PicHandle_t* handle) {
    if (!filename || !handle) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    FIL file;
    FRESULT res;
    PicFormat format;
    
    // 检测图片格式
    PicError error = detect_image_format(filename, &format);
    if (error != PIC_SUCCESS) {
        g_last_error = error;
        return error;
    }
    
    // 打开文件
    res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        g_last_error = (res == FR_NO_FILE) ? PIC_ERROR_FILE_NOT_FOUND : PIC_ERROR_FILE_OPEN;
        return g_last_error;
    }
    
    // 分配句柄内存
    PicHandle* pic_handle = (PicHandle*)malloc(sizeof(PicHandle));
    if (!pic_handle) {
        f_close(&file);
        g_last_error = PIC_ERROR_MEMORY_ALLOC;
        return g_last_error;
    }
    
    // 初始化句柄
    memset(pic_handle, 0, sizeof(PicHandle));
    strncpy(pic_handle->info.filename, filename, sizeof(pic_handle->info.filename) - 1);
    pic_handle->info.format = format;
    pic_handle->info.file_size = f_size(&file);
    
    // 根据格式加载图片
    switch (format) {
        case PIC_FORMAT_RAW_565:
            error = load_raw_565(pic_handle, &file);
            break;
        case PIC_FORMAT_BMP:
            error = load_bmp(pic_handle, &file);
            break;
        case PIC_FORMAT_JPEG:
            error = load_jpeg(pic_handle, &file);
            break;
        case PIC_FORMAT_PNG:
            error = PIC_ERROR_UNSUPPORTED_FORMAT;
            break;
        default:
            error = PIC_ERROR_INVALID_FORMAT;
            break;
    }
    
    f_close(&file);
    
    if (error != PIC_SUCCESS) {
        free(pic_handle);
        g_last_error = error;
        return error;
    }
    
    pic_handle->is_loaded = true;
    *handle = pic_handle;
    g_last_error = PIC_SUCCESS;
    return PIC_SUCCESS;
}

void PIC_Free(PicHandle_t handle) {
    if (!handle) return;
    
    if (handle->pixel_data) {
        free(handle->pixel_data);
    }
    
    free(handle);
}

PicError PIC_GetInfo(PicHandle_t handle, PicInfo* info) {
    if (!handle || !info) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    if (!handle->is_loaded) {
        g_last_error = PIC_ERROR_INVALID_FORMAT;
        return g_last_error;
    }
    
    *info = handle->info;
    g_last_error = PIC_SUCCESS;
    return PIC_SUCCESS;
}

PicError PIC_ParseInfo(const char* filename, PicInfo* info) {
    if (!filename || !info) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    FIL file;
    FRESULT res;
    PicFormat format;
    
    PicError error = detect_image_format(filename, &format);
    if (error != PIC_SUCCESS) {
        g_last_error = error;
        return error;
    }
    
    res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        g_last_error = (res == FR_NO_FILE) ? PIC_ERROR_FILE_NOT_FOUND : PIC_ERROR_FILE_OPEN;
        return g_last_error;
    }
    
    memset(info, 0, sizeof(PicInfo));
    strncpy(info->filename, filename, sizeof(info->filename) - 1);
    info->format = format;
    info->file_size = f_size(&file);
    
    switch (format) {
        case PIC_FORMAT_BMP: {
            BMPHeader header;
            UINT bytes_read;
            res = f_read(&file, &header, sizeof(BMPHeader), &bytes_read);
            if (res != FR_OK || bytes_read != sizeof(BMPHeader)) {
                f_close(&file);
                g_last_error = PIC_ERROR_FILE_READ;
                return g_last_error;
            }
            if (!is_bmp_file((uint8_t*)&header)) {
                f_close(&file);
                g_last_error = PIC_ERROR_INVALID_FORMAT;
                return g_last_error;
            }
            info->width = (uint16_t)abs(header.width);
            info->height = (uint16_t)abs(header.height);
            info->data_offset = header.data_offset;
            break;
        }
        case PIC_FORMAT_JPEG: {
            uint8_t* workbuf = (uint8_t*)malloc(PIC_TJPGDEC_WORKSPACE);
            if (!workbuf) {
                f_close(&file);
                g_last_error = PIC_ERROR_MEMORY_ALLOC;
                return g_last_error;
            }
            
            JDEC jdec;
            JpegContext ctx;
            ctx.file = &file;
            
            JRESULT jres = jd_prepare(&jdec, jpeg_input_func, workbuf, PIC_TJPGDEC_WORKSPACE, &ctx);
            free(workbuf);
            
            if (jres != JDR_OK) {
                f_close(&file);
                g_last_error = PIC_ERROR_DECODE_FAILED;
                return g_last_error;
            }
            
            info->width = jdec.width;
            info->height = jdec.height;
            break;
        }
        case PIC_FORMAT_RAW_565: {
            uint32_t file_size = f_size(&file);
            uint32_t pixels = file_size / 2;
            uint16_t sqrt_pixels = (uint16_t)sqrt((double)pixels);
            info->width = sqrt_pixels;
            info->height = sqrt_pixels;
            break;
        }
        default:
            f_close(&file);
            g_last_error = PIC_ERROR_UNSUPPORTED_FORMAT;
            return g_last_error;
    }
    
    f_close(&file);
    g_last_error = PIC_SUCCESS;
    return PIC_SUCCESS;
}

PicError PIC_Display(PicHandle_t handle, uint16_t x, uint16_t y) {
    if (!handle || !handle->is_loaded || !handle->pixel_data) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    ST7735_DrawImage(x, y, handle->info.width, handle->info.height, handle->pixel_data);
    
    g_last_error = PIC_SUCCESS;
    return PIC_SUCCESS;
}

PicError PIC_DisplayDMA(PicHandle_t handle, uint16_t x, uint16_t y) {
    if (!handle || !handle->is_loaded || !handle->pixel_data) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    if((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    if((x + handle->info.width - 1) >= ST7735_WIDTH) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    if((y + handle->info.height - 1) >= ST7735_HEIGHT) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }

    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x + handle->info.width - 1, y + handle->info.height - 1);
    
    ST7735_DC_HIGH();
    HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, (uint8_t*)handle->pixel_data, handle->data_size);
    while (HAL_SPI_GetState(&ST7735_SPI_PORT) != HAL_SPI_STATE_READY);
    while (__HAL_SPI_GET_FLAG(&ST7735_SPI_PORT, SPI_FLAG_BSY));
    
    ST7735_Unselect();
    
    g_last_error = PIC_SUCCESS;
    return PIC_SUCCESS;
}

PicError PIC_DisplayScaled(PicHandle_t handle, uint16_t x, uint16_t y, float scale) {
    if (!handle || !handle->is_loaded || !handle->pixel_data || scale <= 0) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    // 简化实现：只支持整数缩放
    uint16_t scaled_width = (uint16_t)(handle->info.width * scale);
    uint16_t scaled_height = (uint16_t)(handle->info.height * scale);
    
    if (scaled_width == 0 || scaled_height == 0) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    // 简单的最近邻缩放算法
    uint16_t* scaled_data = (uint16_t*)malloc(scaled_width * scaled_height * sizeof(uint16_t));
    if (!scaled_data) {
        g_last_error = PIC_ERROR_MEMORY_ALLOC;
        return g_last_error;
    }
    
    for (uint16_t sy = 0; sy < scaled_height; sy++) {
        for (uint16_t sx = 0; sx < scaled_width; sx++) {
            uint16_t src_x = (uint16_t)(sx / scale);
            uint16_t src_y = (uint16_t)(sy / scale);
            
            if (src_x < handle->info.width && src_y < handle->info.height) {
                scaled_data[sy * scaled_width + sx] = 
                    handle->pixel_data[src_y * handle->info.width + src_x];
            }
        }
    }
    
    // 显示缩放后的图片
    ST7735_DrawImage(x, y, scaled_width, scaled_height, scaled_data);
    
    free(scaled_data);
    g_last_error = PIC_SUCCESS;
    return PIC_SUCCESS;
}

PicError PIC_DisplayRegion(PicHandle_t handle, uint16_t x, uint16_t y, 
                          uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h) {
    if (!handle || !handle->is_loaded || !handle->pixel_data) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    // 检查区域是否有效
    if (src_x >= handle->info.width || src_y >= handle->info.height ||
        src_w == 0 || src_h == 0 ||
        src_x + src_w > handle->info.width || src_y + src_h > handle->info.height) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    // 提取区域数据
    uint16_t* region_data = (uint16_t*)malloc(src_w * src_h * sizeof(uint16_t));
    if (!region_data) {
        g_last_error = PIC_ERROR_MEMORY_ALLOC;
        return g_last_error;
    }
    
    for (uint16_t ry = 0; ry < src_h; ry++) {
        for (uint16_t rx = 0; rx < src_w; rx++) {
            uint16_t src_pixel_x = src_x + rx;
            uint16_t src_pixel_y = src_y + ry;
            region_data[ry * src_w + rx] = 
                handle->pixel_data[src_pixel_y * handle->info.width + src_pixel_x];
        }
    }
    
    // 显示区域
    ST7735_DrawImage(x, y, src_w, src_h, region_data);
    
    free(region_data);
    g_last_error = PIC_SUCCESS;
    return PIC_SUCCESS;
}

PicError PIC_DisplayRawData(const uint16_t* data, uint16_t width, uint16_t height, 
                           uint16_t x, uint16_t y) {
    if (!data || width == 0 || height == 0) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    ST7735_DrawImage(x, y, width, height, data);
    g_last_error = PIC_SUCCESS;
    return PIC_SUCCESS;
}

bool PIC_IsSupportedFormat(const char* filename) {
    if (!filename) return false;
    
    PicFormat format;
    return detect_image_format(filename, &format) == PIC_SUCCESS;
}

const char* PIC_GetErrorString(PicError error) {
    if (error < 0 || error >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "未知错误";
    }
    return error_strings[error];
}

PicError PIC_GetLastError(void) {
    return g_last_error;
}

// 内部函数实现

static PicError detect_image_format(const char* filename, PicFormat* format) {
    if (!filename || !format) {
        return PIC_ERROR_INVALID_PARAM;
    }
    
    // 根据文件扩展名检测格式
    const char* ext = strrchr(filename, '.');
    if (!ext) {
        return PIC_ERROR_INVALID_FORMAT;
    }
    
    if (strcasecmp(ext, ".raw") == 0 || strcasecmp(ext, ".565") == 0) {
        *format = PIC_FORMAT_RAW_565;
    } else if (strcasecmp(ext, ".bmp") == 0) {
        *format = PIC_FORMAT_BMP;
    } else if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) {
        *format = PIC_FORMAT_JPEG;
    } else if (strcasecmp(ext, ".png") == 0) {
        *format = PIC_FORMAT_PNG;
    } else {
        return PIC_ERROR_UNSUPPORTED_FORMAT;
    }
    
    return PIC_SUCCESS;
}

static PicError load_raw_565(PicHandle_t handle, FIL* file) {
    // 对于RAW格式，假设文件大小就是图片数据大小
    uint32_t file_size = f_size(file);
    
    // 计算图片尺寸（假设是正方形或根据文件大小推断）
    uint32_t pixel_count = file_size / sizeof(uint16_t);
    uint16_t width = 1;
    uint16_t height = 1;
    
    // 尝试推断合理的尺寸
    for (uint16_t w = 1; w <= 320; w++) {
        if (pixel_count % w == 0) {
            uint16_t h = pixel_count / w;
            if (h <= 240) {  // 假设最大高度为240
                width = w;
                height = h;
                break;
            }
        }
    }
    
    handle->info.width = width;
    handle->info.height = height;
    handle->data_size = file_size;
    
    // 分配内存
    handle->pixel_data = (uint16_t*)malloc(file_size);
    if (!handle->pixel_data) {
        return PIC_ERROR_MEMORY_ALLOC;
    }
    
    // 读取数据
    UINT bytes_read;
    FRESULT res = f_read(file, handle->pixel_data, file_size, &bytes_read);
    if (res != FR_OK || bytes_read != file_size) {
        free(handle->pixel_data);
        handle->pixel_data = nullptr;
        return PIC_ERROR_FILE_READ;
    }
    
    return PIC_SUCCESS;
}

static PicError load_bmp(PicHandle_t handle, FIL* file) {
    BMPHeader header;
    UINT bytes_read;
    
    // 读取BMP文件头
    FRESULT res = f_read(file, &header, sizeof(BMPHeader), &bytes_read);
    if (res != FR_OK || bytes_read != sizeof(BMPHeader)) {
        return PIC_ERROR_FILE_READ;
    }
    
    // 验证BMP文件
    if (!is_bmp_file((uint8_t*)&header)) {
        return PIC_ERROR_INVALID_FORMAT;
    }
    
    // 检查是否支持
    if (header.bits_per_pixel != 24 && header.bits_per_pixel != 32) {
        return PIC_ERROR_UNSUPPORTED_FORMAT;
    }
    
    handle->info.width = (uint16_t)abs(header.width);
    handle->info.height = (uint16_t)abs(header.height);
    handle->info.data_offset = header.data_offset;
    
    uint32_t row_size = ((handle->info.width * header.bits_per_pixel + 31) / 32) * 4;
    handle->data_size = handle->info.width * handle->info.height * sizeof(uint16_t);
    
    PicError error = decode_bmp_data(handle, file, header.data_offset, row_size, header.bits_per_pixel);
    
    return error;
}

static PicError decode_bmp_data(PicHandle_t handle, FIL* file, uint32_t data_offset, uint32_t row_size, uint16_t bits_per_pixel) {
    if (!file || row_size == 0) {
        return PIC_ERROR_INVALID_PARAM;
    }
    
    // 分配RGB565数据内存
    uint32_t pixel_count = handle->info.width * handle->info.height;
    handle->pixel_data = (uint16_t*)malloc(pixel_count * sizeof(uint16_t));
    if (!handle->pixel_data) {
        return PIC_ERROR_MEMORY_ALLOC;
    }
    
    // 每个像素的字节数
    uint8_t bytes_per_pixel = bits_per_pixel / 8;
    
    // 分配行缓冲区（只存储一行BMP数据）
    uint8_t* row_buffer = (uint8_t*)malloc(row_size);
    if (!row_buffer) {
        free(handle->pixel_data);
        handle->pixel_data = nullptr;
        return PIC_ERROR_MEMORY_ALLOC;
    }
    
    FRESULT res;
    UINT bytes_read;
    
    // 逐行读取并转换
    for (uint16_t y = 0; y < handle->info.height; y++) {
        // BMP数据从下到上存储
        uint16_t src_y = handle->info.height - 1 - y;
        
        // 定位到源行
        res = f_lseek(file, data_offset + src_y * row_size);
        if (res != FR_OK) {
            free(row_buffer);
            free(handle->pixel_data);
            handle->pixel_data = nullptr;
            return PIC_ERROR_FILE_READ;
        }
        
        // 读取一行BMP数据
        res = f_read(file, row_buffer, row_size, &bytes_read);
        if (res != FR_OK || bytes_read != row_size) {
            free(row_buffer);
            free(handle->pixel_data);
            handle->pixel_data = nullptr;
            return PIC_ERROR_FILE_READ;
        }
        
        // 转换该行的每个像素
        for (uint16_t x = 0; x < handle->info.width; x++) {
            const uint8_t* pixel = row_buffer + x * bytes_per_pixel;
            
            // BMP存储顺序是BGR（24位）或BGRA（32位）
            uint8_t b = pixel[0];
            uint8_t g = pixel[1];
            uint8_t r = pixel[2];
            
            // 转换为RGB565
            handle->pixel_data[y * handle->info.width + x] = rgb888_to_565(r, g, b);
        }
    }
    
    // 释放行缓冲区
    free(row_buffer);
    
    return PIC_SUCCESS;
}

static bool is_bmp_file(const uint8_t* header) {
    // 检查BMP文件签名 "BM"
    return header[0] == 'B' && header[1] == 'M';
}

static PicError load_jpeg(PicHandle_t handle, FIL* file) {
    // 分配JPEG解码工作缓冲区
    // TJpgDec需要的工作缓冲区大小取决于JPEG文件的MCU大小和配置
    uint8_t* workbuf = (uint8_t*)malloc(PIC_TJPGDEC_WORKSPACE);
    if (!workbuf) {
        return PIC_ERROR_MEMORY_ALLOC;
    }
    
    // 初始化JPEG解码器
    JDEC jdec;
    JpegContext ctx;
    ctx.file = file;
    ctx.workbuf = workbuf;
    
    // 准备JPEG解码
    JRESULT jres = jd_prepare(&jdec, jpeg_input_func, workbuf, PIC_TJPGDEC_WORKSPACE, &ctx);
    if (jres != JDR_OK) {
        free(workbuf);
        return PIC_ERROR_DECODE_FAILED;
    }
    
    // 获取图片尺寸
    handle->info.width = jdec.width;
    handle->info.height = jdec.height;
    handle->data_size = jdec.width * jdec.height * sizeof(uint16_t);
    
    // 分配RGB565数据内存
    handle->pixel_data = (uint16_t*)malloc(handle->data_size);
    if (!handle->pixel_data) {
        free(workbuf);
        return PIC_ERROR_MEMORY_ALLOC;
    }
    
    // 设置目标缓冲区指针
    ctx.pixel_data = handle->pixel_data;
    ctx.display_width = jdec.width;
    
    // 解码JPEG到内存
    jres = jd_decomp(&jdec, jpeg_output_func_mem, 0);
    
    free(workbuf);
    
    if (jres != JDR_OK) {
        free(handle->pixel_data);
        handle->pixel_data = nullptr;
        return PIC_ERROR_DECODE_FAILED;
    }
    
    return PIC_SUCCESS;
}

static int jpeg_output_func_mem(JDEC* jd, void* bitmap, JRECT* rect) {
    JpegContext* ctx = (JpegContext*)jd->device;
    
    uint16_t* src = (uint16_t*)bitmap;
    uint16_t w = rect->right - rect->left + 1;
    uint16_t h = rect->bottom - rect->top + 1;
    
    for (uint16_t y = 0; y < h; y++) {
        uint16_t dst_y = rect->top + y;
        for (uint16_t x = 0; x < w; x++) {
            uint16_t dst_x = rect->left + x;
            uint16_t dst_idx = dst_y * ctx->display_width + dst_x;
            uint16_t pixel = src[y * w + x];
            ctx->pixel_data[dst_idx] = ((pixel & 0xFF00) >> 8) | ((pixel & 0xFF) << 8);
        }
    }
    
    return 1;
}

static uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    color565 = ((color565 & 0xFF00) >> 8) | ((color565 & 0xFF) << 8);
    return color565;
}

PicError PIC_DisplayStreaming(const char* filename, uint16_t x, uint16_t y,
                            uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h) {
    if (!filename) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    FIL file;
    FRESULT res;
    PicFormat format;
    
    // 检测图片格式
    PicError error = detect_image_format(filename, &format);
    if (error != PIC_SUCCESS) {
        g_last_error = error;
        return error;
    }
    
    // 打开文件
    res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        g_last_error = (res == FR_NO_FILE) ? PIC_ERROR_FILE_NOT_FOUND : PIC_ERROR_FILE_OPEN;
        return g_last_error;
    }
    
    // 根据格式处理
    switch (format) {
        case PIC_FORMAT_BMP: {
            BMPHeader header;
            UINT bytes_read;
            
            // 读取BMP文件头
            res = f_read(&file, &header, sizeof(BMPHeader), &bytes_read);
            if (res != FR_OK || bytes_read != sizeof(BMPHeader)) {
                f_close(&file);
                g_last_error = PIC_ERROR_FILE_READ;
                return g_last_error;
            }
            
            // 验证BMP文件
            if (!is_bmp_file((uint8_t*)&header)) {
                f_close(&file);
                g_last_error = PIC_ERROR_INVALID_FORMAT;
                return g_last_error;
            }
            
            // 检查是否支持
            if (header.bits_per_pixel != 24 && header.bits_per_pixel != 32) {
                f_close(&file);
                g_last_error = PIC_ERROR_UNSUPPORTED_FORMAT;
                return g_last_error;
            }
            
            // 获取图片尺寸
            uint16_t img_width = (uint16_t)abs(header.width);
            uint16_t img_height = (uint16_t)abs(header.height);
            
            // 处理默认参数（0表示使用图片的完整尺寸）
            if (src_w == 0) src_w = img_width - src_x;
            if (src_h == 0) src_h = img_height - src_y;
            
            // 验证区域参数
            if (src_x >= img_width || src_y >= img_height ||
                src_w == 0 || src_h == 0 ||
                src_x + src_w > img_width || src_y + src_h > img_height) {
                f_close(&file);
                g_last_error = PIC_ERROR_INVALID_PARAM;
                return g_last_error;
            }
            
            // 调用流式显示函数
            error = display_bmp_streaming(&file, header.data_offset, header.bits_per_pixel,
                                         img_width, img_height,
                                         x, y, src_x, src_y, src_w, src_h);
            break;
        }
        case PIC_FORMAT_JPEG: {
            // JPEG使用缩放参数（src_w作为scale参数）
            // TJpgDec缩放参数定义：
            //   0 = 1/1（原始大小）
            //   1 = 1/2
            //   2 = 1/4
            //   3 = 1/8
            // 如果src_w > 3，则使用原始大小（scale=0）
            uint8_t scale = (src_w > 3) ? 0 : src_w;
            
            // 分配JPEG解码工作缓冲区
            uint8_t* workbuf = (uint8_t*)malloc(PIC_TJPGDEC_WORKSPACE);
            if (!workbuf) {
                f_close(&file);
                g_last_error = PIC_ERROR_MEMORY_ALLOC;
                return g_last_error;
            }
            
            // 初始化JPEG解码器
            JDEC jdec;
            JpegContext ctx;
            ctx.file = &file;
            ctx.workbuf = workbuf;
            ctx.display_x = x;
            ctx.display_y = y;
            ctx.scale = scale;
            
            // 准备JPEG解码
            JRESULT jres = jd_prepare(&jdec, jpeg_input_func, workbuf, PIC_TJPGDEC_WORKSPACE, &ctx);
            if (jres != JDR_OK) {
                free(workbuf);
                f_close(&file);
                g_last_error = PIC_ERROR_DECODE_FAILED;
                return g_last_error;
            }
            
            // 计算显示尺寸（考虑缩放）
            // scale: 0=1/1, 1=1/2, 2=1/4, 3=1/8
            uint16_t scale_factor = 1 << scale;
            ctx.display_width = (jdec.width + scale_factor - 1) / scale_factor;
            ctx.display_height = (jdec.height + scale_factor - 1) / scale_factor;
            
            // 检查是否超出LCD范围
            if (x + ctx.display_width > ST7735_WIDTH || y + ctx.display_height > ST7735_HEIGHT) {
                free(workbuf);
                f_close(&file);
                g_last_error = PIC_ERROR_INVALID_PARAM;
                return g_last_error;
            }
            
            // 解码JPEG并显示
            jres = jd_decomp(&jdec, jpeg_output_func, scale);
            
            free(workbuf);
            
            if (jres != JDR_OK) {
                g_last_error = PIC_ERROR_DECODE_FAILED;
                error = PIC_ERROR_DECODE_FAILED;
            } else {
                error = PIC_SUCCESS;
            }
            break;
        }
        case PIC_FORMAT_RAW_565:
        case PIC_FORMAT_PNG:
        default:
            f_close(&file);
            g_last_error = PIC_ERROR_UNSUPPORTED_FORMAT;
            return g_last_error;
    }
    
    f_close(&file);
    g_last_error = error;
    return error;
}

static PicError display_bmp_streaming(FIL* file, uint32_t data_offset, uint16_t bits_per_pixel,
                                     uint16_t img_width, uint16_t img_height,
                                     uint16_t display_x, uint16_t display_y,
                                     uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h) {
    if (!file || src_w == 0 || src_h == 0) {
        return PIC_ERROR_INVALID_PARAM;
    }
    
    // 计算BMP行大小（需要4字节对齐）
    uint32_t row_size = ((img_width * bits_per_pixel + 31) / 32) * 4;
    
    // 每个像素的字节数
    uint8_t bytes_per_pixel = bits_per_pixel / 8;
    
    // 分配BMP行缓冲区
    uint8_t* bmp_row_buffer = (uint8_t*)malloc(row_size);
    if (!bmp_row_buffer) {
        return PIC_ERROR_MEMORY_ALLOC;
    }
    
    // 分配RGB565显示缓冲区（只分配要显示的宽度）
    uint16_t* display_buffer = (uint16_t*)malloc(src_w * sizeof(uint16_t));
    if (!display_buffer) {
        free(bmp_row_buffer);
        return PIC_ERROR_MEMORY_ALLOC;
    }
    
    FRESULT res;
    UINT bytes_read;
    
    // 设置LCD显示窗口
    ST7735_Select();
    ST7735_SetAddressWindow(display_x, display_y, display_x + src_w - 1, display_y + src_h - 1);
    ST7735_DC_HIGH();
    
    // 逐行读取并显示
    for (uint16_t dy = 0; dy < src_h; dy++) {
        // BMP数据从下到上存储
        uint16_t src_y_abs = src_y + (src_h - 1 - dy);
        
        // 定位到源行
        res = f_lseek(file, data_offset + src_y_abs * row_size);
        if (res != FR_OK) {
            free(bmp_row_buffer);
            free(display_buffer);
            return PIC_ERROR_FILE_READ;
        }
        
        // 读取一行BMP数据
        res = f_read(file, bmp_row_buffer, row_size, &bytes_read);
        if (res != FR_OK || bytes_read != row_size) {
            free(bmp_row_buffer);
            free(display_buffer);
            return PIC_ERROR_FILE_READ;
        }
        
        // 转换要显示的区域
        for (uint16_t dx = 0; dx < src_w; dx++) {
            uint16_t src_x_abs = src_x + dx;
            const uint8_t* pixel = bmp_row_buffer + src_x_abs * bytes_per_pixel;
            
            // BMP存储顺序是BGR（24位）或BGRA（32位）
            uint8_t b = pixel[0];
            uint8_t g = pixel[1];
            uint8_t r = pixel[2];
            
            // 转换为RGB565
            display_buffer[dx] = rgb888_to_565(r, g, b);
        }
        
        // 发送一行到LCD
        HAL_SPI_Transmit(&ST7735_SPI_PORT, (uint8_t*)display_buffer, src_w * sizeof(uint16_t), HAL_MAX_DELAY);
    }
    
    ST7735_Unselect();
    
    // 释放缓冲区
    free(bmp_row_buffer);
    free(display_buffer);
    
    ST7735_Unselect();
    
    return PIC_SUCCESS;
}

// JPEG输入函数：从文件读取数据
static size_t jpeg_input_func(JDEC* jd, uint8_t* buf, size_t nbyte) {
    JpegContext* ctx = (JpegContext*)jd->device;
    UINT bytes_read;
    FRESULT res = f_read(ctx->file, buf, nbyte, &bytes_read);
    if (res != FR_OK) {
        return 0;
    }
    return bytes_read;
}

// JPEG输出函数：将解码后的数据发送到LCD
static int jpeg_output_func(JDEC* jd, void* bitmap, JRECT* rect) {
    JpegContext* ctx = (JpegContext*)jd->device;
    
    // 计算显示区域
    uint16_t x = ctx->display_x + rect->left;
    uint16_t y = ctx->display_y + rect->top;
    uint16_t w = rect->right - rect->left + 1;
    uint16_t h = rect->bottom - rect->top + 1;
    
    // 设置LCD显示窗口
    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    ST7735_DC_HIGH();
    
    // TJpgDec输出格式为RGB565（由tjpgdcnf.h中的JD_FORMAT=1配置）
    // 需要字节交换以适配ST7735的SPI传输格式
    uint16_t* src = (uint16_t*)bitmap;
    uint32_t pixel_count = w * h;
    
    // 分配临时缓冲区用于字节交换
    uint16_t* temp_buffer = (uint16_t*)malloc(pixel_count * sizeof(uint16_t));
    if (!temp_buffer) {
        ST7735_Unselect();
        return 0;
    }
    
    // 字节交换
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint16_t pixel = src[i];
        temp_buffer[i] = ((pixel & 0xFF00) >> 8) | ((pixel & 0xFF) << 8);
    }
    
    // 批量发送
    HAL_SPI_Transmit(&ST7735_SPI_PORT, (uint8_t*)temp_buffer, pixel_count * sizeof(uint16_t), HAL_MAX_DELAY);
    
    free(temp_buffer);
    ST7735_Unselect();
    
    return 1;
}

static PicError display_bmp_streaming_dma(FIL* file, uint32_t data_offset, uint16_t bits_per_pixel,
                                          uint16_t img_width, uint16_t img_height,
                                          uint16_t display_x, uint16_t display_y,
                                          uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h) {
    if (!file || src_w == 0 || src_h == 0) {
        return PIC_ERROR_INVALID_PARAM;
    }
    
    uint32_t row_size = ((img_width * bits_per_pixel + 31) / 32) * 4;
    uint8_t bytes_per_pixel = bits_per_pixel / 8;
    
    uint8_t* bmp_row_buffer = (uint8_t*)malloc(row_size);
    if (!bmp_row_buffer) {
        return PIC_ERROR_MEMORY_ALLOC;
    }
    
    uint16_t row_buffer_size = src_w * sizeof(uint16_t);
    uint16_t* display_buffer_a = (uint16_t*)malloc(row_buffer_size);
    uint16_t* display_buffer_b = (uint16_t*)malloc(row_buffer_size);
    
    if (!display_buffer_a || !display_buffer_b) {
        free(bmp_row_buffer);
        if (display_buffer_a) free(display_buffer_a);
        if (display_buffer_b) free(display_buffer_b);
        return PIC_ERROR_MEMORY_ALLOC;
    }
    
    FRESULT res;
    UINT bytes_read;
    
    ST7735_Select();
    ST7735_SetAddressWindow(display_x, display_y, display_x + src_w - 1, display_y + src_h - 1);
    ST7735_DC_HIGH();
    
    uint16_t src_y_abs = src_y + (src_h - 1);
    res = f_lseek(file, data_offset + src_y_abs * row_size);
    if (res != FR_OK) {
        free(bmp_row_buffer);
        free(display_buffer_a);
        free(display_buffer_b);
        ST7735_Unselect();
        return PIC_ERROR_FILE_READ;
    }
    
    res = f_read(file, bmp_row_buffer, row_size, &bytes_read);
    if (res != FR_OK || bytes_read != row_size) {
        free(bmp_row_buffer);
        free(display_buffer_a);
        free(display_buffer_b);
        ST7735_Unselect();
        return PIC_ERROR_FILE_READ;
    }
    
    for (uint16_t dx = 0; dx < src_w; dx++) {
        uint16_t src_x_abs = src_x + dx;
        const uint8_t* pixel = bmp_row_buffer + src_x_abs * bytes_per_pixel;
        uint8_t b = pixel[0];
        uint8_t g = pixel[1];
        uint8_t r = pixel[2];
        display_buffer_a[dx] = rgb888_to_565(r, g, b);
    }
    
    for (uint16_t dy = 0; dy < src_h; dy++) {
        uint16_t* current_buf = (dy % 2 == 0) ? display_buffer_a : display_buffer_b;
        uint16_t* next_buf = (dy % 2 == 0) ? display_buffer_b : display_buffer_a;
        
        HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, (uint8_t*)current_buf, row_buffer_size);
        
        if (dy < src_h - 1) {
            uint16_t src_y_next = src_y + (src_h - 1 - dy - 1);
            res = f_lseek(file, data_offset + src_y_next * row_size);
            if (res == FR_OK) {
                res = f_read(file, bmp_row_buffer, row_size, &bytes_read);
                if (res == FR_OK && bytes_read == row_size) {
                    for (uint16_t dx = 0; dx < src_w; dx++) {
                        uint16_t src_x_abs = src_x + dx;
                        const uint8_t* pixel = bmp_row_buffer + src_x_abs * bytes_per_pixel;
                        uint8_t b = pixel[0];
                        uint8_t g = pixel[1];
                        uint8_t r = pixel[2];
                        next_buf[dx] = rgb888_to_565(r, g, b);
                    }
                }
            }
        }
        
        while (HAL_SPI_GetState(&ST7735_SPI_PORT) != HAL_SPI_STATE_READY);
        while (__HAL_SPI_GET_FLAG(&ST7735_SPI_PORT, SPI_FLAG_BSY));
    }
    
    free(bmp_row_buffer);
    free(display_buffer_a);
    free(display_buffer_b);
    ST7735_Unselect();
    
    return PIC_SUCCESS;
}

static int jpeg_output_func_dma(JDEC* jd, void* bitmap, JRECT* rect) {
    JpegContext* ctx = (JpegContext*)jd->device;
    
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
    
    HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, (uint8_t*)temp_buffer, pixel_count * sizeof(uint16_t));
    while (HAL_SPI_GetState(&ST7735_SPI_PORT) != HAL_SPI_STATE_READY);
    while (__HAL_SPI_GET_FLAG(&ST7735_SPI_PORT, SPI_FLAG_BSY));
    
    free(temp_buffer);
    ST7735_Unselect();
    
    return 1;
}

PicError PIC_DisplayStreamingDMA(const char* filename, uint16_t x, uint16_t y,
                               uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h) {
    if (!filename) {
        g_last_error = PIC_ERROR_INVALID_PARAM;
        return g_last_error;
    }
    
    FIL file;
    FRESULT res;
    PicFormat format;
    
    PicError error = detect_image_format(filename, &format);
    if (error != PIC_SUCCESS) {
        g_last_error = error;
        return error;
    }
    
    res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        g_last_error = (res == FR_NO_FILE) ? PIC_ERROR_FILE_NOT_FOUND : PIC_ERROR_FILE_OPEN;
        return g_last_error;
    }
    
    switch (format) {
        case PIC_FORMAT_BMP: {
            BMPHeader header;
            UINT bytes_read;
            
            res = f_read(&file, &header, sizeof(BMPHeader), &bytes_read);
            if (res != FR_OK || bytes_read != sizeof(BMPHeader)) {
                f_close(&file);
                g_last_error = PIC_ERROR_FILE_READ;
                return g_last_error;
            }
            
            if (!is_bmp_file((uint8_t*)&header)) {
                f_close(&file);
                g_last_error = PIC_ERROR_INVALID_FORMAT;
                return g_last_error;
            }
            
            if (header.bits_per_pixel != 24 && header.bits_per_pixel != 32) {
                f_close(&file);
                g_last_error = PIC_ERROR_UNSUPPORTED_FORMAT;
                return g_last_error;
            }
            
            uint16_t img_width = (uint16_t)abs(header.width);
            uint16_t img_height = (uint16_t)abs(header.height);
            
            if (src_w == 0) src_w = img_width - src_x;
            if (src_h == 0) src_h = img_height - src_y;
            
            if (src_x >= img_width || src_y >= img_height ||
                src_w == 0 || src_h == 0 ||
                src_x + src_w > img_width || src_y + src_h > img_height) {
                f_close(&file);
                g_last_error = PIC_ERROR_INVALID_PARAM;
                return g_last_error;
            }
            
            error = display_bmp_streaming_dma(&file, header.data_offset, header.bits_per_pixel,
                                              img_width, img_height,
                                              x, y, src_x, src_y, src_w, src_h);
            break;
        }
        case PIC_FORMAT_JPEG: {
            uint8_t scale = (src_w > 3) ? 0 : src_w;
            
            uint8_t* workbuf = (uint8_t*)malloc(PIC_TJPGDEC_WORKSPACE);
            if (!workbuf) {
                f_close(&file);
                g_last_error = PIC_ERROR_MEMORY_ALLOC;
                return g_last_error;
            }
            
            JDEC jdec;
            JpegContext ctx;
            ctx.file = &file;
            ctx.workbuf = workbuf;
            ctx.display_x = x;
            ctx.display_y = y;
            ctx.scale = scale;
            
            JRESULT jres = jd_prepare(&jdec, jpeg_input_func, workbuf, PIC_TJPGDEC_WORKSPACE, &ctx);
            if (jres != JDR_OK) {
                free(workbuf);
                f_close(&file);
                g_last_error = PIC_ERROR_DECODE_FAILED;
                return g_last_error;
            }
            
            uint16_t scale_factor = 1 << scale;
            ctx.display_width = (jdec.width + scale_factor - 1) / scale_factor;
            ctx.display_height = (jdec.height + scale_factor - 1) / scale_factor;
            
            if (x + ctx.display_width > ST7735_WIDTH || y + ctx.display_height > ST7735_HEIGHT) {
                free(workbuf);
                f_close(&file);
                g_last_error = PIC_ERROR_INVALID_PARAM;
                return g_last_error;
            }
            
            jres = jd_decomp(&jdec, jpeg_output_func_dma, scale);
            
            free(workbuf);
            
            if (jres != JDR_OK) {
                g_last_error = PIC_ERROR_DECODE_FAILED;
                error = PIC_ERROR_DECODE_FAILED;
            } else {
                error = PIC_SUCCESS;
            }
            break;
        }
        case PIC_FORMAT_RAW_565:
        case PIC_FORMAT_PNG:
        default:
            f_close(&file);
            g_last_error = PIC_ERROR_UNSUPPORTED_FORMAT;
            return g_last_error;
    }
    
    f_close(&file);
    g_last_error = error;
    return error;
}