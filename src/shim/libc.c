#include "shim/libc.h"
#include "stddef.h"

static EFI_SYSTEM_TABLE* priv_st = NULL;

void setup_libc(EFI_SYSTEM_TABLE* st) {
    priv_st = st;
}

void* malloc(size_t size) {
    void* p = NULL;
    if (EFI_ERROR(
            priv_st->BootServices->AllocatePool(EfiBootServicesData, size, &p)
        )) {
        return NULL;
    }
    return p;
}

void free(void* ptr) {
    if (ptr != NULL) {
        priv_st->BootServices->FreePool(ptr);
    }
}

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*) dest;
    const unsigned char* s = (const unsigned char*) src;

    size_t blocks = n >> 3;
    while (blocks--) {
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        d[3] = s[3];
        d[4] = s[4];
        d[5] = s[5];
        d[6] = s[6];
        d[7] = s[7];
        d += 8;
        s += 8;
    }

    size_t remainder = n & 7;
    while (remainder--) {
        *d++ = *s++;
    }

    return dest;
}

void* memset(void* dest, int value, size_t n) {
    unsigned char* d = (unsigned char*) dest;
    unsigned char v = (unsigned char) value;

    size_t blocks = n >> 3;
    while (blocks--) {
        d[0] = v;
        d[1] = v;
        d[2] = v;
        d[3] = v;
        d[4] = v;
        d[5] = v;
        d[6] = v;
        d[7] = v;
        d += 8;
    }

    size_t remainder = n & 7;
    while (remainder--) {
        *d++ = v;
    }

    return dest;
}

size_t strlen(const char* s) {
    const char* p = s;

    while (*p != '\0') {
        p++;
    }

    return (size_t) (p - s);
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n > 0) {
        if (*s1 != *s2 || *s1 == '\0') {
            return *(unsigned char*) s1 - *(unsigned char*) s2;
        }
        s1++;
        s2++;
        n--;
    }
    return 0;
}
