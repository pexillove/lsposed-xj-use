#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <cstdint>
#include <cstdio>

struct RangeInfo {
    void *start;
    void *end;
    int protect;
    char filename[256];
};

typedef bool traverse_maps_callback(RangeInfo *);

class Memory {
public:
    static size_t getPageSize();

    static int getPageProtection(void *addr);

    static RangeInfo getRangeBase(void *addr);

    static void traverse_maps(traverse_maps_callback);

    inline static uint32_t roundUpToPtrSize(uint32_t x);

    inline static uint32_t read32(void *addr);

    inline static void *readAddr(void *addr);

    inline static char *readString(void *addr);

    static void write32(void *addr, uint32_t value);

    static void writeAddr(void *addr, void *value);

    static void writeString(void *addr, char *string);

    static void writeBytes(void *addr, const uint8_t *bytes, size_t bytes_length);
};

inline uint32_t Memory::roundUpToPtrSize(uint32_t x) {
    return (x + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
}

inline uint32_t Memory::read32(void *addr) {
    return *((uint32_t *) addr);
}

inline void *Memory::readAddr(void *addr) {
    return *((void **) addr);
}

inline char *Memory::readString(void *addr) {
    return (char *) addr;
}

#endif
