#ifndef SD_AND_LCD2_UNICODE_RENDER_H
#define SD_AND_LCD2_UNICODE_RENDER_H

#include <stdint.h>
#include <stdbool.h>
#include "unicode_font_types.h"

#define FONT_RENDER_DEBUG_INFO false

#ifdef __cplusplus
extern "C" {
#endif

void WriteUnicodeChar(uint16_t x, uint16_t y, uint32_t unicode, UnicodeFont* font, uint16_t color, uint16_t bgcolor);
void WriteUnicodeCharNoBg(uint16_t x, uint16_t y, uint32_t unicode, UnicodeFont* font, uint16_t color);
void WriteUnicodeString(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color, uint16_t bgcolor);
void WriteUnicodeStringNoBg(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color);
void WriteUnicodeStringUTF8(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color, uint16_t bgcolor);
void WriteUnicodeStringUTF8NoBg(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color);
void DrawPlaceholderBox(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

void WriteUnicodeCharDMA(uint16_t x, uint16_t y, uint32_t unicode, UnicodeFont* font, uint16_t color, uint16_t bgcolor);
void WriteUnicodeCharNoBgDMA(uint16_t x, uint16_t y, uint32_t unicode, UnicodeFont* font, uint16_t color);
void WriteUnicodeStringDMA(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color, uint16_t bgcolor);
void WriteUnicodeStringNoBgDMA(uint16_t x, uint16_t y, const uint32_t* unicode_str, UnicodeFont* font, uint16_t color);
void WriteUnicodeStringUTF8DMA(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color, uint16_t bgcolor);
void WriteUnicodeStringUTF8NoBgDMA(uint16_t x, uint16_t y, const char* utf8_str, UnicodeFont* font, uint16_t color);

uint32_t UTF8ToUnicode(const char** utf8_str);
bool IsUTF8ContinuationByte(uint8_t byte);

uint16_t UnicodeStringLength(const uint32_t* unicode_str, UnicodeFont* font);
uint16_t UnicodeStringUTF8Length(const char* utf8_str, UnicodeFont* font);

#ifdef __cplusplus
}
#endif

#endif