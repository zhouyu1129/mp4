//
// Created by 周煜 on 2026/1/21.
//

#ifndef FS_H
#define FS_H
#include "ff.h"

namespace fs {
    enum ObjectType {
        file,
        dir,
    };

    void utf8_to_gbk(const char* utf8_str, char* gbk_buf, size_t buf_size);
    void gbk_to_utf8(const char* gbk_str, char* utf8_buf, size_t buf_size);

    bool suffix_matches(const char* str, const char* suffix);

    class Object {
    public:
        const ObjectType type;
        const char* name;
        Object(ObjectType type, const char* name);
        // 禁用拷贝构造（避免双重释放）
        Object(const Object&) = delete;
        Object& operator=(const Object&) = delete;
        // 启用移动构造（提高性能）
        Object(Object&& other) noexcept;
        ~Object();
    };

    class DirIterator {
    private:
        DIR dir;
        FILINFO fno;
        char utf8_name[_MAX_LFN * 4 + 1];
        bool is_end;
        bool convert_to_utf8;
        void next();
    public:
        DirIterator();
        DirIterator(const char* path, bool convert_utf8 = true);
        ~DirIterator();
        DirIterator& operator++();
        DirIterator operator++(int);
        DirIterator& operator+=(uint32_t n);
        DirIterator operator+(uint32_t n) const;
        Object operator*() const;
        bool operator!=(const DirIterator& other) const;
    };

    class DirectoryRange {
    public:
        const char* path;
        bool convert_to_utf8;
        explicit DirectoryRange(const char* p, bool convert_utf8 = true);
        [[nodiscard]] DirIterator begin() const;
        static DirIterator end();
    };

    inline DirectoryRange listdir(const char* path, bool convert_to_utf8 = true) {
        return DirectoryRange(path, convert_to_utf8);
    }
}

#endif //FS_H
