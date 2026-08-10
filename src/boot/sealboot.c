#include <efi.h>
#include <stdint.h>

#include "boot/boot.h"
#include "boot/loader.h"
#include "boot/mstatus.h"
#include "gfx/img.h"
#include "shim/libc.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

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

static inline uint16_t get_pw(EFI_SYSTEM_TABLE* st, char* pw) {
    EFI_INPUT_KEY key;
    size_t index;
    size_t curr = 0;

    st->ConOut->OutputString(st->ConOut, L" > ");
    for (;;) {
        st->BootServices->WaitForEvent(1, &st->ConIn->WaitForKey, &index);
        st->ConIn->ReadKeyStroke(st->ConIn, &key);
        if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            break;
        } else {
            if (key.UnicodeChar < 0x80) {
                pw[curr++] = (char) key.UnicodeChar;
                st->ConOut->OutputString(st->ConOut, L"*");

                if (curr >= PW_MAX_LEN) {
                    st->ConOut->OutputString(
                        st->ConOut,
                        L"\r\nError: Your password is longer than " TOSTRING(
                            PW_MAX_LEN
                        ) " characters!\r\n"
                    );
                    st->ConOut->SetCursorPosition(
                        st->ConOut,
                        st->ConOut->Mode->CursorColumn - 1,
                        3
                    );
                    memset(pw, 0, PW_MAX_LEN);
                }
            } else {
                st->ConOut->OutputString(
                    st->ConOut,
                    L"\r\nError: A password cannot contain a non-ASCII "
                    L"character!\r\n"
                );
                st->ConOut->SetCursorPosition(
                    st->ConOut,
                    st->ConOut->Mode->CursorColumn - 1,
                    3
                );
                memset(pw, 0, PW_MAX_LEN);
            }
        }
    }

    pw[curr] = '\0';
    st->ConOut->OutputString(st->ConOut, L"\r\n\r\n");

    return curr;
}

static void print_status(
    EFI_SYSTEM_TABLE* st,
    const wchar_t* name,
    bool status,
    const wchar_t* extra
) {
    SIMPLE_TEXT_OUTPUT_INTERFACE* out = st->ConOut;

    out->OutputString(out, L"[ ");
    if (status) {
        out->SetAttribute(out, EFI_GREEN | EFI_BACKGROUND_BLACK);
        out->OutputString(out, L" UP  ");
    } else {
        out->SetAttribute(out, EFI_RED | EFI_BACKGROUND_BLACK);
        out->OutputString(out, L"DOWN ");
    }
    out->SetAttribute(out, EFI_WHITE | EFI_BACKGROUND_BLACK);
    out->OutputString(out, L"] ");
    if (name != NULL) {
        out->SetAttribute(out, EFI_CYAN | EFI_BACKGROUND_BLACK);
        out->OutputString(out, (wchar_t*) name);
        out->SetAttribute(out, EFI_WHITE | EFI_BACKGROUND_BLACK);
        out->OutputString(out, L": ");
    }

    if (extra != NULL) {
        out->OutputString(out, (wchar_t*) extra);
    }

    out->OutputString(out, L"\r\n");
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
        L"\r\n=========================================\r\n\r\n"
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

    bool sb = secure_boot_active(SystemTable);
    print_status(SystemTable, L"secure_boot", sb, L"Secure boot status");
    bool wifi = wifi_present(SystemTable);
    print_status(SystemTable, L"wifi", wifi, L"Driver existence status");

    out->OutputString(
        out,
        L"\r\n=========================================\r\n\r\n"
    );

    char pw[PW_MAX_LEN] = { 0 };
    uint16_t pw_sz = get_pw(SystemTable, pw);

    EFI_STATUS status;
    sealcore_entry_fn_t fn = load_sealcore(SystemTable, &status, pw, pw_sz);
    if (EFI_ERROR(status)) {
        SystemTable->ConOut->OutputString(
            SystemTable->ConOut,
            L"Authentication error!\r\n"
        );
        wait_for_enter(SystemTable);

        return status;
    }

    fn(ImageHandle, SystemTable);

    free_sealcore(SystemTable, fn);
    return EFI_SUCCESS;
}
