#ifndef BOOT_MSTATUS_H
#define BOOT_MSTATUS_H

#include <efi.h>
#include <stdbool.h>

bool secure_boot_active(EFI_SYSTEM_TABLE* st);
bool wifi_present(EFI_SYSTEM_TABLE* st);

#endif // BOOT_MSTATUS_H
