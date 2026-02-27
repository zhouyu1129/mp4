#include "unicode_font_types.h"
#include "ff.h"
#include <cstdio>

SimpleCharIndex::SimpleCharIndex(int max_entries) : entries(nullptr), entry_count(0), max_entries(max_entries) {
    entries = new UnicodeCharEntry[max_entries];
}

SimpleCharIndex::~SimpleCharIndex() {
    Clear();
    delete[] entries;
}

bool SimpleCharIndex::Insert(uint32_t unicode, const UnicodeCharInfo& info) {
    if (entry_count >= max_entries) {
        return false;
    }

    if (entry_count == 0 || unicode > entries[entry_count - 1].unicode) {
        entries[entry_count] = UnicodeCharEntry(unicode, info);
        entry_count++;
        return true;
    }

    int left = 0;
    int right = entry_count - 1;
    int insert_pos = entry_count;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (entries[mid].unicode == unicode) {
            return false;
        }

        if (entries[mid].unicode < unicode) {
            left = mid + 1;
        }
        else {
            insert_pos = mid;
            right = mid - 1;
        }
    }

    for (int i = entry_count; i > insert_pos; i--) {
        entries[i] = entries[i - 1];
    }

    entries[insert_pos] = UnicodeCharEntry(unicode, info);
    entry_count++;

    return true;
}

bool SimpleCharIndex::Search(uint32_t unicode, UnicodeCharInfo& info) const {
    int left = 0;
    int right = entry_count - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (entries[mid].unicode == unicode) {
            info = entries[mid].info;
            return true;
        }

        if (entries[mid].unicode < unicode) {
            left = mid + 1;
        }
        else {
            right = mid - 1;
        }
    }

    return false;
}

void SimpleCharIndex::Clear() {
    entry_count = 0;
}

LRUCache::LRUCache(int capacity) : capacity(capacity), size(0), head(nullptr), tail(nullptr) {
}

LRUCache::~LRUCache() {
    Clear();
}

void LRUCache::MoveToFront(CacheEntry* entry) {
    if (entry == head) return;

    if (entry->prev) entry->prev->next = entry->next;
    if (entry->next) entry->next->prev = entry->prev;
    if (entry == tail) tail = entry->prev;

    entry->prev = nullptr;
    entry->next = head;
    if (head) head->prev = entry;
    head = entry;
    if (!tail) tail = head;
}

void LRUCache::RemoveTail() {
    if (!tail) return;

    CacheEntry* toRemove = tail;
    tail = tail->prev;
    if (tail) tail->next = nullptr;
    else head = nullptr;

    delete toRemove;
    size--;
}

bool LRUCache::Get(uint32_t unicode, std::shared_ptr<uint8_t[]>& bitmap) {
    CacheEntry* current = head;
    while (current) {
        if (current->unicode == unicode) {
            bitmap = current->bitmap;
            MoveToFront(current);
            return true;
        }
        current = current->next;
    }
    return false;
}

bool LRUCache::Put(uint32_t unicode, const std::shared_ptr<uint8_t[]>& bitmap) {
    CacheEntry* current = head;
    while (current) {
        if (current->unicode == unicode) {
            current->bitmap = bitmap;
            MoveToFront(current);
            return true;
        }
        current = current->next;
    }

    if (size >= capacity) {
        RemoveTail();
    }

    auto* newEntry = new CacheEntry(unicode, bitmap);
    newEntry->next = head;
    if (head) head->prev = newEntry;
    head = newEntry;
    if (!tail) tail = head;
    size++;

    return true;
}

void LRUCache::Clear() {
    CacheEntry* current = head;
    while (current) {
        CacheEntry* next = current->next;
        delete current;
        current = next;
    }
    head = tail = nullptr;
    size = 0;
}

UnicodeFont::UnicodeFont() : char_index(1000), cache(LRU_CACHE_SIZE), default_width(0), default_height(0),
                             font_file_size(0), initialized(false), use_index_cache(true), char_count(0) {
    font_path[0] = '\0';
}

