//
// Canvas 画布类定义
// 提供离屏缓冲区和图形绘制功能
// 支持基本图形、文本渲染和图像绘制
//

#ifndef SD_AND_LCD2_CANVAS_H
#define SD_AND_LCD2_CANVAS_H

#include "unicode_render.h"
#include "pic_types.h"

#define ENABLE_ADVANCED_METHOD 1
// 是否启用高级绘图方法

#define TRIANGLE_USE_SCANLINE 1
// 三角形填充算法选择
// 0: 重心坐标算法
// 1: 扫描线算法

#define ELLIPSE_USE_MIDPOINT 1
// 椭圆填充算法选择
// 0: 浮点运算算法
// 1: 中点椭圆算法

#ifdef __cplusplus
#include <optional>
/**
 * @brief Canvas 画布类，提供离屏缓冲区和图形绘制功能
 * 
 * Canvas 类提供了一个离屏缓冲区，可以在内存中进行各种图形绘制操作，
 * 然后一次性显示到 LCD 上。这种方式可以减少闪烁，提高显示效率。
 * 
 * 支持的功能：
 * - 基本图形：矩形、三角形、圆形、椭圆
 * - 文本渲染：支持 UTF-8 和 Unicode 字符串
 * - 图像绘制：支持从 DynamicImage 绘制图片
 * - 显示优化：支持 DMA 传输
 * 
 * 内存管理：
 * - 可以自动管理缓冲区内存（构造函数分配，析构函数释放）
 * - 也可以使用用户提供的外部缓冲区
 */
class Canvas {
private:
    uint16_t* buffer = nullptr;
    uint16_t width = 0, height = 0;
    bool auto_release = true;

    static uint16_t swap_bytes(uint16_t value) {
        return ((value & 0xFF00) >> 8) | ((value & 0xFF) << 8);
    }

    static uint16_t GetCharSpacing(uint16_t char_width, uint32_t unicode);

    void WriteUnicodeStringImpl(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color,
                                std::optional<uint16_t> bgcolor);
    void WriteUnicodeStringImpl(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color,
                                std::optional<uint16_t> bgcolor);
    void DrawChar(uint16_t x, uint16_t y, const std::shared_ptr<uint8_t[]>& bitmap, uint16_t char_width, uint16_t char_height,
                  uint16_t color, std::optional<uint16_t> bgcolor);
    void DrawSpace(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t bgcolor);

public:
    /**
     * @brief 构造函数，自动分配缓冲区
     * @param width 画布宽度
     * @param height 画布高度
     * @note 缓冲区由类自动管理，析构时自动释放
     */
    Canvas(uint16_t width, uint16_t height) : width(width), height(height) {
        buffer = new uint16_t[width * height];
    }

    /**
     * @brief 构造函数，使用用户提供的外部缓冲区
     * @param buffer 外部缓冲区指针
     * @param width 画布宽度
     * @param height 画布高度
     * @note 缓冲区由用户管理，析构时不会释放
     */
    Canvas(uint16_t* buffer, uint16_t width, uint16_t height) : buffer(buffer), width(width), height(height),
                                                                auto_release(false) {
    }

    /**
     * @brief 析构函数
     * @note 如果缓冲区由类自动管理，则释放缓冲区
     */
    ~Canvas() {
        if (auto_release) delete [] buffer;
    }

    /**
     * @brief 填充矩形区域
     * @param x 矩形左上角X坐标
     * @param y 矩形左上角Y坐标
     * @param w 矩形宽度
     * @param h 矩形高度
     * @param color 填充颜色（RGB565格式）
     */
    void FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

    /**
     * @brief 填充整个画布
     * @param color 填充颜色（RGB565格式）
     */
    void FillCanvas(uint16_t color);

#if ENABLE_ADVANCED_METHOD != 0
    /**
     * @brief 绘制空心矩形
     * @param x 矩形左上角X坐标
     * @param y 矩形左上角Y坐标
     * @param w 矩形宽度
     * @param h 矩形高度
     * @param color 边框颜色（RGB565格式）
     */
    void HollowRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

