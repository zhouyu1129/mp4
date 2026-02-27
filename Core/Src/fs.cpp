//
// Created by 周煜 on 2026/1/21.
//

#include "fs.h"
#include <cstring>
#include <cctype>

namespace fs {
    void utf8_to_gbk(const char* utf8_str, char* gbk_buf, size_t buf_size) {
        if (!utf8_str || !gbk_buf || buf_size == 0) return;
        
        size_t gbk_pos = 0;
        const auto* p = reinterpret_cast<const unsigned char*>(utf8_str);
        
        while (*p && gbk_pos < buf_size - 1) {
            uint32_t unicode = 0;
            
            if ((*p & 0x80) == 0) {
                unicode = *p++;
            } else if ((*p & 0xE0) == 0xC0) {
                if ((p[1] & 0xC0) != 0x80) { p++; continue; }
                unicode = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
                p += 2;
            } else if ((*p & 0xF0) == 0xE0) {
                if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) { p++; continue; }
                unicode = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
                p += 3;
            } else if ((*p & 0xF8) == 0xF0) {
                if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) { p++; continue; }
                unicode = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
                p += 4;
            } else {
                p++;
                continue;
            }
            
            if (unicode < 0x80) {
                if (gbk_pos < buf_size - 1) {
                    gbk_buf[gbk_pos++] = static_cast<char>(unicode);
                }
            } else {
                WCHAR gbk = ff_convert(static_cast<WCHAR>(unicode), 0);
                if (gbk == 0) {
                    if (gbk_pos < buf_size - 1) {
                        gbk_buf[gbk_pos++] = '?';
                    }
                } else if (gbk < 0x100) {
                    if (gbk_pos < buf_size - 1) {
                        gbk_buf[gbk_pos++] = static_cast<char>(gbk);
                    }
                } else {
                    if (gbk_pos < buf_size - 2) {
                        gbk_buf[gbk_pos++] = static_cast<char>(gbk >> 8);
                        gbk_buf[gbk_pos++] = static_cast<char>(gbk & 0xFF);
                    }
                }
            }
        }
        gbk_buf[gbk_pos] = '\0';
    }

    void gbk_to_utf8(const char* gbk_str, char* utf8_buf, size_t buf_size) {
        if (!gbk_str || !utf8_buf || buf_size == 0) return;
        
        size_t utf8_pos = 0;
        const auto* p = reinterpret_cast<const unsigned char*>(gbk_str);
        
        while (*p && utf8_pos < buf_size - 1) {
            uint32_t unicode = 0;
            
            if (*p < 0x80) {
                utf8_buf[utf8_pos++] = *p++;
            } else {
                WCHAR gbk_code;
                if (p[0] >= 0x81 && p[0] <= 0xFE && p[1] >= 0x40 && p[1] <= 0xFE && p[1] != 0x7F) {
                    gbk_code = (static_cast<WCHAR>(p[0]) << 8) | p[1];
                    p += 2;
                } else {
                    utf8_buf[utf8_pos++] = '?';
                    p++;
                    continue;
                }
                
                unicode = ff_convert(gbk_code, 1);
                if (unicode == 0) {
                    utf8_buf[utf8_pos++] = '?';
                    continue;
                }
                
                if (unicode < 0x80) {
                    if (utf8_pos < buf_size - 1) {
                        utf8_buf[utf8_pos++] = static_cast<char>(unicode);
                    }
                } else if (unicode < 0x800) {
                    if (utf8_pos < buf_size - 2) {
                        utf8_buf[utf8_pos++] = static_cast<char>(0xC0 | (unicode >> 6));
                        utf8_buf[utf8_pos++] = static_cast<char>(0x80 | (unicode & 0x3F));
                    }
                } else if (unicode < 0x10000) {
                    if (utf8_pos < buf_size - 3) {
                        utf8_buf[utf8_pos++] = static_cast<char>(0xE0 | (unicode >> 12));
                        utf8_buf[utf8_pos++] = static_cast<char>(0x80 | ((unicode >> 6) & 0x3F));
                        utf8_buf[utf8_pos++] = static_cast<char>(0x80 | (unicode & 0x3F));
                    }
                } else {
                    if (utf8_pos < buf_size - 4) {
                        utf8_buf[utf8_pos++] = static_cast<char>(0xF0 | (unicode >> 18));
                        utf8_buf[utf8_pos++] = static_cast<char>(0x80 | ((unicode >> 12) & 0x3F));
                        utf8_buf[utf8_pos++] = static_cast<char>(0x80 | ((unicode >> 6) & 0x3F));
                        utf8_buf[utf8_pos++] = static_cast<char>(0x80 | (unicode & 0x3F));
                    }
                }
            }
        }
        utf8_buf[utf8_pos] = '\0';
    }

    bool suffix_matches(const char* str, const char* suffix) {
        auto len1 = strlen(str), len2 = strlen(suffix);
        if (len1 < len2) return false;
        for (uint32_t i = 0; i < len2; i++) {
            if (tolower(str[len1 - len2 + i]) != tolower(suffix[i])) return false;
        }
        return true;
    }

    Object::Object(ObjectType type, const char* name) : type(type), name(nullptr) {
        if (name) {
            // strdup 内部使用 malloc 分配内存并复制字符串
            // 注意：在某些嵌入式环境可能没有 strdup，需自行实现 malloc+strcpy
            this->name = strdup(name);
        }
    }

    Object::Object(Object&& other) noexcept : type(other.type), name(other.name) {
        other.name = nullptr; // 接管指针，置空源对象
    }

    Object::~Object() {
        if (name) {
            free((void*)name);
        }
    }

    void DirIterator::next() {
        auto res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) {
            // 读取错误或到达目录末尾
            is_end = true;
            f_closedir(&dir); // 关闭目录
        }
    }

    DirIterator::DirIterator() : utf8_name{}, is_end(true), convert_to_utf8(true) {}

    DirIterator::DirIterator(const char* path, bool convert_utf8) 
        : utf8_name{}, is_end(false), convert_to_utf8(convert_utf8) {
        FRESULT res = f_opendir(&dir, path);
        if (res != FR_OK) {
            is_end = true;
        } else {
            next();
        }
    }

    DirIterator::~DirIterator() {
        if (!is_end) {
            f_closedir(&dir);
        }
    }

    DirIterator& DirIterator::operator++() {
        if (!is_end) {
            next();
        }
        return *this;
    }

    DirIterator DirIterator::operator++(int) {
        DirIterator tmp = *this;
        ++(*this);
        return tmp;
    }

    DirIterator& DirIterator::operator+=(uint32_t n) {
        while (n--) next();
        return *this;
    }

    DirIterator DirIterator::operator+(uint32_t n) const {
        DirIterator tmp = *this;
        tmp += n;
        return tmp;
    }

    Object DirIterator::operator*() const {
        const auto t = (fno.fattrib & AM_DIR) ? ObjectType::dir : file;
        if (convert_to_utf8) {
            gbk_to_utf8(fno.fname, const_cast<char*>(utf8_name), sizeof(utf8_name));
            return {t, utf8_name};
        } else {
            return {t, fno.fname};
        }
    }

    bool DirIterator::operator!=(const DirIterator& other) const {
        return is_end != other.is_end;
    }

    DirectoryRange::DirectoryRange(const char* p, bool convert_utf8) 
        : path(p), convert_to_utf8(convert_utf8) {}

    DirIterator DirectoryRange::begin() const {
        return DirIterator(path, convert_to_utf8);
    }

    DirIterator DirectoryRange::end() {
        return {};
    }
}
