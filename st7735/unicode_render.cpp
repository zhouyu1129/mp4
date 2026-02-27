#include "unicode_render.h"
#include "st7735.h"
#include <cstdio>
#include <memory>

static bool GetBitmapPixel(const uint8_t* bitmap, uint16_t width, uint16_t height, uint16_t x, uint16_t y) {
    if (x >= width || y >= height) return false;
    
    uint32_t bytes_per_row = (width + 7) / 8;
    uint32_t byte_index = y * bytes_per_row + (x / 8);
    uint8_t bit_offset = 7 - (x % 8);
    
    uint32_t total_bytes = bytes_per_row * height;
    if (byte_index >= total_bytes) return false;
    
    return (bitmap[byte_index] >> bit_offset) & 1;
}

void WriteUnicodeChar(uint16_t x, uint16_t y, uint32_t unicode, UnicodeFont* font, uint16_t color, uint16_t bgcolor) {
    if (!font || !font->IsValid()) {
        printf("WriteUnicodeChar: 字体无效!\r\n");
        return;
    }
    
    std::shared_ptr<uint8_t[]> bitmap;
    uint16_t width, height;
    
    if (!font->LoadChar(unicode, bitmap, &width, &height)) {
        printf("WriteUnicodeChar: 字符 U+%04lX 不在字体中，绘制占位符方框!\r\n", unicode);
        width = font->GetDefaultWidth();
        height = font->GetDefaultHeight();
        DrawPlaceholderBox(x, y, width, height, color);
        return;
    }
    
    uint16_t baseline_offset = (font->GetDefaultHeight() * 8) / 10;
    uint16_t char_baseline = (height * 8) / 10;
    
    uint16_t render_y = y + baseline_offset - char_baseline;
    
    if (FONT_RENDER_DEBUG_INFO) printf("WriteUnicodeChar: 渲染字符 U+%04lX, 位置: (%d, %d), 尺寸: %dx%d, 基线Y: %d\r\n",
           unicode, x, render_y, width, height, render_y);
    
    if (x + width > ST7735_WIDTH || render_y + height > ST7735_HEIGHT) {
        printf("WriteUnicodeChar: 位置超出屏幕范围!\r\n");
        return;
    }
    
    uint16_t pixel_count = 0;
    uint16_t* row_buffer = (uint16_t*)malloc(width * sizeof(uint16_t));
    if (!row_buffer) {
        printf("WriteUnicodeChar: 内存分配失败!\r\n");
        return;
    }
    
    uint16_t bytes_per_row = (width + 7) / 8;
    
    for (uint16_t row = 0; row < height; row++) {
        const uint8_t* bitmap_row = bitmap.get() + row * bytes_per_row;
        
        for (uint16_t col = 0; col < width; col++) {
            uint8_t byte_index = col / 8;
            uint8_t bit_mask = 1 << (7 - (col % 8));
            
            if (bitmap_row[byte_index] & bit_mask) {
                row_buffer[col] = color;
                pixel_count++;
            } else {
                row_buffer[col] = bgcolor;
            }
        }
        
        ST7735_Select();
        ST7735_SetAddressWindow(x, render_y + row, x + width - 1, render_y + row);
        ST7735_DC_HIGH();
        
        uint8_t* byte_buffer = (uint8_t*)row_buffer;
        for (uint16_t i = 0; i < width; i++) {
            uint8_t tmp = byte_buffer[i * 2];
            byte_buffer[i * 2] = byte_buffer[i * 2 + 1];
            byte_buffer[i * 2 + 1] = tmp;
        }
        
        HAL_SPI_Transmit(&ST7735_SPI_PORT, byte_buffer, width * 2, HAL_MAX_DELAY);
        ST7735_Unselect();
    }
    
    free(row_buffer);
    
    if (FONT_RENDER_DEBUG_INFO) printf("WriteUnicodeChar: 字符 U+%04lX 渲染完成，绘制了 %d 个像素\r\n", unicode, pixel_count);
}

