//
// Canvas 画布类实现
// 提供离屏缓冲区和图形绘制功能
//

#include "canvas.h"
#include <algorithm>
#include <optional>
#include <cmath>

extern "C" {
#include "st7735.h"
}

void Canvas::FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (!buffer) return;
    if (x >= width || y >= height) return;

    if (x + w > width) w = width - x;
    if (y + h > height) h = height - y;
    if (w == 0 || h == 0) return;

    uint16_t swapped_color = swap_bytes(color);
    uint16_t* row_start = buffer + y * width + x;

    for (uint16_t row = 0; row < h; row++) {
        std::fill_n(row_start + row * width, w, swapped_color);
    }
}

void Canvas::FillCanvas(uint16_t color) {
    if (!buffer) return;

    uint16_t swapped_color = swap_bytes(color);
    std::fill_n(buffer, static_cast<size_t>(width) * height, swapped_color);
}

#if ENABLE_ADVANCED_METHOD != 0

void Canvas::HollowRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (!buffer) return;
    if (x >= width || y >= height) return;
    if (w == 0 || h == 0) return;

    if (x + w > width) w = width - x;
    if (y + h > height) h = height - y;

    uint16_t swapped_color = swap_bytes(color);

    for (uint16_t col = x; col < x + w; col++) {
        buffer[y * width + col] = swapped_color;
        if (h > 1) {
            buffer[(y + h - 1) * width + col] = swapped_color;
        }
    }

    for (uint16_t row = y; row < y + h; row++) {
        buffer[row * width + x] = swapped_color;
        if (w > 1) {
            buffer[row * width + (x + w - 1)] = swapped_color;
        }
    }
}

void Canvas::FillTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3,
                          uint16_t color) {
    if (!buffer) return;

    uint16_t swapped_color = swap_bytes(color);

#if TRIANGLE_USE_SCANLINE != 0
    if (y2 < y1) { std::swap(x1, x2); std::swap(y1, y2); }
    if (y3 < y1) { std::swap(x1, x3); std::swap(y1, y3); }
    if (y3 < y2) { std::swap(x2, x3); std::swap(y2, y3); }

    int32_t total_height = y3 - y1;
    if (total_height == 0) return;

    auto draw_horizontal_line = [this, swapped_color](int32_t x_start, int32_t x_end, int32_t y) {
        if (y < 0 || y >= static_cast<int32_t>(height)) return;
        if (x_start > x_end) std::swap(x_start, x_end);
        x_start = std::max(0l, x_start);
        x_end = std::min(static_cast<int32_t>(width) - 1, x_end);
        if (x_start <= x_end) {
            std::fill(buffer + y * width + x_start, buffer + y * width + x_end + 1, swapped_color);
        }
    };

    for (int32_t i = 0; i < total_height; i++) {
        bool second_half = i > y2 - y1 || y2 == y1;
        int32_t segment_height = second_half ? y3 - y2 : y2 - y1;
        if (segment_height == 0) continue;

        double alpha = static_cast<double>(i) / total_height;
        double beta = second_half
            ? static_cast<double>(i - (y2 - y1)) / segment_height
            : static_cast<double>(i) / segment_height;

        int32_t ax = x1 + static_cast<int32_t>((x3 - x1) * alpha);
        int32_t bx = second_half 
            ? x2 + static_cast<int32_t>((x3 - x2) * beta)
            : x1 + static_cast<int32_t>((x2 - x1) * beta);

        draw_horizontal_line(ax, bx, y1 + i);
    }
#else
    int32_t min_y = std::min({y1, y2, y3});
    int32_t max_y = std::max({y1, y2, y3});

    if (min_y >= height || max_y < 0) return;

    auto edge_function = [](int32_t x, int32_t y, int32_t ax, int32_t ay, int32_t bx, int32_t by) -> int32_t {
        return (bx - ax) * (y - ay) - (by - ay) * (x - ax);
    };

    int32_t area = edge_function(x1, y1, x2, y2, x3, y3);
    if (area == 0) return;

    int32_t min_x = std::min({x1, x2, x3});
    int32_t max_x = std::max({x1, x2, x3});

    min_y = std::max(0l, min_y);
    max_y = std::min(static_cast<int32_t>(height) - 1, max_y);
    min_x = std::max(0l, min_x);
    max_x = std::min(static_cast<int32_t>(width) - 1, max_x);

    for (int32_t y = min_y; y <= max_y; y++) {
        int32_t x_start = max_x;
        int32_t x_end = min_x;

        for (int32_t x = min_x; x <= max_x; x++) {
            int32_t w0 = edge_function(x, y, x2, y2, x3, y3);
            int32_t w1 = edge_function(x, y, x3, y3, x1, y1);
            int32_t w2 = edge_function(x, y, x1, y1, x2, y2);

            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                if (x < x_start) x_start = x;
                if (x > x_end) x_end = x;
            }
        }

        if (x_start <= x_end) {
            std::fill(buffer + y * width + x_start, buffer + y * width + x_end + 1, swapped_color);
        }
    }