UnicodeFont::~UnicodeFont() {
    if (initialized) {
        cache.Clear();
        char_index.Clear();
    }
}

bool UnicodeFont::Load(const char* path, int cache_size) {
    if (initialized) {
        printf("字体已加载，请先卸载!\r\n");
        return false;
    }

    strncpy(font_path, path, sizeof(font_path) - 1);
    font_path[sizeof(font_path) - 1] = '\0';

    printf("开始加载字体: %s\r\n", font_path);

    FIL font_file;
    if (f_open(&font_file, font_path, FA_READ) != FR_OK) {
        printf("字体文件打开失败!\r\n");
        return false;
    }

    font_file_size = f_size(&font_file);

    if (!ParseFontHeader(&font_file)) {
        f_close(&font_file);
        return false;
    }

    if (!ParseCharIndex(&font_file)) {
        f_close(&font_file);
        return false;
    }

    f_close(&font_file);

    printf("字体加载完成!\r\n");
    initialized = true;
    return true;
}

bool UnicodeFont::ParseFontHeader(FIL* file) {
    uint8_t header[8];
    UINT bytes_read;

    if (f_read(file, header, 8, &bytes_read) != FR_OK || bytes_read != 8) {
        printf("读取字体头失败，读取字节数: %u\r\n", bytes_read);
        return false;
    }

    if (header[0] != 0x55 || header[1] != 0x46 || header[2] != 0x4E || header[3] != 0x54) {
        printf("字体头签名错误!\r\n");
        return false;
    }

    default_width = (header[4] << 8) | header[5];
    default_height = (header[6] << 8) | header[7];
    return true;
}

bool UnicodeFont::ParseCharIndex(FIL* file) {
    uint8_t char_count_bytes[4];
    UINT bytes_read;

    if (f_read(file, char_count_bytes, 4, &bytes_read) != FR_OK || bytes_read != 4) {
        printf("读取字符数量失败，读取字节数: %u\r\n", bytes_read);
        return false;
    }

    uint32_t char_count = (char_count_bytes[0] << 24) | (char_count_bytes[1] << 16) |
        (char_count_bytes[2] << 8) | char_count_bytes[3];

    if (char_count <= 1000) {
        for (uint32_t i = 0; i < char_count; i++) {
            uint8_t unicode_bytes[4];
            UnicodeCharInfo info;

            if (f_read(file, unicode_bytes, 4, &bytes_read) != FR_OK || bytes_read != 4) {
                printf("读取字符 %lu 的Unicode码失败\r\n", i);
                return false;
            }

            uint32_t unicode = (unicode_bytes[0] << 24) | (unicode_bytes[1] << 16) |
                (unicode_bytes[2] << 8) | unicode_bytes[3];

            uint8_t info_bytes[12];
            if (f_read(file, info_bytes, sizeof(info_bytes), &bytes_read) != FR_OK ||
                bytes_read != sizeof(info_bytes)) {
                printf("读取字符 %lu 的索引信息失败，读取字节数: %u\r\n", i, bytes_read);
                return false;
            }

            info.width = (info_bytes[0] << 8) | info_bytes[1];
            info.height = (info_bytes[2] << 8) | info_bytes[3];
            info.data_offset = (info_bytes[4] << 24) | (info_bytes[5] << 16) |
                (info_bytes[6] << 8) | info_bytes[7];
            info.data_size = (info_bytes[8] << 24) | (info_bytes[9] << 16) |
                (info_bytes[10] << 8) | info_bytes[11];

            if (i < char_count) {
                if (!char_index.Insert(unicode, info)) {
                    printf("插入字符 %lu (U+%04lX) 到索引失败\r\n", i, unicode);
                    return false;
                }
            }
        }
    }

    printf("字符索引解析完成，共 %lu 个字符\r\n", char_count);

    this->char_count = char_count;

    if (char_count > 1000) {
        use_index_cache = false;
        printf("字符数超过1000，禁用索引缓存，使用直接文件读取模式\r\n");
        char_index.Clear();
    }
    else {
        use_index_cache = true;
        printf("使用索引缓存模式\r\n");
    }

    return true;
}

