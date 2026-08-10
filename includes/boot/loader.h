#ifndef BOOT_LOADER_H
#define BOOT_LOADER_H

#include <efi.h>
#include <stdint.h>

#define SEALCORE_SIZE 64 * 1024

typedef EFI_STATUS EFIAPI (*sealcore_entry_fn_t)(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE* SystemTable
);

sealcore_entry_fn_t load_sealcore(
    EFI_SYSTEM_TABLE* st,
    EFI_STATUS* status,
    char* key,
    int16_t key_len
);
void free_sealcore(EFI_SYSTEM_TABLE* st, sealcore_entry_fn_t fn);

#endif // BOOT_LOADER_H