#endif
}

void Canvas::HollowTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3,
                            uint16_t color) {
    if (!buffer) return;

    Line(x1, y1, x2, y2, color);
    Line(x2, y2, x3, y3, color);
    Line(x3, y3, x1, y1, color);
}

void Canvas::Line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    if (!buffer) return;

    uint16_t swapped_color = swap_bytes(color);

    int32_t dx = abs(static_cast<int32_t>(x1) - static_cast<int32_t>(x0));
    int32_t dy = -abs(static_cast<int32_t>(y1) - static_cast<int32_t>(y0));
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;
    int32_t cx = x0, cy = y0;

    while (true) {
        if (cx >= 0 && cx < static_cast<int32_t>(width) && cy >= 0 && cy < static_cast<int32_t>(height)) {
            buffer[cy * width + cx] = swapped_color;
        }

        if (cx == static_cast<int32_t>(x1) && cy == static_cast<int32_t>(y1)) break;

        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            cx += sx;
        }
        if (e2 <= dx) {
            err += dx;
            cy += sy;
        }
    }
}

void Canvas::FillCircle(uint16_t cx, uint16_t cy, uint16_t radius, uint16_t color) {
    if (!buffer) return;
    if (radius == 0) return;

    uint16_t swapped_color = swap_bytes(color);

    int32_t x = 0;
    int32_t y = radius;
    int32_t d = 3 - 2 * radius;

    while (x <= y) {
        for (int32_t i = cx - x; i <= cx + x; i++) {
            if (i >= 0 && i < static_cast<int32_t>(width)) {
                int32_t py1 = cy - y;
                int32_t py2 = cy + y;
                if (py1 >= 0 && py1 < static_cast<int32_t>(height)) {
                    buffer[py1 * width + i] = swapped_color;
                }
                if (py2 >= 0 && py2 < static_cast<int32_t>(height)) {
                    buffer[py2 * width + i] = swapped_color;
                }
            }
        }

        for (int32_t i = cx - y; i <= cx + y; i++) {
            if (i >= 0 && i < static_cast<int32_t>(width)) {
                int32_t py1 = cy - x;
                int32_t py2 = cy + x;
                if (py1 >= 0 && py1 < static_cast<int32_t>(height)) {
                    buffer[py1 * width + i] = swapped_color;
                }
                if (py2 >= 0 && py2 < static_cast<int32_t>(height)) {
                    buffer[py2 * width + i] = swapped_color;
                }
            }
        }

        if (d < 0) {
            d = d + 4 * x + 6;
        }
        else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void Canvas::HollowCircle(uint16_t cx, uint16_t cy, uint16_t radius, uint16_t color) {
    if (!buffer) return;
    if (radius == 0) return;

    uint16_t swapped_color = swap_bytes(color);

    int32_t x = 0;
    int32_t y = radius;
    int32_t d = 3 - 2 * radius;

    auto set_pixel = [this, swapped_color](int32_t px, int32_t py) {
        if (px >= 0 && px < static_cast<int32_t>(width) && py >= 0 && py < static_cast<int32_t>(height)) {
            buffer[py * width + px] = swapped_color;
        }
    };

    while (x <= y) {
        set_pixel(cx + x, cy + y);
        set_pixel(cx - x, cy + y);
        set_pixel(cx + x, cy - y);
        set_pixel(cx - x, cy - y);
        set_pixel(cx + y, cy + x);
        set_pixel(cx - y, cy + x);
        set_pixel(cx + y, cy - x);
        set_pixel(cx - y, cy - x);

        if (d < 0) {
            d = d + 4 * x + 6;
        }
        else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void Canvas::FillEllipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry, uint16_t color) {
    if (!buffer) return;
    if (rx == 0 || ry == 0) return;

    uint16_t swapped_color = swap_bytes(color);

#if ELLIPSE_USE_MIDPOINT != 0
    int32_t x = 0;
    int32_t y = ry;
    int32_t rx2 = static_cast<int32_t>(rx) * rx;
    int32_t ry2 = static_cast<int32_t>(ry) * ry;
    int32_t two_rx2 = 2 * rx2;
    int32_t two_ry2 = 2 * ry2;
    int32_t px = 0;
    int32_t py = two_rx2 * y;

    auto draw_horizontal_line = [this, swapped_color, cx](int32_t x1, int32_t y_pos) {
        if (y_pos < 0 || y_pos >= static_cast<int32_t>(height)) return;
        int32_t start = cx - x1;
        int32_t end = cx + x1;
        start = std::max(0l, start);
        end = std::min(static_cast<int32_t>(width) - 1, end);
        if (start <= end) {
            std::fill(buffer + y_pos * width + start, buffer + y_pos * width + end + 1, swapped_color);
        }
    };

    auto p = static_cast<int32_t>(ry2 - rx2 * ry + 0.25 * rx2);
    while (px < py) {
        draw_horizontal_line(x, cy + y);
        draw_horizontal_line(x, cy - y);
        x++;
        px += two_ry2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= two_rx2;
            p += ry2 + px - py;
        }
    }

    p = static_cast<int32_t>(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
        draw_horizontal_line(x, cy + y);
        draw_horizontal_line(x, cy - y);
        y--;
        py -= two_rx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += two_ry2;
            p += rx2 - py + px;
        }
    }
#else
    int32_t rx2 = rx * rx;
    int32_t ry2 = ry * ry;

    for (int32_t y = -static_cast<int32_t>(ry); y <= static_cast<int32_t>(ry); y++) {
        auto x_limit = static_cast<int32_t>(sqrt(rx2 * (1 - static_cast<double>(y * y) / ry2)));

        int32_t py = cy + y;
        if (py >= 0 && py < static_cast<int32_t>(height)) {
            int32_t start = std::max(0l, cx - x_limit);
            int32_t end = std::min(static_cast<int32_t>(width) - 1, cx + x_limit);
            if (start <= end) {
                std::fill(buffer + py * width + start, buffer + py * width + end + 1, swapped_color);
            }
        }
    }
#endif
}

void Canvas::HollowEllipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry, uint16_t color) {
    if (!buffer) return;
    if (rx == 0 || ry == 0) return;

    uint16_t swapped_color = swap_bytes(color);

    int32_t rx2 = rx * rx;
    int32_t ry2 = ry * ry;

    auto set_pixel = [this, swapped_color](int32_t px, int32_t py) {
        if (px >= 0 && px < static_cast<int32_t>(width) && py >= 0 && py < static_cast<int32_t>(height)) {
            buffer[py * width + px] = swapped_color;
        }
    };

    for (int32_t y = -static_cast<int32_t>(ry); y <= static_cast<int32_t>(ry); y++) {
        auto x = static_cast<int32_t>(sqrt(rx2 * (1 - static_cast<double>(y * y) / ry2)));

        set_pixel(cx + x, cy + y);
        set_pixel(cx - x, cy + y);
    }
}

