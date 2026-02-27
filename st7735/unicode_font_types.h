#ifndef UNICODE_FONT_TYPES_H
#define UNICODE_FONT_TYPES_H

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#include "ff.h"

#define LRU_CACHE_SIZE 20
#define FONT_DEBUG_INFO false

struct UnicodeCharInfo {
    uint16_t width = 0;
    uint16_t height = 0;
    uint32_t data_offset = 0;
    uint32_t data_size = 0;
};

struct UnicodeCharEntry {
    uint32_t unicode;
    UnicodeCharInfo info;
    
    UnicodeCharEntry() : unicode(0) {}
    UnicodeCharEntry(uint32_t u, const UnicodeCharInfo& i) : unicode(u), info(i) {}
};

class SimpleCharIndex {
private:
    UnicodeCharEntry* entries;
    int entry_count;
    int max_entries;
    
public:
    SimpleCharIndex(int max_entries = 1000);
    ~SimpleCharIndex();
    
    bool Insert(uint32_t unicode, const UnicodeCharInfo& info);
    bool Search(uint32_t unicode, UnicodeCharInfo& info) const;
    void Clear();
    
    [[nodiscard]] int GetEntryCount() const { return entry_count; }
    [[nodiscard]] bool IsEmpty() const { return entry_count == 0; }
};

class LRUCache {
private:
    struct CacheEntry {
        uint32_t unicode;
        std::shared_ptr<uint8_t[]> bitmap;
        CacheEntry* prev;
        CacheEntry* next;
        
        CacheEntry(uint32_t u, std::shared_ptr<uint8_t[]> b) 
            : unicode(u), bitmap(std::move(b)), prev(nullptr), next(nullptr) {}
    };
    
    int capacity;
    int size;
    CacheEntry* head;
    CacheEntry* tail;
    
    void MoveToFront(CacheEntry* entry);
    void RemoveTail();
    
public:
    explicit LRUCache(int capacity = LRU_CACHE_SIZE);
    ~LRUCache();
    
    bool Get(uint32_t unicode, std::shared_ptr<uint8_t[]>& bitmap);
    bool Put(uint32_t unicode, const std::shared_ptr<uint8_t[]>& bitmap);
    void Clear();
    
    [[nodiscard]] int GetCacheSize() const { return size; }
    [[nodiscard]] int GetMaxCacheSize() const { return capacity; }
};

class UnicodeFont {
private:
    char font_path[256]{};
    SimpleCharIndex char_index;
    LRUCache cache;
    uint16_t default_width;
    uint16_t default_height;
    uint32_t font_file_size;
    bool initialized;
    bool use_index_cache;
    uint32_t char_count;
    
    bool ParseFontHeader(FIL* file);
    bool ParseCharIndex(FIL* file);
    bool LoadCharFromFile(uint32_t unicode, std::shared_ptr<uint8_t[]>& bitmap, uint16_t* width, uint16_t* height);
    
public:
    UnicodeFont();
    ~UnicodeFont();
    
    bool Load(const char* path, int cache_size = LRU_CACHE_SIZE);
    [[nodiscard]] bool IsValid() const { return initialized; }
    bool LoadChar(uint32_t unicode, std::shared_ptr<uint8_t[]>& bitmap, uint16_t* width, uint16_t* height);
    bool GetCharWidth(uint32_t unicode, uint16_t* width) const;
    [[nodiscard]] uint16_t GetDefaultWidth() const { return default_width; }
    [[nodiscard]] uint16_t GetDefaultHeight() const { return default_height; }
    [[nodiscard]] bool UsesIndexCache() const { return use_index_cache; }
    [[nodiscard]] uint32_t GetCharCount() const { return char_count; }
};

#endif // UNICODE_FONT_TYPES_H