void WriteUnicodeString(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color, uint16_t bgcolor) {
    if (!unicode_str || !font || !font->IsValid()) return;
    
    uint16_t total_width = 0;
    uint16_t line_width = 0;
    uint16_t line_count = 1;
    
    const uint32_t* temp_ptr = unicode_str;
    while (*temp_ptr != 0) {
        uint16_t width;
        
        if (!font->GetCharWidth(*temp_ptr, &width)) {
            width = font->GetDefaultWidth();
        }
        
        if (line_width + width > ST7735_WIDTH) {
            if (line_width > total_width) {
                total_width = line_width;
            }
            line_width = width;
            line_count++;
        } else {
            line_width += width;
        }
        
        temp_ptr++;
    }
    
    if (line_width > total_width) {
        total_width = line_width;
    }
    
    uint16_t total_height = line_count * font->GetDefaultHeight();
    
    ST7735_FillRectangle(x, y, total_width, total_height, bgcolor);
    
    uint16_t current_x = x;
    uint16_t current_y = y;
    
    while (*unicode_str != 0) {
        std::shared_ptr<uint8_t[]> bitmap;
        uint16_t width, height;
        
        if (!font->LoadChar(*unicode_str, bitmap, &width, &height)) {
            width = font->GetDefaultWidth();
            height = font->GetDefaultHeight();
        }
        
        if (current_x + width > ST7735_WIDTH) {
            current_x = x;
            current_y += font->GetDefaultHeight();
            
            if (current_y + font->GetDefaultHeight() > ST7735_HEIGHT) {
                break;
            }
        }
        
        WriteUnicodeCharNoBg(current_x, current_y, *unicode_str, font, color);
        
        current_x += width;
        unicode_str++;
    }
}

bool IsUTF8ContinuationByte(uint8_t byte) {
    return (byte & 0xC0) == 0x80;
}

uint32_t UTF8ToUnicode(const char** utf8_str) {
    if (!utf8_str || !*utf8_str) return 0;
    
    const uint8_t* str = (const uint8_t*)*utf8_str;
    uint32_t unicode = 0;
    
    if ((str[0] & 0x80) == 0x00) {
        unicode = str[0];
        (*utf8_str)++;
    } else if ((str[0] & 0xE0) == 0xC0) {
        if (IsUTF8ContinuationByte(str[1])) {
            unicode = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
            (*utf8_str) += 2;
        }
    } else if ((str[0] & 0xF0) == 0xE0) {
        if (IsUTF8ContinuationByte(str[1]) && IsUTF8ContinuationByte(str[2])) {
            unicode = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
            (*utf8_str) += 3;
        }
    } else if ((str[0] & 0xF8) == 0xF0) {
        if (IsUTF8ContinuationByte(str[1]) && IsUTF8ContinuationByte(str[2]) && IsUTF8ContinuationByte(str[3])) {
            unicode = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
            (*utf8_str) += 4;
        }
    } else {
        (*utf8_str)++;
    }
    
    return unicode;
}

