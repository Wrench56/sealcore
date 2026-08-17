#include "boot/mstatus.h"

#define EFI_WIRELESS_MAC_CONNECTION_PROTOCOL_GUID                              \
    { 0xda55bc9,                                                               \
      0x45f8,                                                                  \
      0x4bb4,                                                                  \
      { 0x87, 0x19, 0x52, 0x24, 0xf1, 0x8a, 0x4d, 0x45 } }

#define EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL_GUID                           \
    { 0x1b0fb9bf,                                                              \
      0x699d,                                                                  \
      0x4fdd,                                                                  \
      { 0xa7, 0xc3, 0x25, 0x46, 0x68, 0x1b, 0xf6, 0x3b } }

static const EFI_GUID global_var_guid = EFI_GLOBAL_VARIABLE;

static EFI_GUID wmp1 = EFI_WIRELESS_MAC_CONNECTION_PROTOCOL_GUID;
static EFI_GUID wmp2 = EFI_WIRELESS_MAC_CONNECTION_II_PROTOCOL_GUID;

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

static bool protocol_present(EFI_BOOT_SERVICES* bs, EFI_GUID* guid) {
    EFI_HANDLE* handles = NULL;
    size_t count = 0;
    if (EFI_ERROR(
            bs->LocateHandleBuffer(ByProtocol, guid, NULL, &count, &handles)
        )) {
        return false;
    }
    if (handles) {
        bs->FreePool(handles);
    }
    return count > 0;
}

// TODO: This loads a bit too much bloat perhaps...
static void connect_all(EFI_BOOT_SERVICES* bs) {
    EFI_HANDLE* handles = NULL;
    UINTN count = 0;
    if (EFI_ERROR(
            bs->LocateHandleBuffer(AllHandles, NULL, NULL, &count, &handles)
        )) {
        return;
    }
    for (UINTN i = 0; i < count; i++) {
        bs->ConnectController(handles[i], NULL, NULL, true);
    }
    bs->FreePool(handles);
}

bool wifi_present(EFI_SYSTEM_TABLE* st) {
    connect_all(st->BootServices);
    return protocol_present(st->BootServices, &wmp2) ||
        protocol_present(st->BootServices, &wmp1);
}
