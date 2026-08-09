#include <efi.h>
#include <stdint.h>

#include "gfx/img.h"

#define SEALCORE_SIZE 4 * 1024 * 1024

typedef EFI_STATUS EFIAPI (*sealcore_entry_fn_t)(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE* SystemTable
);

static const EFI_GUID sfs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* sfs_prot = NULL;

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

    EFI_STATUS status;

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"Hello World!\r\n");

    setup_gfx(SystemTable);
    image_t image;
    image.width = 164;
    image.height = 164;
    image.data = (pixel_data_t*) &imgdata;
    draw_img(&image, 0, 0);

    status = SystemTable->BootServices
        ->LocateProtocol((EFI_GUID*) &sfs_guid, NULL, (VOID**) &sfs_prot);
    if (EFI_ERROR(status)) {
        return status;
    }

    EFI_FILE_PROTOCOL* bootvol;
    status = sfs_prot->OpenVolume(sfs_prot, &bootvol);
    if (EFI_ERROR(status)) {
        return status;
    }

    EFI_FILE_PROTOCOL* sealcore_file;
    status = bootvol->Open(
        bootvol,
        &sealcore_file,
        L"\\EFI\\SEALCORE\\sealcore.bin",
        EFI_FILE_READ_ONLY,
        0
    );
    if (EFI_ERROR(status)) {
        return status;
    }

    EFI_PHYSICAL_ADDRESS sc_phys = 0;
    UINTN sc_pages = EFI_SIZE_TO_PAGES(SEALCORE_SIZE);
    status = SystemTable->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiLoaderCode,
        sc_pages,
        &sc_phys
    );
    if (EFI_ERROR(status)) {
        return EFI_OUT_OF_RESOURCES;
    }

    uint64_t sc_sz = SEALCORE_SIZE;

    status = sealcore_file->Read(sealcore_file, &sc_sz, (void*) sc_phys);
    if (EFI_ERROR(status)) {
        return status;
    } else if (sc_sz == 0) {
        return EFI_NOT_FOUND;
    }

    sealcore_entry_fn_t fn = (sealcore_entry_fn_t) sc_phys;
    fn(ImageHandle, SystemTable);

    SystemTable->ConOut->OutputString(
        SystemTable->ConOut,
        L"Press Enter to continue...\r\n"
    );
    wait_for_enter(SystemTable);

    SystemTable->BootServices->FreePages(sc_phys, sc_pages);
    return EFI_SUCCESS;
}