bool UnicodeFont::LoadChar(uint32_t unicode, std::shared_ptr<uint8_t[]>& bitmap, uint16_t* width, uint16_t* height) {
    if (!initialized) {
        printf("LoadChar: 字体未初始化!\r\n");
        return false;
    }

    if (use_index_cache) {
        if (cache.Get(unicode, bitmap)) {
            if constexpr (FONT_DEBUG_INFO) printf("LoadChar: 字符 U+%04lX 从缓存加载\r\n", unicode);
            UnicodeCharInfo info;
            if (char_index.Search(unicode, info)) {
                *width = info.width;
                *height = info.height;
            }
            else {
                *width = default_width;
                *height = default_height;
            }
            return true;
        }

        UnicodeCharInfo info;
        if (!char_index.Search(unicode, info)) {
            printf("LoadChar: 字符 U+%04lX 不在索引中!\r\n", unicode);
            return false;
        }

        if (strlen(font_path) == 0) {
            printf("LoadChar: 字体路径为空!\r\n");
            return false;
        }

        static FIL font_file;
        static bool file_opened = false;

        if (!file_opened) {
            FRESULT open_result = f_open(&font_file, font_path, FA_READ);
            if (open_result != FR_OK) {
                printf("LoadChar: 打开文件失败! 错误码: %d, 路径: %s\r\n", open_result, font_path);
                return false;
            }
            file_opened = true;
        }

        FRESULT seek_result = f_lseek(&font_file, info.data_offset);
        if (seek_result != FR_OK) {
            printf("LoadChar: 文件定位失败! 错误码: %d, 偏移量: %lu\r\n", seek_result, info.data_offset);
            return false;
        }

        uint32_t bitmap_size = ((info.width + 7) / 8) * info.height;

        FSIZE_t file_size = f_size(&font_file);
        if (info.data_offset + bitmap_size > file_size) {
            printf("LoadChar: 位图数据超出文件范围! 偏移: %lu, 大小: %lu, 文件大小: %lu\r\n",
                   info.data_offset, bitmap_size, file_size);
            return false;
        }

        std::shared_ptr<uint8_t[]> char_bitmap(new uint8_t[bitmap_size]);

        UINT bytes_read;
        FRESULT read_result = f_read(&font_file, char_bitmap.get(), bitmap_size, &bytes_read);
        if (read_result != FR_OK || bytes_read != bitmap_size) {
            printf("LoadChar: 读取位图失败! 错误码: %d, 期望字节数: %lu, 实际读取: %u\r\n",
                   read_result, bitmap_size, bytes_read);
            return false;
        }

        if (!cache.Put(unicode, char_bitmap)) {
            printf("LoadChar: 缓存插入失败!\r\n");
            return false;
        }

        bitmap = char_bitmap;
        *width = info.width;
        *height = info.height;

        if constexpr (FONT_DEBUG_INFO) printf("LoadChar: 字符 U+%04lX 加载成功!\r\n", unicode);
        return true;
    }
    else {
        return LoadCharFromFile(unicode, bitmap, width, height);
    }
}