#endif

void Canvas::WriteUnicodeString(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color) {
    WriteUnicodeStringImpl(x, y, utf8_str, font, color, std::nullopt);
}

void Canvas::WriteUnicodeString(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color,
                                uint16_t bgcolor) {
    WriteUnicodeStringImpl(x, y, utf8_str, font, color, bgcolor);
}

void Canvas::WriteUnicodeString(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font,
                                uint16_t color) {
    WriteUnicodeStringImpl(x, y, unicode_str, font, color, std::nullopt);
}

void Canvas::WriteUnicodeString(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color,
                                uint16_t bgcolor) {
    WriteUnicodeStringImpl(x, y, unicode_str, font, color, bgcolor);
}

uint16_t Canvas::GetCharSpacing(uint16_t char_width, uint32_t unicode) {
    uint16_t spacing = char_width + 1;

    if ((unicode >= 0x2000 && unicode <= 0x206F) ||
        (unicode >= 0x3000 && unicode <= 0x303F) ||
        (unicode >= 0xFF00 && unicode <= 0xFFEF) ||
        unicode == 0x002C || unicode == 0x002E || unicode == 0x003B ||
        unicode == 0x003A || unicode == 0x0021 || unicode == 0x003F) {
        spacing += 1;
    }

    return spacing;
}