void WriteUnicodeStringUTF8(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color, uint16_t bgcolor) {
    if (!utf8_str || !font || !font->IsValid()) {
        printf("WriteUnicodeStringUTF8: 参数无效!\r\n");
        return;
    }
    
    if (FONT_RENDER_DEBUG_INFO) printf("开始渲染字符串: %s\r\n", utf8_str);
    
    uint16_t total_width = 0;
    uint16_t line_width = 0;
    uint16_t line_count = 1;
    
    const char* temp_ptr = utf8_str;
    while (*temp_ptr != '\0') {
        uint32_t unicode = UTF8ToUnicode(&temp_ptr);
        if (unicode == 0) break;
        
        uint16_t width;
        
        if (!font->GetCharWidth(unicode, &width)) {
            width = font->GetDefaultWidth();
        }
        
        uint16_t char_spacing = width + 1;
        
        if (unicode >= 0x2000 && unicode <= 0x206F) {
            char_spacing += 1;
        } else if (unicode >= 0x3000 && unicode <= 0x303F) {
            char_spacing += 1;
        } else if (unicode >= 0xFF00 && unicode <= 0xFFEF) {
            char_spacing += 1;
        } else if (unicode == 0x002C || unicode == 0x002E || unicode == 0x003B || 
                   unicode == 0x003A || unicode == 0x0021 || unicode == 0x003F) {
            char_spacing += 1;
        }
        
        if (line_width + char_spacing > ST7735_WIDTH) {
            if (line_width > total_width) {
                total_width = line_width;
            }
            line_width = char_spacing;
            line_count++;
        } else {
            line_width += char_spacing;
        }
    }
    
    if (line_width > total_width) {
        total_width = line_width;
    }
    
    uint16_t total_height = line_count * font->GetDefaultHeight();
    
    ST7735_FillRectangle(x, y, total_width, total_height, bgcolor);
    
    uint16_t current_x = x;
    uint16_t current_y = y;
    
    const char* ptr = utf8_str;
    while (*ptr != '\0') {
        uint32_t unicode = UTF8ToUnicode(&ptr);
        if (unicode == 0) {
            printf("UTF8转换失败!\r\n");
            break;
        }
        
        uint16_t width, height;
        
        if (unicode == 0x0020 || unicode == 0x00A0 || unicode == 0x2000 || unicode == 0x2001 || 
            unicode == 0x2002 || unicode == 0x2003 || unicode == 0x2004 || unicode == 0x2005 ||
            unicode == 0x2006 || unicode == 0x2007 || unicode == 0x2008 || unicode == 0x2009 ||
            unicode == 0x200A || unicode == 0x202F || unicode == 0x205F || unicode == 0x3000) {
            width = font->GetDefaultWidth();
        } else {
            std::shared_ptr<uint8_t[]> bitmap;
            
            if (!font->LoadChar(unicode, bitmap, &width, &height)) {
                printf("字符 U+%04lX 加载失败，使用默认尺寸\r\n", unicode);
                width = font->GetDefaultWidth();
                height = font->GetDefaultHeight();
            }
            
            if (current_x + width > ST7735_WIDTH) {
                current_x = x;
                current_y += font->GetDefaultHeight();
                
                if (current_y + font->GetDefaultHeight() > ST7735_HEIGHT) {
                    printf("超出屏幕范围，停止渲染\r\n");
                    break;
                }
            }
            
            WriteUnicodeCharNoBg(current_x, current_y, unicode, font, color);
        }
        
        uint16_t char_spacing = width + 1;
        
        if (unicode >= 0x2000 && unicode <= 0x206F) {
            char_spacing += 1;
        } else if (unicode >= 0x3000 && unicode <= 0x303F) {
            char_spacing += 1;
        } else if (unicode >= 0xFF00 && unicode <= 0xFFEF) {
            char_spacing += 1;
        } else if (unicode == 0x002C || unicode == 0x002E || unicode == 0x003B || 
                   unicode == 0x003A || unicode == 0x0021 || unicode == 0x003F) {
            char_spacing += 1;
        }
        
        current_x += char_spacing;
    }
}

void DrawPlaceholderBox(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if (x + width > ST7735_WIDTH || y + height > ST7735_HEIGHT) {
        return;
    }
    
    for (uint16_t i = 0; i < width; i++) {
        ST7735_DrawPixel(x + i, y, color);
        ST7735_DrawPixel(x + i, y + height - 1, color);
    }
    
    for (uint16_t i = 0; i < height; i++) {
        ST7735_DrawPixel(x, y + i, color);
        ST7735_DrawPixel(x + width - 1, y + i, color);
    }
    
    for (uint16_t i = 0; i < width && i < height; i++) {
        ST7735_DrawPixel(x + i, y + i, color);
        ST7735_DrawPixel(x + width - 1 - i, y + i, color);
    }
}

