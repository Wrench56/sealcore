#include <efi.h>

static void wait_for_enter(EFI_SYSTEM_TABLE* st) {
    EFI_INPUT_KEY key;
    UINTN index;

    for (;;) {
        st->BootServices->WaitForEvent(1, &st->ConIn->WaitForKey, &index);
        st->ConIn->ReadKeyStroke(st->ConIn, &key);
        if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            break;
        }
    }
}

EFI_STATUS EFIAPI main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    (void) ImageHandle;

    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        L"Hello World from sealcore!\r\n"
    );

    wait_for_enter(SystemTable);
    return EFI_SUCCESS;
}
