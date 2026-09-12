#include <sys/mman.h>

#include <cstring>
#include <unistd.h>

#include "memory_utils.h"

size_t Memory::getPageSize() {
    static size_t _page_size = 0;
    if (_page_size == 0) {
        _page_size = sysconf(_SC_PAGESIZE);
        if (_page_size <= 0) {
            _page_size = 4096;
        }
    }
    return _page_size;
}

int Memory::getPageProtection(void *addr) {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        return -1;
    }

    uintptr_t target = (uintptr_t) addr;
    char line[512];
    int prot = 0;
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        uintptr_t start, end;
        char perms[5];

        if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) >= 2) {
            if (target >= start && target < end) {
                if (perms[0] == 'r') prot |= PROT_READ;
                if (perms[1] == 'w') prot |= PROT_WRITE;
                if (perms[2] == 'x') prot |= PROT_EXEC;
                found = 1;
                break;
            }
        }
    }

    fclose(fp);
    return found ? prot : -1;
}

RangeInfo Memory::getRangeBase(void *addr) {
    RangeInfo info = {nullptr, nullptr, 0, ""};
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        return info;
    }

    uintptr_t target = (uintptr_t) addr;
    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        uintptr_t start, end;
        char perms[5];
        long long offset, inode;
        char dev[32], pathname[256] = "";

        int fields = sscanf(line, "%lx-%lx %4s %llx %31s %lld %255s",
                            &start, &end, perms, &offset, dev, &inode, pathname);

        if (fields >= 3 && target >= start && target < end) {
            int prot = 0;
            if (perms[0] == 'r') prot |= PROT_READ;
            if (perms[1] == 'w') prot |= PROT_WRITE;
            if (perms[2] == 'x') prot |= PROT_EXEC;

            info.start = (void *) start;
            info.end = (void *) end;
            info.protect = prot;

            if (fields >= 7) {
                strncpy(info.filename, pathname, sizeof(info.filename) - 1);
                info.filename[sizeof(info.filename) - 1] = '\0';
            }
            break;
        }
    }

    fclose(fp);
    return info;
}

void Memory::traverse_maps(traverse_maps_callback callback) {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        return;
    }
    char line[512];

    while (fgets(line, sizeof(line), fp)) {
        uintptr_t start, end;
        char perms[5];
        long long offset, inode;
        char dev[32], pathname[256] = "";

        int fields = sscanf(line, "%lx-%lx %4s %llx %31s %lld %255s",
                            &start, &end, perms, &offset, dev, &inode, pathname);
        int prot = 0;
        if (perms[0] == 'r') prot |= PROT_READ;
        if (perms[1] == 'w') prot |= PROT_WRITE;
        if (perms[2] == 'x') prot |= PROT_EXEC;
        RangeInfo info = {nullptr, nullptr, 0, ""};
        info.start = (void *) start;
        info.end = (void *) end;
        info.protect = prot;

        if (fields >= 7) {
            strncpy(info.filename, pathname, sizeof(info.filename) - 1);
            info.filename[sizeof(info.filename) - 1] = '\0';
        }

        callback(&info);
    }
}

void Memory::write32(void *addr, uint32_t value) {
    int flags = getPageProtection(addr);
    if (flags == -1) {
        return;
    }

    size_t page_size = getPageSize();
    void *page_start = (void *) ((uintptr_t) addr & ~(page_size - 1));

    mprotect(page_start, page_size, flags | PROT_WRITE);
    *((uint32_t *) addr) = value;
    mprotect(page_start, page_size, flags);
}

void Memory::writeAddr(void *addr, void *value) {
    int flags = getPageProtection(addr);
    if (flags == -1) {
        return;
    }

    size_t page_size = getPageSize();
    void *page_start = (void *) ((uintptr_t) addr & ~(page_size - 1));

    mprotect(page_start, page_size, flags | PROT_WRITE);
    *((void **) addr) = value;
    mprotect(page_start, page_size, flags);
}

void Memory::writeString(void *addr, char *string) {
    int flags = getPageProtection(addr);
    if (flags == -1) {
        return;
    }

    size_t num = strlen(string);
    size_t page_size = getPageSize();

    void *page_start = (void *) ((uintptr_t) addr & ~(page_size - 1));
    void *end_addr = (char *) addr + num;
    void *page_end = (void *) (((uintptr_t) end_addr + page_size - 1) & ~(page_size - 1));

    size_t protect_size = (char *) page_end - (char *) page_start;

    mprotect(page_start, protect_size, flags | PROT_WRITE);
    memcpy(addr, string, num);
    mprotect(page_start, protect_size, flags);
}

void Memory::writeBytes(void *addr, const uint8_t *bytes, size_t bytes_length) {
    if (!addr || !bytes || bytes_length == 0) {
        return;
    }

    int flags = getPageProtection(addr);
    if (flags == -1) {
        return;
    }

    size_t page_size = getPageSize();

    void *page_start = (void *) ((uintptr_t) addr & ~(page_size - 1));
    void *end_addr = (char *) addr + bytes_length;
    void *page_end = (void *) (((uintptr_t) end_addr + page_size - 1) & ~(page_size - 1));

    size_t protect_size = (char *) page_end - (char *) page_start;

    mprotect(page_start, protect_size, PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy(addr, bytes, bytes_length);
    mprotect(page_start, protect_size, flags);
}