void WriteUnicodeCharNoBg(uint16_t x, uint16_t y, uint32_t unicode, UnicodeFont* font, uint16_t color) {
    if (!font || !font->IsValid()) return;
    
    if (unicode == 0x0020 || unicode == 0x00A0 || unicode == 0x2000 || unicode == 0x2001 || 
        unicode == 0x2002 || unicode == 0x2003 || unicode == 0x2004 || unicode == 0x2005 ||
        unicode == 0x2006 || unicode == 0x2007 || unicode == 0x2008 || unicode == 0x2009 ||
        unicode == 0x200A || unicode == 0x202F || unicode == 0x205F || unicode == 0x3000) {
        return;
    }
    
    std::shared_ptr<uint8_t[]> bitmap;
    uint16_t width, height;
    
    if (!font->LoadChar(unicode, bitmap, &width, &height)) {
        printf("WriteUnicodeCharNoBg: 字符 U+%04lX 不在字体中，绘制占位符方框!\r\n", unicode);
        DrawPlaceholderBox(x, y, font->GetDefaultWidth(), font->GetDefaultHeight(), color);
        return;
    }
    
    uint16_t baseline_offset = font->GetDefaultHeight() - 1;
    uint16_t char_baseline = height - 1;
    uint16_t render_y = y + baseline_offset - char_baseline;
    
    if (x + width > ST7735_WIDTH || render_y + height > ST7735_HEIGHT) {
        printf("WriteUnicodeCharNoBg: 位置超出屏幕范围!\r\n");
        return;
    }
    
    uint16_t pixel_count = 0;
    uint16_t bytes_per_row = (width + 7) / 8;
    
    for (uint16_t row = 0; row < height; row++) {
        const uint8_t* bitmap_row = bitmap.get() + row * bytes_per_row;
        
        for (uint16_t col = 0; col < width; col++) {
            uint8_t byte_index = col / 8;
            uint8_t bit_mask = 1 << (7 - (col % 8));
            
            if (bitmap_row[byte_index] & bit_mask) {
                ST7735_Select();
                ST7735_SetAddressWindow(x + col, render_y + row, x + col, render_y + row);
                ST7735_DC_HIGH();
                
                uint8_t data[2];
                data[0] = (color >> 8) & 0xFF;
                data[1] = color & 0xFF;
                HAL_SPI_Transmit(&ST7735_SPI_PORT, data, 2, HAL_MAX_DELAY);
                ST7735_Unselect();
                pixel_count++;
            }
        }
    }
    
    if (FONT_RENDER_DEBUG_INFO) printf("WriteUnicodeCharNoBg: 字符 U+%04lX 渲染完成，绘制了 %d 个像素\r\n", unicode, pixel_count);
}

void WriteUnicodeStringNoBg(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color) {
    if (!unicode_str || !font || !font->IsValid()) return;
    
    uint16_t current_x = x;
    uint16_t current_y = y;
    
    while (*unicode_str != 0) {
        std::shared_ptr<uint8_t[]> bitmap;
        uint16_t width, height;
        
        if (!font->LoadChar(*unicode_str, bitmap, &width, &height)) {
            width = font->GetDefaultWidth();
            height = font->GetDefaultHeight();
        }
        
        if (current_x + width > ST7735_WIDTH) {
            current_x = 0;
            current_y += font->GetDefaultHeight();
            
            if (current_y + font->GetDefaultHeight() > ST7735_HEIGHT) {
                break;
            }
        }
        
        WriteUnicodeCharNoBg(current_x, current_y, *unicode_str, font, color);
        
        current_x += width + 1;
        unicode_str++;
    }
}

