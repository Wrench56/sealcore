#include "boot/mstatus.h"

#define EFI_WIRELESS_MAC_CONNECTION_PROTOCOL_GUID                              \
    { 0xda55bc9,                                                               \
      0x45f8,                                                                  \
      0x4bb4,                                                                  \
      { 0x87, 0x19, 0x52, 0x24, 0xf1, 0x8a, 0x4d, 0x45 } }

static const EFI_GUID global_var_guid = EFI_GLOBAL_VARIABLE;
static const EFI_GUID wmp = EFI_WIRELESS_MAC_CONNECTION_PROTOCOL_GUID;

static EFI_STATUS read_u8_var(
    EFI_SYSTEM_TABLE* st,
    wchar_t* name,
    EFI_GUID* guid,
    uint8_t* out
) {
    size_t size = sizeof(UINT8);
    uint32_t attr;
    return st->RuntimeServices->GetVariable(name, guid, &attr, &size, out);
}

bool secure_boot_active(EFI_SYSTEM_TABLE* st) {
    uint8_t sb = 0;
    uint8_t sm = 0;

    if (read_u8_var(st, L"SecureBoot", (EFI_GUID*) &global_var_guid, &sb) !=
        EFI_SUCCESS) {
        return 0;
    }

    read_u8_var(st, L"SetupMode", (EFI_GUID*) &global_var_guid, &sm);
    return sb == 1 && sm == 0;
}

bool wifi_present(EFI_SYSTEM_TABLE* st) {
    EFI_HANDLE* h = NULL;
    size_t n = 0;

    if (st->BootServices
            ->LocateHandleBuffer(ByProtocol, (EFI_GUID*) &wmp, NULL, &n, &h) !=
        EFI_SUCCESS) {
        return false;
    }

    st->BootServices->FreePool(h);
    return n > 0;
}