    /**
     * @brief 填充三角形
     * @param x1 第一个顶点的X坐标
     * @param y1 第一个顶点的Y坐标
     * @param x2 第二个顶点的X坐标
     * @param y2 第二个顶点的Y坐标
     * @param x3 第三个顶点的X坐标
     * @param y3 第三个顶点的Y坐标
     * @param color 填充颜色（RGB565格式）
     */
    void FillTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);

    /**
     * @brief 绘制空心三角形
     * @param x1 第一个顶点的X坐标
     * @param y1 第一个顶点的Y坐标
     * @param x2 第二个顶点的X坐标
     * @param y2 第二个顶点的Y坐标
     * @param x3 第三个顶点的X坐标
     * @param y3 第三个顶点的Y坐标
     * @param color 边框颜色（RGB565格式）
     */
    void HollowTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);

    /**
     * @brief 绘制直线
     * @param x0 起点X坐标
     * @param y0 起点Y坐标
     * @param x1 终点X坐标
     * @param y1 终点Y坐标
     * @param color 线条颜色（RGB565格式）
     * @note 使用 Bresenham 算法绘制
     */
    void Line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

    /**
     * @brief 填充圆形
     * @param cx 圆心X坐标
     * @param cy 圆心Y坐标
     * @param radius 圆半径
     * @param color 填充颜色（RGB565格式）
     * @note 使用 Bresenham 圆形算法
     */
    void FillCircle(uint16_t cx, uint16_t cy, uint16_t radius, uint16_t color);

    /**
     * @brief 绘制空心圆形
     * @param cx 圆心X坐标
     * @param cy 圆心Y坐标
     * @param radius 圆半径
     * @param color 边框颜色（RGB565格式）
     * @note 使用 Bresenham 圆形算法
     */
    void HollowCircle(uint16_t cx, uint16_t cy, uint16_t radius, uint16_t color);

    /**
     * @brief 填充椭圆
     * @param cx 椭圆中心X坐标
     * @param cy 椭圆中心Y坐标
     * @param rx X轴半径
     * @param ry Y轴半径
     * @param color 填充颜色（RGB565格式）
     */
    void FillEllipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry, uint16_t color);

    /**
     * @brief 绘制空心椭圆
     * @param cx 椭圆中心X坐标
     * @param cy 椭圆中心Y坐标
     * @param rx X轴半径
     * @param ry Y轴半径
     * @param color 边框颜色（RGB565格式）
     */
    void HollowEllipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry, uint16_t color);