void WriteUnicodeStringUTF8NoBg(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color) {
    if (!utf8_str || !font || !font->IsValid()) return;
    
    uint16_t current_x = x;
    uint16_t current_y = y;
    
    const char* ptr = utf8_str;
    
    while (*ptr != '\0') {
        uint32_t unicode = UTF8ToUnicode(&ptr);
        if (unicode == 0) break;
        
        std::shared_ptr<uint8_t[]> bitmap;
        uint16_t width, height;
        
        if (!font->LoadChar(unicode, bitmap, &width, &height)) {
            width = font->GetDefaultWidth();
            height = font->GetDefaultHeight();
        }
        
        if (current_x + width > ST7735_WIDTH) {
            current_x = 0;
            current_y += font->GetDefaultHeight();
            
            if (current_y + font->GetDefaultHeight() > ST7735_HEIGHT) {
                break;
            }
        }
        
        WriteUnicodeCharNoBg(current_x, current_y, unicode, font, color);
        
        current_x += width + 1;
    }
}

uint16_t UnicodeStringLength(const uint32_t* unicode_str, UnicodeFont* font) {
    if (!unicode_str || !font || !font->IsValid()) return 0;

    uint16_t total_width = 0;

    while (*unicode_str != 0) {
        uint16_t width;

        if (!font->GetCharWidth(*unicode_str, &width)) {
            width = font->GetDefaultWidth();
        }

        total_width += width + 1;
        unicode_str++;
    }

    return total_width > 0 ? total_width - 1 : 0;
}

uint16_t UnicodeStringUTF8Length(const char* utf8_str, UnicodeFont* font) {
    if (!utf8_str || !font || !font->IsValid()) return 0;

    uint16_t total_width = 0;
    const char* ptr = utf8_str;

    while (*ptr != '\0') {
        uint32_t unicode = UTF8ToUnicode(&ptr);
        if (unicode == 0) break;

        uint16_t width;

        if (!font->GetCharWidth(unicode, &width)) {
            width = font->GetDefaultWidth();
        }

        uint16_t char_spacing = width + 1;

        if (unicode >= 0x2000 && unicode <= 0x206F) {
            char_spacing += 1;
        } else if (unicode >= 0x3000 && unicode <= 0x303F) {
            char_spacing += 1;
        } else if (unicode >= 0xFF00 && unicode <= 0xFFEF) {
            char_spacing += 1;
        } else if (unicode == 0x002C || unicode == 0x002E || unicode == 0x003B ||
                   unicode == 0x003A || unicode == 0x0021 || unicode == 0x003F) {
            char_spacing += 1;
        }

        total_width += char_spacing;
    }
    return total_width > 0 ? total_width - 1 : 0;
}

static void DrawPixelDMA(uint16_t x, uint16_t y, uint16_t color) {
    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x, y);
    ST7735_DC_HIGH();
    
    uint8_t data[2];
    data[0] = (color >> 8) & 0xFF;
    data[1] = color & 0xFF;
    
    HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, data, 2);
    while (HAL_SPI_GetState(&ST7735_SPI_PORT) != HAL_SPI_STATE_READY);
    while (__HAL_SPI_GET_FLAG(&ST7735_SPI_PORT, SPI_FLAG_BSY));
    
    ST7735_Unselect();
}

static void FillRectDMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x + w > ST7735_WIDTH) w = ST7735_WIDTH - x;
    if (y + h > ST7735_HEIGHT) h = ST7735_HEIGHT - y;
    if (w == 0 || h == 0) return;
    
    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    ST7735_DC_HIGH();
    
    uint16_t* buffer = (uint16_t*)malloc(w * sizeof(uint16_t));
    if (!buffer) {
        ST7735_Unselect();
        return;
    }
    
    for (uint16_t i = 0; i < w; i++) {
        buffer[i] = color;
    }
    
    uint8_t* byte_buffer = (uint8_t*)buffer;
    for (uint16_t i = 0; i < w; i++) {
        uint8_t tmp = byte_buffer[i * 2];
        byte_buffer[i * 2] = byte_buffer[i * 2 + 1];
        byte_buffer[i * 2 + 1] = tmp;
    }
    
    for (uint16_t row = 0; row < h; row++) {
        HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, byte_buffer, w * 2);
        while (HAL_SPI_GetState(&ST7735_SPI_PORT) != HAL_SPI_STATE_READY);
        while (__HAL_SPI_GET_FLAG(&ST7735_SPI_PORT, SPI_FLAG_BSY));
    }
    
    free(buffer);
    ST7735_Unselect();
}

