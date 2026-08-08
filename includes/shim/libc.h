#ifndef SEALCORE_SHIM_LIBC_H
#define SEALCORE_SHIM_LIBC_H

#include <efi.h>

void setup_libc(EFI_SYSTEM_TABLE* st);
void* malloc(size_t size);
void free(void* ptr);

void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* dest, int value, size_t n);

size_t strlen(const char* s);
int strncmp(const char* s1, const char* s2, size_t n);

#endif // SEALCORE_SHIM_LIBC_H
