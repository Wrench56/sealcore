#include <efi.h>
#include <stdint.h>

#include "boot/loader.h"
#include "gfx/img.h"

static const wchar_t
    banner[] = L"   _____            __                   \r\n"
               L"  / ___/___  ____ _/ /________  ________ \r\n"
               L"  \\__ \\/ _ \\/ __ `/ / ___/ __ \\/ ___/ _ \\\r\n"
               L" ___/ /  __/ /_/ / / /__/ /_/ / /  /  __/\r\n"
               L"/____/\\___/\\__,_/_/\\___/\\____/_/   \\___/\r\n";

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

    SIMPLE_TEXT_OUTPUT_INTERFACE* out = SystemTable->ConOut;

    out->ClearScreen(out);
    out->SetAttribute(out, EFI_CYAN | EFI_BACKGROUND_BLACK);
    out->OutputString(out, (wchar_t*) banner);
    out->SetAttribute(out, EFI_WHITE | EFI_BACKGROUND_BLACK);
    out->OutputString(
        out,
        L"\r\n=========================================\r\n"
    );

    setup_gfx(SystemTable);

    uint32_t width;
    uint32_t height;
    get_screen_resolution(&width, &height);

    image_t image;
    image.width = 164;
    image.height = 164;
    image.data = (pixel_data_t*) &imgdata;
    draw_img(&image, width - image.width - 10, 10);

    EFI_STATUS status;
    char key[] = "test1234";
    sealcore_entry_fn_t
        fn = load_sealcore(SystemTable, &status, key, sizeof(key));
    if (EFI_ERROR(status)) {
        return status;
    }

    fn(ImageHandle, SystemTable);
    free_sealcore(SystemTable, fn);

    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        L"Press Enter to continue...\r\n"
    );
    wait_for_enter(SystemTable);

    return EFI_SUCCESS;
}