static void DrawPlaceholderBoxDMA(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    if (x + width > ST7735_WIDTH || y + height > ST7735_HEIGHT) return;
    
    for (uint16_t i = 0; i < width; i++) {
        DrawPixelDMA(x + i, y, color);
        DrawPixelDMA(x + i, y + height - 1, color);
    }
    
    for (uint16_t i = 0; i < height; i++) {
        DrawPixelDMA(x, y + i, color);
        DrawPixelDMA(x + width - 1, y + i, color);
    }
    
    for (uint16_t i = 0; i < width && i < height; i++) {
        DrawPixelDMA(x + i, y + i, color);
        DrawPixelDMA(x + width - 1 - i, y + i, color);
    }
}

void WriteUnicodeCharDMA(uint16_t x, uint16_t y, uint32_t unicode, UnicodeFont* font, uint16_t color, uint16_t bgcolor) {
    if (!font || !font->IsValid()) return;
    
    std::shared_ptr<uint8_t[]> bitmap;
    uint16_t width, height;
    
    if (!font->LoadChar(unicode, bitmap, &width, &height)) {
        DrawPlaceholderBoxDMA(x, y, font->GetDefaultWidth(), font->GetDefaultHeight(), color);
        return;
    }
    
    uint16_t baseline_offset = (font->GetDefaultHeight() * 8) / 10;
    uint16_t char_baseline = (height * 8) / 10;
    uint16_t render_y = y + baseline_offset - char_baseline;
    
    if (x + width > ST7735_WIDTH || render_y + height > ST7735_HEIGHT) return;
    
    uint16_t* row_buffer = (uint16_t*)malloc(width * sizeof(uint16_t));
    if (!row_buffer) return;
    
    for (uint16_t row = 0; row < height; row++) {
        for (uint16_t col = 0; col < width; col++) {
            row_buffer[col] = GetBitmapPixel(bitmap.get(), width, height, col, row) ? color : bgcolor;
        }
        
        ST7735_Select();
        ST7735_SetAddressWindow(x, render_y + row, x + width - 1, render_y + row);
        ST7735_DC_HIGH();
        
        uint8_t* byte_buffer = (uint8_t*)row_buffer;
        for (uint16_t i = 0; i < width; i++) {
            uint8_t tmp = byte_buffer[i * 2];
            byte_buffer[i * 2] = byte_buffer[i * 2 + 1];
            byte_buffer[i * 2 + 1] = tmp;
        }
        
        HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, byte_buffer, width * 2);
        while (HAL_SPI_GetState(&ST7735_SPI_PORT) != HAL_SPI_STATE_READY);
        while (__HAL_SPI_GET_FLAG(&ST7735_SPI_PORT, SPI_FLAG_BSY));
        
        ST7735_Unselect();
    }
    
    free(row_buffer);
}

void WriteUnicodeCharNoBgDMA(uint16_t x, uint16_t y, uint32_t unicode, UnicodeFont* font, uint16_t color) {
    if (!font || !font->IsValid()) return;
    
    if (unicode == 0x0020 || unicode == 0x00A0 || unicode == 0x2000 || unicode == 0x2001 ||
        unicode == 0x2002 || unicode == 0x2003 || unicode == 0x2004 || unicode == 0x2005 ||
        unicode == 0x2006 || unicode == 0x2007 || unicode == 0x2008 || unicode == 0x2009 ||
        unicode == 0x200A || unicode == 0x202F || unicode == 0x205F || unicode == 0x3000) {
        return;
    }
    
    std::shared_ptr<uint8_t[]> bitmap;
    uint16_t width, height;
    
    if (!font->LoadChar(unicode, bitmap, &width, &height)) {
        DrawPlaceholderBoxDMA(x, y, font->GetDefaultWidth(), font->GetDefaultHeight(), color);
        return;
    }
    
    uint16_t baseline_offset = font->GetDefaultHeight() - 1;
    uint16_t char_baseline = height - 1;
    uint16_t render_y = y + baseline_offset - char_baseline;
    
    if (x + width > ST7735_WIDTH || render_y + height > ST7735_HEIGHT) return;
    
    for (uint16_t row = 0; row < height; row++) {
        for (uint16_t col = 0; col < width; col++) {
            if (GetBitmapPixel(bitmap.get(), width, height, col, row)) {
                DrawPixelDMA(x + col, render_y + row, color);
            }
        }
    }
}

