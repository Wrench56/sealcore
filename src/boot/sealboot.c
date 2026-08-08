#include <efi.h>
#include <stdint.h>

#include "gfx/img.h"

static const uint8_t imgdata[] = {
    #include "boot/logo.hex"
};

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

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    (void) ImageHandle;

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Hello World!\r\n");
    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        L"Press Enter to continue...\r\n"
    );

    setup_gfx(SystemTable);
    image_t image;
    image.width = 164;
    image.height = 164;
    image.data = (pixel_data_t*) &imgdata;
    draw_img(&image, 0, 0);

    wait_for_enter(SystemTable);
    return EFI_SUCCESS;
}