#endif

    /**
     * @brief 绘制 UTF-8 字符串
     * @param x 起始X坐标
     * @param y 起始Y坐标
     * @param utf8_str UTF-8 字符串
     * @param font 字体对象
     * @param color 文字颜色（RGB565格式）
     * @note 不绘制背景，透明显示
     */
    void WriteUnicodeString(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color);

    /**
     * @brief 绘制 UTF-8 字符串（带背景色）
     * @param x 起始X坐标
     * @param y 起始Y坐标
     * @param utf8_str UTF-8 字符串
     * @param font 字体对象
     * @param color 文字颜色（RGB565格式）
     * @param bgcolor 背景颜色（RGB565格式）
     */
    void WriteUnicodeString(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color,
                            uint16_t bgcolor);

    /**
     * @brief 绘制 Unicode 字符串
     * @param x 起始X坐标
     * @param y 起始Y坐标
     * @param unicode_str Unicode 字符串（以0结尾）
     * @param font 字体对象
     * @param color 文字颜色（RGB565格式）
     * @note 不绘制背景，透明显示
     */
    void WriteUnicodeString(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color);

    /**
     * @brief 绘制 Unicode 字符串（带背景色）
     * @param x 起始X坐标
     * @param y 起始Y坐标
     * @param unicode_str Unicode 字符串（以0结尾）
     * @param font 字体对象
     * @param color 文字颜色（RGB565格式）
     * @param bgcolor 背景颜色（RGB565格式）
     */
    void WriteUnicodeString(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color,
                            uint16_t bgcolor);

    /**
     * @brief 在画布上绘制图片
     * @param image 图片对象
     * @param x 图片在画布上的起始X坐标
     * @param y 图片在画布上的起始Y坐标
     * @param x0 图片被渲染部分的起始X坐标（默认0）
     * @param y0 图片被渲染部分的起始Y坐标（默认0）
     * @param w 图片被渲染部分的宽度（默认0表示到图片右边缘）
     * @param h 图片被渲染部分的高度（默认0表示到图片下边缘）
     * @return 成功返回PIC_SUCCESS，失败返回错误码
     * @note 可以只渲染图片的一部分
     */
    PicError DrawImage(const DynamicImage& image, uint16_t x, uint16_t y, uint16_t x0 = 0, uint16_t y0 = 0,
                       uint16_t w = 0,
                       uint16_t h = 0);

    /**
     * @brief 将画布内容显示到 LCD
     * @param x 显示位置的X坐标
     * @param y 显示位置的Y坐标
     * @note 使用普通 SPI 传输
     */
    void DrawCanvas(uint16_t x = 0, uint16_t y = 0) const;

    /**
     * @brief 将画布内容显示到 LCD（使用 DMA 传输）
     * @param x 显示位置的X坐标
     * @param y 显示位置的Y坐标
     * @param wait_dma 是否阻塞等待DMA传输
     * @note 使用 DMA 传输
     * @note 注意：如果你需要在传输后修改画布，请调用isDMAIdle手动阻塞等待并调用ST7735_Unselect或者设置wait_dma为true，否则DMA传输会受影响
     */
    void DrawCanvasDMA(uint16_t x = 0, uint16_t y = 0, bool wait_dma = true) const;

    /**
     * @brief 获取画布尺寸
     * @return 返回包含宽度和高度的 pair 对象
     */
    [[nodiscard]] std::pair<uint16_t, uint16_t> GetSize() {
        return {width, height};
    }

    /**
     * @brief 检查缓冲区是否有效
     * @return 缓冲区有效返回true，否则返回false
     */
    [[nodiscard]] bool isBufferValid() const { return buffer; }

    /**
     * @brief 检查DMA是否完成
     * @return 完成返回true，否则返回false
     */
    [[nodiscard]] static bool isDMAIdle();

    /**
     * @brief 重新分配缓冲区（保持当前尺寸）
     * @return 成功返回true，失败返回false
     * @note 如果缓冲区由类自动管理，则先释放旧缓冲区
     */
    bool RenewBuffer();

    /**
     * @brief 重新分配缓冲区（指定新尺寸）
     * @param width 新的画布宽度
     * @param height 新的画布高度
     * @return 成功返回true，失败返回false
     * @note 如果缓冲区由类自动管理，则先释放旧缓冲区
     */
    bool RenewBuffer(uint16_t width, uint16_t height);

    /**
     * @brief 使用用户提供的外部缓冲区
     * @param buffer 外部缓冲区指针
     * @note 缓冲区由用户管理，析构时不会释放
     */
    void RenewBuffer(uint16_t* buffer);

    /**
     * @brief 使用用户提供的外部缓冲区（指定尺寸）
     * @param buffer 外部缓冲区指针
     * @param width 画布宽度
     * @param height 画布高度
     * @note 缓冲区由用户管理，析构时不会释放
     */
    void RenewBuffer(uint16_t* buffer, uint16_t width, uint16_t height);

    /**
     * @brief 复制画布区域
     * @param x 源区域左上角X坐标
     * @param y 源区域左上角Y坐标
     * @param w 复制区域的宽度
     * @param h 复制区域的高度
     * @param x0 目标位置X坐标
     * @param y0 目标位置Y坐标
     * @note 将画布上从(x, y)开始的区域复制到(x0, y0)位置，自动处理重叠情况
     */
    void Copy(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t x0, uint16_t y0);
};
#endif

#endif //SD_AND_LCD2_CANVAS_H