bool UnicodeFont::LoadCharFromFile(uint32_t unicode, std::shared_ptr<uint8_t[]>& bitmap, uint16_t* width,
                                   uint16_t* height) {
    if (!initialized) {
        printf("LoadCharFromFile: 字体未初始化!\r\n");
        return false;
    }

    if constexpr (FONT_DEBUG_INFO) printf("LoadCharFromFile: 直接文件读取字符 U+%04lX\r\n", unicode);

    if (strlen(font_path) == 0) {
        printf("LoadCharFromFile: 字体路径为空!\r\n");
        return false;
    }

    FIL font_file;
    FRESULT open_result = f_open(&font_file, font_path, FA_READ);
    if (open_result != FR_OK) {
        printf("LoadCharFromFile: 打开文件失败! 错误码: %d, 路径: %s\r\n", open_result, font_path);
        return false;
    }

    uint8_t header[8];
    UINT bytes_read;
    if (f_read(&font_file, header, 8, &bytes_read) != FR_OK || bytes_read != 8) {
        printf("LoadCharFromFile: 读取字体头失败\r\n");
        f_close(&font_file);
        return false;
    }

    uint8_t char_count_bytes[4];
    if (f_read(&font_file, char_count_bytes, 4, &bytes_read) != FR_OK || bytes_read != 4) {
        printf("LoadCharFromFile: 读取字符数量失败\r\n");
        f_close(&font_file);
        return false;
    }

    uint32_t file_char_count = (char_count_bytes[0] << 24) | (char_count_bytes[1] << 16) |
        (char_count_bytes[2] << 8) | char_count_bytes[3];

    uint32_t data_offset = 0;
    uint16_t char_width = default_width;
    uint16_t char_height = default_height;
    bool found = false;

    uint32_t left = 0;
    uint32_t right = file_char_count - 1;

    while (left <= right) {
        uint32_t mid = left + (right - left) / 2;

        uint32_t mid_offset = 8 + 4 + mid * 16;

        FRESULT seek_result = f_lseek(&font_file, mid_offset);
        if (seek_result != FR_OK) {
            printf("LoadCharFromFile: 定位中间字符失败! 错误码: %d, 偏移量: %lu\r\n", seek_result, mid_offset);
            f_close(&font_file);
            return false;
        }

        uint8_t unicode_bytes[4];
        if (f_read(&font_file, unicode_bytes, 4, &bytes_read) != FR_OK || bytes_read != 4) {
            printf("LoadCharFromFile: 读取中间字符的Unicode码失败\r\n");
            f_close(&font_file);
            return false;
        }

        uint32_t current_unicode = (unicode_bytes[0] << 24) | (unicode_bytes[1] << 16) |
            (unicode_bytes[2] << 8) | unicode_bytes[3];

        if (current_unicode == unicode) {
            uint8_t info_bytes[12];
            if (f_read(&font_file, info_bytes, sizeof(info_bytes), &bytes_read) != FR_OK ||
                bytes_read != sizeof(info_bytes)) {
                printf("LoadCharFromFile: 读取字符信息失败\r\n");
                f_close(&font_file);
                return false;
            }

            char_width = (info_bytes[0] << 8) | info_bytes[1];
            char_height = (info_bytes[2] << 8) | info_bytes[3];
            data_offset = (info_bytes[4] << 24) | (info_bytes[5] << 16) |
                (info_bytes[6] << 8) | info_bytes[7];
            found = true;
            break;
        }
        else if (current_unicode < unicode) {
            left = mid + 1;
        }
        else {
            right = mid - 1;
        }
    }

    if (!found) {
        printf("LoadCharFromFile: 字符 U+%04lX 不在字体中!\r\n", unicode);
        f_close(&font_file);
        return false;
    }

    FRESULT seek_result = f_lseek(&font_file, data_offset);
    if (seek_result != FR_OK) {
        printf("LoadCharFromFile: 文件定位失败! 错误码: %d, 偏移量: %lu\r\n", seek_result, data_offset);
        f_close(&font_file);
        return false;
    }

    uint32_t bitmap_size = ((char_width + 7) / 8) * char_height;

    FSIZE_t file_size = f_size(&font_file);
    if (data_offset + bitmap_size > file_size) {
        printf("LoadCharFromFile: 位图数据超出文件范围!\r\n");
        f_close(&font_file);
        return false;
    }

    std::shared_ptr<uint8_t[]> char_bitmap(new uint8_t[bitmap_size]);

    FRESULT read_result = f_read(&font_file, char_bitmap.get(), bitmap_size, &bytes_read);
    if (read_result != FR_OK || bytes_read != bitmap_size) {
        printf("LoadCharFromFile: 读取位图失败! 错误码: %d, 期望字节数: %lu, 实际读取: %u\r\n",
               read_result, bitmap_size, bytes_read);
        f_close(&font_file);
        return false;
    }

    f_close(&font_file);

    bitmap = char_bitmap;
    *width = char_width;
    *height = char_height;

    if constexpr (FONT_DEBUG_INFO) printf("LoadCharFromFile: 字符 U+%04lX 加载成功!\r\n", unicode);
    return true;
}

