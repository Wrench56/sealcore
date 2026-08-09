#include <stddef.h>
#include <stdint.h>

#include "boot/loader.h"
#include "shim/libc.h"

static inline EFI_STATUS read_file(
    EFI_SYSTEM_TABLE* st,
    const wchar_t* filename,
    void* heap,
    uint32_t heap_sz
) {
    EFI_STATUS status = st->BootServices->LocateProtocol(
        (EFI_GUID*) &sfs_guid,
        NULL,
        (VOID**) &sfs_prot
    );
    if (EFI_ERROR(status)) {
        return status;
    }

    EFI_FILE_PROTOCOL* bootvol;
    status = sfs_prot->OpenVolume(sfs_prot, &bootvol);
    if (EFI_ERROR(status)) {
        return status;
    }

    EFI_FILE_PROTOCOL* file;
    status = bootvol->Open(
        bootvol,
        &file,
        (wchar_t*) filename,
        EFI_FILE_READ_ONLY,
        0
    );
    if (EFI_ERROR(status)) {
        bootvol->Close(bootvol);
        return status;
    }

    uint64_t sc_sz = heap_sz;
    status = file->Read(file, &sc_sz, heap);
    if (EFI_ERROR(status)) {
        bootvol->Close(bootvol);
        file->Close(file);
        return status;
    } else if (sc_sz == 0) {
        bootvol->Close(bootvol);
        file->Close(file);
        return EFI_NOT_FOUND;
    }

    bootvol->Close(bootvol);
    file->Close(file);

    return EFI_SUCCESS;
}

sealcore_entry_fn_t load_sealcore(EFI_SYSTEM_TABLE* st, EFI_STATUS* status) {

    EFI_PHYSICAL_ADDRESS sc_phys = 0;
    UINTN sc_pages = EFI_SIZE_TO_PAGES(SEALCORE_SIZE);
    *status = st->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiLoaderCode,
        sc_pages,
        &sc_phys
    );
    if (EFI_ERROR(*status)) {
        return NULL;
    }

    *status = read_file(
        st,
        L"\\EFI\\SEALCORE\\sealcore.bin",
        (void*) sc_phys,
        SEALCORE_SIZE
    );
    if (EFI_ERROR(*status)) {
        return NULL;
    }

    return (sealcore_entry_fn_t) sc_phys;
}

void free_sealcore(EFI_SYSTEM_TABLE* st, sealcore_entry_fn_t fn) {
    memset(fn, 0, SEALCORE_SIZE);

    UINTN sc_pages = EFI_SIZE_TO_PAGES(SEALCORE_SIZE);
    st->BootServices->FreePages((EFI_PHYSICAL_ADDRESS) fn, sc_pages);
}
