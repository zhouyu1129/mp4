//
// 动态图片类型定义和接口
// 支持从SD卡动态加载和显示图片
//

#ifndef SD_AND_LCD2_PIC_TYPES_H
#define SD_AND_LCD2_PIC_TYPES_H

#include "ff.h"
#include "tjpgd.h"

#ifdef __cplusplus
extern "C" {
#else
#include <stdint.h>
#endif

#define PIC_TJPGDEC_WORKSPACE 10000

// 图片格式定义
typedef enum {
    PIC_FORMAT_UNKNOWN = 0,
    PIC_FORMAT_RAW_565,      // 原始RGB565数据
    PIC_FORMAT_BMP,          // BMP格式
    PIC_FORMAT_JPEG,         // JPEG格式（需要解码）
    PIC_FORMAT_PNG           // PNG格式（需要解码）
} PicFormat;

// 图片信息结构体
typedef struct {
    char filename[64];       // 文件名
    uint16_t width;          // 图片宽度
    uint16_t height;         // 图片高度
    PicFormat format;        // 图片格式
    uint32_t file_size;      // 文件大小
    uint32_t data_offset;    // 数据偏移量（对于BMP等格式）
} PicInfo;

// 动态图片句柄
typedef struct PicHandle* PicHandle_t;

// 错误码定义
typedef enum {
    PIC_SUCCESS = 0,
    PIC_ERROR_FILE_NOT_FOUND,
    PIC_ERROR_FILE_OPEN,
    PIC_ERROR_FILE_READ,
    PIC_ERROR_INVALID_FORMAT,
    PIC_ERROR_MEMORY_ALLOC,
    PIC_ERROR_INVALID_PARAM,
    PIC_ERROR_UNSUPPORTED_FORMAT,
    PIC_ERROR_DECODE_FAILED
} PicError;

// 内部数据结构
typedef struct PicHandle {
    PicInfo info;               // 图片信息
    uint16_t* pixel_data;       // 像素数据（RGB565格式）
    uint32_t data_size;         // 数据大小
    bool is_loaded;             // 是否已加载
} PicHandle;

// 函数声明

/**
 * @brief 初始化动态图片系统
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 */
PicError PIC_Init(void);

/**
 * @brief 释放动态图片系统资源
 */
void PIC_Deinit(void);

/**
 * @brief 从SD卡加载图片
 * @param filename 图片文件名（包含路径）
 * @param handle 返回的图片句柄
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 */
PicError PIC_LoadFromSD(const char* filename, PicHandle_t* handle);

/**
 * @brief 释放图片资源
 * @param handle 图片句柄
 */
void PIC_Free(PicHandle_t handle);

/**
 * @brief 获取图片信息
 * @param handle 图片句柄
 * @param info 返回的图片信息
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 */
PicError PIC_GetInfo(PicHandle_t handle, PicInfo* info);

/**
 * @brief 解析图片信息（不加载整张图片）
 * @param filename 图片文件路径
 * @param info 返回的图片信息
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 */
PicError PIC_ParseInfo(const char* filename, PicInfo* info);

/**
 * @brief 在LCD上显示图片
 * @param handle 图片句柄
 * @param x 显示位置的X坐标
 * @param y 显示位置的Y坐标
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 */
PicError PIC_Display(PicHandle_t handle, uint16_t x, uint16_t y);

/**
 * @brief 在LCD上显示图片（使用DMA传输）
 * @param handle 图片句柄
 * @param x 显示位置的X坐标
 * @param y 显示位置的Y坐标
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 */
PicError PIC_DisplayDMA(PicHandle_t handle, uint16_t x, uint16_t y);

/**
 * @brief 在LCD上显示图片（缩放版本）
 * @param handle 图片句柄
 * @param x 显示位置的X坐标
 * @param y 显示位置的Y坐标
 * @param scale 缩放比例（1.0为原始大小）
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 */
PicError PIC_DisplayScaled(PicHandle_t handle, uint16_t x, uint16_t y, float scale);

/**
 * @brief 在LCD上显示图片（指定区域）
 * @param handle 图片句柄
 * @param x 显示位置的X坐标
 * @param y 显示位置的Y坐标
 * @param src_x 源图片的X坐标
 * @param src_y 源图片的Y坐标
 * @param src_w 源图片区域的宽度
 * @param src_h 源图片区域的高度
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 */
PicError PIC_DisplayRegion(PicHandle_t handle, uint16_t x, uint16_t y, 
                          uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h);

/**
 * @brief 直接显示原始RGB565数据
 * @param data RGB565数据指针
 * @param width 图片宽度
 * @param height 图片高度
 * @param x 显示位置的X坐标
 * @param y 显示位置的Y坐标
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 */
PicError PIC_DisplayRawData(const uint16_t* data, uint16_t width, uint16_t height, 
                           uint16_t x, uint16_t y);

/**
 * @brief 流式显示图片（不加载整张图片到内存）
 * @param filename 图片文件路径
 * @param x 显示位置的X坐标
 * @param y 显示位置的Y坐标
 * @param src_x 源图片的X坐标（BMP使用，0表示从图片左边开始；JPEG使用时忽略）
 * @param src_y 源图片的Y坐标（BMP使用，0表示从图片顶部开始；JPEG使用时忽略）
 * @param src_w 要显示的区域宽度（BMP使用，0表示显示到图片右边缘；JPEG使用时作为缩放参数：0=1/1原始大小, 1=1/2, 2=1/4, 3=1/8）
 * @param src_h 要显示的区域高度（BMP使用，0表示显示到图片底部；JPEG使用时忽略）
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 * 
 * @note 此函数支持BMP和JPEG格式，使用流式解码，逐行读取并显示图片，不会将整张图片加载到内存
 *       BMP内存占用：行缓冲区（约1KB）+ 显示缓冲区（约0.5KB）
 *       JPEG内存占用：工作缓冲区（约10KB（可在efine中调节）） + BMP内存占用量
 *       适合显示大图片或内存受限的场景
 */
PicError PIC_DisplayStreaming(const char* filename, uint16_t x, uint16_t y,
                            uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h);

/**
 * @brief 流式显示图片（使用DMA双缓冲，性能更高）
 * @param filename 图片文件路径
 * @param x 显示位置的X坐标
 * @param y 显示位置的Y坐标
 * @param src_x 源图片的X坐标（BMP使用，0表示从图片左边开始；JPEG使用时忽略）
 * @param src_y 源图片的Y坐标（BMP使用，0表示从图片顶部开始；JPEG使用时忽略）
 * @param src_w 要显示的区域宽度（BMP使用，0表示显示到图片右边缘；JPEG使用时作为缩放参数：0=1/1原始大小, 1=1/2, 2=1/4, 3=1/8）
 * @param src_h 要显示的区域高度（BMP使用，0表示显示到图片底部；JPEG使用时忽略）
 * @return 成功返回PIC_SUCCESS，失败返回错误码
 * 
 * @note 使用DMA双缓冲技术，在发送当前行时并行准备下一行数据
 *       相比PIC_DisplayStreaming有更高的显示效率
 */
PicError PIC_DisplayStreamingDMA(const char* filename, uint16_t x, uint16_t y,
                               uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h);

/**
 * @brief 检查文件是否为支持的图片格式
 * @param filename 文件名
 * @return 是支持的格式返回true，否则返回false
 */
bool PIC_IsSupportedFormat(const char* filename);

/**
 * @brief 获取错误信息字符串
 * @param error 错误码
 * @return 错误信息字符串
 */
const char* PIC_GetErrorString(PicError error);

/**
 * @brief 获取最后发生的错误
 * @return 最后发生的错误码
 */
PicError PIC_GetLastError(void);

// C++接口（如果使用C++编译）
#ifdef __cplusplus

class DynamicImage {
private:
    PicHandle_t handle;
    
public:
    DynamicImage() : handle(nullptr) {}
    
    /**
     * @brief 构造函数，从SD卡加载图片
     * @param filename 图片文件名
     */
    explicit DynamicImage(const char* filename) : handle(nullptr) {
        LoadFromSD(filename);
    }
    
    /**
     * @brief 析构函数，自动释放资源
     */
    ~DynamicImage() {
        if (handle) {
            PIC_Free(handle);
        }
    }
    
    /**
     * @brief 从SD卡加载图片
     * @param filename 图片文件名
     * @return 成功返回true，失败返回false
     */
    bool LoadFromSD(const char* filename) {
        if (handle) {
            PIC_Free(handle);
            handle = nullptr;
        }
        return PIC_LoadFromSD(filename, &handle) == PIC_SUCCESS;
    }
    
    /**
     * @brief 检查图片是否已加载
     * @return 已加载返回true，否则返回false
     */
    bool IsLoaded() const {
        return handle != nullptr;
    }
    
    /**
     * @brief 获取图片信息
     * @param info 返回的图片信息
     * @return 成功返回true，失败返回false
     */
    bool GetInfo(PicInfo* info) const {
        if (!handle) return false;
        return PIC_GetInfo(handle, info) == PIC_SUCCESS;
    }
    
    /**
     * @brief 在LCD上显示图片
     * @param x 显示位置的X坐标
     * @param y 显示位置的Y坐标
     * @return 成功返回true，失败返回false
     */
    bool Display(uint16_t x = 0, uint16_t y = 0) const {
        if (!handle) return false;
        return PIC_Display(handle, x, y) == PIC_SUCCESS;
    }

    /**
     * @brief 在LCD上显示图片
     * @param x 显示位置的X坐标
     * @param y 显示位置的Y坐标
     * @return 成功返回true，失败返回false
     */
    bool DisplayDMA(uint16_t x = 0, uint16_t y = 0) const {
        if (!handle) return false;
        return PIC_DisplayDMA(handle, x, y) == PIC_SUCCESS;
    }
    
    /**
     * @brief 在LCD上显示图片（缩放版本）
     * @param x 显示位置的X坐标
     * @param y 显示位置的Y坐标
     * @param scale 缩放比例
     * @return 成功返回true，失败返回false
     */
    bool DisplayScaled(uint16_t x, uint16_t y, float scale) const {
        if (!handle) return false;
        return PIC_DisplayScaled(handle, x, y, scale) == PIC_SUCCESS;
    }

    bool DisplayRegion(uint16_t x, uint16_t y,
                          uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h) const {
        if (!handle) return false;
        return PIC_DisplayRegion(handle, x, y, src_x, src_y, src_w, src_h) == PIC_SUCCESS;
    }
    
    /**
     * @brief 流式显示图片（静态方法，不需要加载图片）
     * @param filename 图片文件路径
     * @param x 显示位置的X坐标
     * @param y 显示位置的Y坐标
     * @param src_x 源图片的X坐标（BMP使用，JPEG使用时忽略）
     * @param src_y 源图片的Y坐标（BMP使用，JPEG使用时忽略）
     * @param src_w 要显示的区域宽度（BMP使用；JPEG使用时作为缩放参数：0=1/1原始大小, 1=1/2, 2=1/4, 3=1/8）
     * @param src_h 要显示的区域高度（BMP使用，JPEG使用时忽略）
     * @return 成功返回true，失败返回false
     */
    static bool DisplayStreaming(const char* filename, uint16_t x, uint16_t y,
                              uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h) {
        return PIC_DisplayStreaming(filename, x, y, src_x, src_y, src_w, src_h) == PIC_SUCCESS;
    }
    
    static bool DisplayStreamingDMA(const char* filename, uint16_t x, uint16_t y,
                                  uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h) {
        return PIC_DisplayStreamingDMA(filename, x, y, src_x, src_y, src_w, src_h) == PIC_SUCCESS;
    }
    
    static bool ParseInfo(const char* filename, PicInfo* info) {
        return PIC_ParseInfo(filename, info) == PIC_SUCCESS;
    }
    
    /**
     * @brief 获取最后发生的错误
     * @return 错误码
     */
    static PicError GetLastError() {
        return PIC_GetLastError();
    }
    
    /**
     * @brief 获取错误信息字符串
     * @return 错误信息字符串
     */
    static const char* GetErrorString() {
        return PIC_GetErrorString(GetLastError());
    }

    [[nodiscard]] PicHandle_t& GetHandle() { return handle; }
    [[nodiscard]] const PicHandle_t& GetHandle() const { return handle; }
    
    // 禁用拷贝构造和赋值
    DynamicImage(const DynamicImage&) = delete;
    DynamicImage& operator=(const DynamicImage&) = delete;
    
    // 允许移动构造和移动赋值
    DynamicImage(DynamicImage&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    
    DynamicImage& operator=(DynamicImage&& other) noexcept {
        if (this != &other) {
            if (handle) {
                PIC_Free(handle);
            }
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

#endif // SD_AND_LCD2_PIC_TYPES_H