void Canvas::WriteUnicodeStringImpl(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color,
                                    std::optional<uint16_t> bgcolor) {
    if (!buffer || !utf8_str || !font) return;

    const char* ptr = utf8_str;
    uint16_t current_x = x;
    uint16_t current_y = y;
    uint16_t default_height = font->GetDefaultHeight();

    uint16_t total_width = 0;
    uint16_t line_width = 0;
    uint16_t line_count = 1;

    const char* temp_ptr = utf8_str;
    while (*temp_ptr != '\0') {
        uint32_t unicode = UTF8ToUnicode(&temp_ptr);
        if (unicode == 0) break;

        uint16_t char_width;
        
        if (!font->GetCharWidth(unicode, &char_width)) {
            char_width = font->GetDefaultWidth();
        }

        uint16_t char_spacing = GetCharSpacing(char_width, unicode);

        if (line_width + char_spacing > width) {
            if (line_width > total_width) {
                total_width = line_width;
            }
            line_width = char_spacing;
            line_count++;
        }
        else {
            line_width += char_spacing;
        }
    }

    if (line_width > total_width) {
        total_width = line_width;
    }

    if (bgcolor.has_value()) {
        uint16_t total_height = line_count * default_height;
        FillRectangle(x, y, total_width, total_height, bgcolor.value());
    }

    while (*ptr != '\0') {
        uint32_t unicode = UTF8ToUnicode(&ptr);
        if (unicode == 0) break;

        std::shared_ptr<uint8_t[]> bitmap;
        uint16_t char_width, char_height;

        if (unicode == 0x0020 || unicode == 0x00A0) {
            char_width = font->GetDefaultWidth();
            char_height = font->GetDefaultHeight();
            current_x += GetCharSpacing(char_width, unicode);
            continue;
        }

        if (!font->LoadChar(unicode, bitmap, &char_width, &char_height)) {
            char_width = font->GetDefaultWidth();
            char_height = font->GetDefaultHeight();
            current_x += GetCharSpacing(char_width, unicode);
            continue;
        }

        if (current_x + char_width > width) {
            current_x = x;
            current_y += default_height;
            if (current_y + default_height > height) break;
        }

        uint16_t baseline_offset = default_height - 1;
        uint16_t char_baseline = char_height - 1;
        uint16_t render_y = current_y + baseline_offset - char_baseline;

        DrawChar(current_x, render_y, bitmap, char_width, char_height, color, std::nullopt);

        current_x += GetCharSpacing(char_width, unicode);
    }
}