bool UnicodeFont::GetCharWidth(uint32_t unicode, uint16_t* width) const {
    if (!initialized) {
        return false;
    }

    if (unicode == 0x0020 || unicode == 0x00A0 || unicode == 0x2000 || unicode == 0x2001 || 
        unicode == 0x2002 || unicode == 0x2003 || unicode == 0x2004 || unicode == 0x2005 ||
        unicode == 0x2006 || unicode == 0x2007 || unicode == 0x2008 || unicode == 0x2009 ||
        unicode == 0x200A || unicode == 0x202F || unicode == 0x205F || unicode == 0x3000) {
        *width = default_width;
        return true;
    }

    if (use_index_cache) {
        UnicodeCharInfo info;
        if (char_index.Search(unicode, info)) {
            *width = info.width;
            return true;
        }
        *width = default_width;
        return false;
    }
    else {
        FIL font_file;
        FRESULT open_result = f_open(&font_file, font_path, FA_READ);
        if (open_result != FR_OK) {
            *width = default_width;
            return false;
        }

        uint8_t header[8];
        UINT bytes_read;
        if (f_read(&font_file, header, 8, &bytes_read) != FR_OK || bytes_read != 8) {
            f_close(&font_file);
            *width = default_width;
            return false;
        }

        uint8_t char_count_bytes[4];
        if (f_read(&font_file, char_count_bytes, 4, &bytes_read) != FR_OK || bytes_read != 4) {
            f_close(&font_file);
            *width = default_width;
            return false;
        }

        uint32_t file_char_count = (char_count_bytes[0] << 24) | (char_count_bytes[1] << 16) |
            (char_count_bytes[2] << 8) | char_count_bytes[3];

        uint32_t left = 0;
        uint32_t right = file_char_count - 1;
        bool found = false;

        while (left <= right) {
            uint32_t mid = left + (right - left) / 2;

            uint32_t mid_offset = 8 + 4 + mid * 16;

            FRESULT seek_result = f_lseek(&font_file, mid_offset);
            if (seek_result != FR_OK) {
                f_close(&font_file);
                *width = default_width;
                return false;
            }

            uint8_t unicode_bytes[4];
            if (f_read(&font_file, unicode_bytes, 4, &bytes_read) != FR_OK || bytes_read != 4) {
                f_close(&font_file);
                *width = default_width;
                return false;
            }

            uint32_t current_unicode = (unicode_bytes[0] << 24) | (unicode_bytes[1] << 16) |
                (unicode_bytes[2] << 8) | unicode_bytes[3];

            if (current_unicode == unicode) {
                uint8_t info_bytes[12];
                if (f_read(&font_file, info_bytes, sizeof(info_bytes), &bytes_read) != FR_OK ||
                    bytes_read != sizeof(info_bytes)) {
                    f_close(&font_file);
                    *width = default_width;
                    return false;
                }

                *width = (info_bytes[0] << 8) | info_bytes[1];
                found = true;
                break;
            }
            else if (current_unicode < unicode) {
                left = mid + 1;
            }
            else {
                right = mid - 1;
            }
        }

        f_close(&font_file);

        if (!found) {
            *width = default_width;
            return false;
        }

        return true;
    }
}