void WriteUnicodeStringDMA(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color, uint16_t bgcolor) {
    if (!unicode_str || !font || !font->IsValid()) return;
    
    uint16_t total_width = 0;
    uint16_t line_width = 0;
    uint16_t line_count = 1;
    
    const uint32_t* temp_ptr = unicode_str;
    while (*temp_ptr != 0) {
        std::shared_ptr<uint8_t[]> bitmap;
        uint16_t width, height;
        
        if (!font->LoadChar(*temp_ptr, bitmap, &width, &height)) {
            width = font->GetDefaultWidth();
        }
        
        if (line_width + width > ST7735_WIDTH) {
            if (line_width > total_width) total_width = line_width;
            line_width = width;
            line_count++;
        } else {
            line_width += width;
        }
        temp_ptr++;
    }
    
    if (line_width > total_width) total_width = line_width;
    uint16_t total_height = line_count * font->GetDefaultHeight();
    
    FillRectDMA(x, y, total_width, total_height, bgcolor);
    
    uint16_t current_x = x;
    uint16_t current_y = y;
    
    while (*unicode_str != 0) {
        std::shared_ptr<uint8_t[]> bitmap;
        uint16_t width, height;
        
        if (!font->LoadChar(*unicode_str, bitmap, &width, &height)) {
            width = font->GetDefaultWidth();
        }
        
        if (current_x + width > ST7735_WIDTH) {
            current_x = x;
            current_y += font->GetDefaultHeight();
            if (current_y + font->GetDefaultHeight() > ST7735_HEIGHT) break;
        }
        
        WriteUnicodeCharNoBgDMA(current_x, current_y, *unicode_str, font, color);
        current_x += width;
        unicode_str++;
    }
}

void WriteUnicodeStringNoBgDMA(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color) {
    if (!unicode_str || !font || !font->IsValid()) return;
    
    uint16_t current_x = x;
    uint16_t current_y = y;
    
    while (*unicode_str != 0) {
        std::shared_ptr<uint8_t[]> bitmap;
        uint16_t width, height;
        
        if (!font->LoadChar(*unicode_str, bitmap, &width, &height)) {
            width = font->GetDefaultWidth();
        }
        
        if (current_x + width > ST7735_WIDTH) {
            current_x = 0;
            current_y += font->GetDefaultHeight();
            if (current_y + font->GetDefaultHeight() > ST7735_HEIGHT) break;
        }
        
        WriteUnicodeCharNoBgDMA(current_x, current_y, *unicode_str, font, color);
        current_x += width + 1;
        unicode_str++;
    }
}