void Canvas::WriteUnicodeStringImpl(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font,
                                    uint16_t color, std::optional<uint16_t> bgcolor) {
    if (!buffer || !unicode_str || !font) return;

    uint16_t current_x = x;
    uint16_t current_y = y;

    uint16_t total_width = 0;
    uint16_t line_width = 0;
    uint16_t line_count = 1;

    const uint32_t* temp_ptr = unicode_str;
    while (*temp_ptr != 0) {
        uint32_t unicode = *temp_ptr;

        uint16_t char_width;
        
        if (!font->GetCharWidth(unicode, &char_width)) {
            char_width = font->GetDefaultWidth();
        }

        uint16_t char_spacing = GetCharSpacing(char_width, unicode);

        if (line_width + char_spacing > width) {
            if (line_width > total_width) {
                total_width = line_width;
            }
            line_width = char_spacing;
            line_count++;
        }
        else {
            line_width += char_spacing;
        }

        temp_ptr++;
    }

    if (line_width > total_width) {
        total_width = line_width;
    }

    if (bgcolor.has_value()) {
        uint16_t total_height = line_count * font->GetDefaultHeight();
        FillRectangle(x, y, total_width, total_height, bgcolor.value());
    }

    while (*unicode_str != 0) {
        uint32_t unicode = *unicode_str;

        std::shared_ptr<uint8_t[]> bitmap;
        uint16_t char_width, char_height;

        if (unicode == 0x0020 || unicode == 0x00A0) {
            char_width = font->GetDefaultWidth();
            char_height = font->GetDefaultHeight();
            current_x += GetCharSpacing(char_width, unicode);
            unicode_str++;
            continue;
        }

        if (!font->LoadChar(unicode, bitmap, &char_width, &char_height)) {
            char_width = font->GetDefaultWidth();
            char_height = font->GetDefaultHeight();
            current_x += GetCharSpacing(char_width, unicode);
            unicode_str++;
            continue;
        }

        if (current_x + char_width > width) {
            current_x = 0;
            current_y += font->GetDefaultHeight();
            if (current_y + font->GetDefaultHeight() > height) break;
        }

        uint16_t baseline_offset = font->GetDefaultHeight() - 1;
        uint16_t char_baseline = char_height - 1;
        uint16_t render_y = current_y + baseline_offset - char_baseline;

        DrawChar(current_x, render_y, bitmap, char_width, char_height, color, std::nullopt);

        current_x += GetCharSpacing(char_width, unicode);
        unicode_str++;
    }
}

void Canvas::DrawChar(uint16_t x, uint16_t y, const std::shared_ptr<uint8_t[]>& bitmap, uint16_t char_width,
                      uint16_t char_height, uint16_t color, std::optional<uint16_t> bgcolor) {
    if (x >= width || y >= height) return;

    uint16_t swapped_color = swap_bytes(color);
    uint16_t swapped_bgcolor = bgcolor.has_value() ? swap_bytes(bgcolor.value()) : 0;

    uint16_t row_end = (y + char_height > height) ? height - y : char_height;
    uint16_t col_end = (x + char_width > width) ? width - x : char_width;

    uint16_t bytes_per_row = (char_width + 7) / 8;
    uint16_t* buffer_ptr = buffer + y * width + x;

    for (uint16_t row = 0; row < row_end; row++) {
        const uint8_t* bitmap_row = bitmap.get() + row * bytes_per_row;
        uint16_t* buffer_row = buffer_ptr + row * width;

        for (uint16_t col = 0; col < col_end; col++) {
            uint8_t bitmap_byte = bitmap_row[col >> 3];
            uint8_t bit_mask = 0x80 >> (col & 7);

            if (bitmap_byte & bit_mask) {
                buffer_row[col] = swapped_color;
            }
            else if (bgcolor.has_value()) {
                buffer_row[col] = swapped_bgcolor;
            }
        }
    }
}

void Canvas::DrawSpace(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t bgcolor) {
    uint16_t swapped_bgcolor = swap_bytes(bgcolor);

    for (uint16_t row = 0; row < h; row++) {
        for (uint16_t col = 0; col < w; col++) {
            uint16_t buf_x = x + col;
            uint16_t buf_y = y + row;
            if (buf_x < width && buf_y < height) {
                buffer[buf_y * width + buf_x] = swapped_bgcolor;
            }
        }
    }
}

PicError Canvas::DrawImage(const DynamicImage& image, uint16_t x, uint16_t y, uint16_t x0, uint16_t y0, uint16_t w,
                           uint16_t h) {
    if (!buffer) return PIC_ERROR_INVALID_PARAM;
    if (!image.IsLoaded()) return PIC_ERROR_INVALID_PARAM;

    PicInfo info;
    if (!image.GetInfo(&info)) return PIC_ERROR_INVALID_PARAM;

    if (w == 0) w = info.width - x0;
    if (h == 0) h = info.height - y0;

    if (x0 >= info.width || y0 >= info.height) return PIC_ERROR_INVALID_PARAM;
    if (x0 + w > info.width) w = info.width - x0;
    if (y0 + h > info.height) h = info.height - y0;

    const auto handle = image.GetHandle();
    const uint16_t* img_data = handle->pixel_data;
    if (!img_data) return PIC_ERROR_INVALID_PARAM;

    if (x >= width || y >= height) return PIC_ERROR_INVALID_PARAM;

    uint16_t copy_w = (x + w > width) ? width - x : w;
    uint16_t copy_h = (y + h > height) ? height - y : h;

    for (uint16_t row = 0; row < copy_h; row++) {
        const uint16_t* src_row = img_data + (y0 + row) * info.width + x0;
        uint16_t* dst_row = buffer + (y + row) * width + x;
        memcpy(dst_row, src_row, copy_w * sizeof(uint16_t));
    }

    return PIC_SUCCESS;
}

void Canvas::DrawCanvas(uint16_t x, uint16_t y) const {
    if (!buffer) return;

    ST7735_DrawImage(x, y, width, height, buffer);
}

bool Canvas::isDMAIdle() {
    return HAL_SPI_GetState(&ST7735_SPI_PORT) == HAL_SPI_STATE_READY 
           && !__HAL_SPI_GET_FLAG(&ST7735_SPI_PORT, SPI_FLAG_BSY);
}

void Canvas::DrawCanvasDMA(uint16_t x, uint16_t y, bool wait_dma) const {
    if (!buffer) return;
    if (!isDMAIdle()) return;

    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x + width - 1, y + height - 1);
    ST7735_DC_HIGH();

    HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, reinterpret_cast<uint8_t*>(buffer), width * height * sizeof(uint16_t));

    if (wait_dma) {
        while (isDMAIdle())
        ST7735_Unselect();
    }
}

bool Canvas::RenewBuffer() {
    if (auto_release) {
        delete[] this->buffer;
    }
    buffer = new uint16_t[width * height];
    auto_release = true;
    return buffer != nullptr;
}

bool Canvas::RenewBuffer(uint16_t width, uint16_t height) {
    if (auto_release) {
        delete[] this->buffer;
    }
    this->width = width;
    this->height = height;
    buffer = new uint16_t[width * height];
    auto_release = true;
    return buffer != nullptr;
}

void Canvas::RenewBuffer(uint16_t* buffer) {
    if (auto_release) {
        delete[] this->buffer;
    }
    this->buffer = buffer;
    auto_release = false;
}

void Canvas::RenewBuffer(uint16_t* buffer, uint16_t width, uint16_t height) {
    if (auto_release) {
        delete[] this->buffer;
    }
    this->buffer = buffer;
    this->width = width;
    this->height = height;
    auto_release = false;
}

void Canvas::Copy(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t x0, uint16_t y0) {
    if (!buffer) return;
    if (w == 0 || h == 0) return;
    
    if (x >= width || y >= height) return;
    if (x + w > width) w = width - x;
    if (y + h > height) h = height - y;
    
    if (x0 >= width || y0 >= height) return;
    if (x0 + w > width) w = width - x0;
    if (y0 + h > height) h = height - y0;
    
    if (w == 0 || h == 0) return;
    
    int16_t row_start, row_end, row_step;
    int16_t col_start, col_end, col_step;
    
    if (y0 > y) {
        row_start = h - 1;
        row_end = -1;
        row_step = -1;
    } else {
        row_start = 0;
        row_end = h;
        row_step = 1;
    }
    
    if (x0 > x) {
        col_start = w - 1;
        col_end = -1;
        col_step = -1;
    } else {
        col_start = 0;
        col_end = w;
        col_step = 1;
    }
    
    for (int16_t row = row_start; row != row_end; row += row_step) {
        uint16_t* src_row = buffer + (y + row) * width + x;
        uint16_t* dst_row = buffer + (y0 + row) * width + x0;
        
        for (int16_t col = col_start; col != col_end; col += col_step) {
            dst_row[col] = src_row[col];
        }
    }
}