void WriteUnicodeStringUTF8DMA(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color, uint16_t bgcolor) {
    if (!utf8_str || !font || !font->IsValid()) return;
    
    uint16_t total_width = 0;
    uint16_t line_width = 0;
    uint16_t line_count = 1;
    
    const char* temp_ptr = utf8_str;
    while (*temp_ptr != '\0') {
        uint32_t unicode = UTF8ToUnicode(&temp_ptr);
        if (unicode == 0) break;
        
        uint16_t width, height;
        std::shared_ptr<uint8_t[]> bitmap;
        
        if (!font->LoadChar(unicode, bitmap, &width, &height)) {
            width = font->GetDefaultWidth();
        }
        
        uint16_t char_spacing = width + 1;
        if (unicode >= 0x2000 && unicode <= 0x206F) char_spacing += 1;
        else if (unicode >= 0x3000 && unicode <= 0x303F) char_spacing += 1;
        else if (unicode >= 0xFF00 && unicode <= 0xFFEF) char_spacing += 1;
        else if (unicode == 0x002C || unicode == 0x002E || unicode == 0x003B ||
                 unicode == 0x003A || unicode == 0x0021 || unicode == 0x003F) char_spacing += 1;
        
        if (line_width + char_spacing > ST7735_WIDTH) {
            if (line_width > total_width) total_width = line_width;
            line_width = char_spacing;
            line_count++;
        } else {
            line_width += char_spacing;
        }
    }
    
    if (line_width > total_width) total_width = line_width;
    uint16_t total_height = line_count * font->GetDefaultHeight();
    
    FillRectDMA(x, y, total_width, total_height, bgcolor);
    
    uint16_t current_x = x;
    uint16_t current_y = y;
    
    const char* ptr = utf8_str;
    while (*ptr != '\0') {
        uint32_t unicode = UTF8ToUnicode(&ptr);
        if (unicode == 0) break;
        
        uint16_t width, height;
        std::shared_ptr<uint8_t[]> bitmap;
        
        if (unicode == 0x0020 || unicode == 0x00A0 || unicode == 0x2000 || unicode == 0x2001 ||
            unicode == 0x2002 || unicode == 0x2003 || unicode == 0x2004 || unicode == 0x2005 ||
            unicode == 0x2006 || unicode == 0x2007 || unicode == 0x2008 || unicode == 0x2009 ||
            unicode == 0x200A || unicode == 0x202F || unicode == 0x205F || unicode == 0x3000) {
            width = font->GetDefaultWidth();
        } else {
            if (!font->LoadChar(unicode, bitmap, &width, &height)) {
                width = font->GetDefaultWidth();
            }
            
            if (current_x + width > ST7735_WIDTH) {
                current_x = x;
                current_y += font->GetDefaultHeight();
                if (current_y + font->GetDefaultHeight() > ST7735_HEIGHT) break;
            }
            
            WriteUnicodeCharNoBgDMA(current_x, current_y, unicode, font, color);
        }
        
        uint16_t char_spacing = width + 1;
        if (unicode >= 0x2000 && unicode <= 0x206F) char_spacing += 1;
        else if (unicode >= 0x3000 && unicode <= 0x303F) char_spacing += 1;
        else if (unicode >= 0xFF00 && unicode <= 0xFFEF) char_spacing += 1;
        else if (unicode == 0x002C || unicode == 0x002E || unicode == 0x003B ||
                 unicode == 0x003A || unicode == 0x0021 || unicode == 0x003F) char_spacing += 1;
        
        current_x += char_spacing;
    }
}

void WriteUnicodeStringUTF8NoBgDMA(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color) {
    if (!utf8_str || !font || !font->IsValid()) return;
    
    uint16_t current_x = x;
    uint16_t current_y = y;
    
    const char* ptr = utf8_str;
    
    while (*ptr != '\0') {
        uint32_t unicode = UTF8ToUnicode(&ptr);
        if (unicode == 0) break;
        
        std::shared_ptr<uint8_t[]> bitmap;
        uint16_t width, height;
        
        if (!font->LoadChar(unicode, bitmap, &width, &height)) {
            width = font->GetDefaultWidth();
        }
        
        if (current_x + width > ST7735_WIDTH) {
            current_x = 0;
            current_y += font->GetDefaultHeight();
            if (current_y + font->GetDefaultHeight() > ST7735_HEIGHT) break;
        }
        
        WriteUnicodeCharNoBgDMA(current_x, current_y, unicode, font, color);
        current_x += width + 1;
    }
}
