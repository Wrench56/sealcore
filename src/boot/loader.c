#include <stddef.h>
#include <stdint.h>

#include "cryptography-primitives/include/ippcp.h"

#include "boot/loader.h"
#include "crypto/argon2/argon2.h"
#include "shim/libc.h"

#ifndef KDF_T_COST
#define KDF_T_COST 3
#endif
#ifndef KDF_M_COST
#define KDF_M_COST (1 << 20)
#endif
#ifndef KDF_SALT
#error "No KDF salt provided!"
#define KDF_SALT { 0 }
#endif

#define KDF_SALT_LEN 16

#ifndef GCM_TAG
#error "No AES256-GCM tag provided!"
#define GCM_TAG { 0 }
#endif

#define SEALCORE_BIN L"\\EFI\\SEALCORE\\sealcore.bin"

#define GCM_KEY_LEN 32
#define GCM_TAG_LEN 16
#define GCM_IV_LEN 12

#ifndef LOADER_GCM_IV
#define LOADER_GCM_IV NULL
#endif

#if defined(__clang__)
#define NOOPT __attribute__((optnone, noinline))
#elif defined(__GNUC__) || defined(__GNUG__)
#define NOOPT __attribute__((optimize("O0")))
#endif

static const EFI_GUID sfs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* sfs_prot = NULL;

static const uint8_t kdf_salt[KDF_SALT_LEN] = KDF_SALT;
static const uint8_t gcm_tag[GCM_TAG_LEN] = GCM_TAG;

// TODO: I don't know if I want to trust myself to do this...
// Just copy from some crypto wizard (e.g. Linux's crypto_memneq())
static NOOPT int32_t
constant_time_eq(const void* a, const void* b, size_t size) {
    const unsigned char* p1 = a;
    const unsigned char* p2 = b;
    volatile uint8_t result = 0;

    for (size_t i = 0; i < size; i++) {
        result |= p1[i] ^ p2[i];
        __asm__ volatile("" : "+r"(result) : : "memory");
    }

    return (result == 0);
}

static inline int32_t gcm_decrypt(
    const uint8_t* key,
    const uint8_t* tag,
    const uint8_t* ciphertext,
    uint32_t ciphertext_len,
    uint8_t* out
) {
    int32_t ret = 0;

    int32_t ctx_sz;
    IppStatus status = ippsAES_GCMGetSize(&ctx_sz);
    if (status != ippStsNoErr) {
        return -1;
    }

    IppsAES_GCMState* state = (IppsAES_GCMState*) malloc(ctx_sz);
    if (state == NULL) {
        ret = -1;
        goto cleanup;
    }

    status = ippsAES_GCMInit(key, GCM_KEY_LEN, state, ctx_sz);
    if (status != ippStsNoErr) {
        ret = -1;
        goto cleanup;
    }

    status = ippsAES_GCMStart(LOADER_GCM_IV, GCM_IV_LEN, NULL, 0, state);
    if (status != ippStsNoErr) {
        ret = -1;
        goto cleanup;
    }

    status = ippsAES_GCMDecrypt(ciphertext, out, ciphertext_len, state);
    if (status != ippStsNoErr) {
        ret = -1;
        goto cleanup;
    }

    uint8_t recovered_tag[GCM_TAG_LEN];
    status = ippsAES_GCMGetTag(recovered_tag, GCM_TAG_LEN, state);
    if (status != ippStsNoErr) {
        ret = -1;
        goto cleanup;
    }

    if (constant_time_eq(tag, recovered_tag, GCM_TAG_LEN) == 0) {
        memset(out, 0, ciphertext_len);
        ret = -2;
    }

cleanup:
    memset(state, 0, ctx_sz);
    free(state);

    return ret;
}

static inline EFI_STATUS read_file(
    EFI_SYSTEM_TABLE* st,
    const wchar_t* filename,
    uint32_t* filesize,
    void* heap,
    uint32_t heap_sz
) {
    *filesize = 0;
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

    *filesize = sc_sz;
    bootvol->Close(bootvol);
    file->Close(file);

    return EFI_SUCCESS;
}

sealcore_entry_fn_t load_sealcore(
    EFI_SYSTEM_TABLE* st,
    EFI_STATUS* status,
    char* key,
    int16_t key_len
) {
    setup_libc(st);
    st->ConOut->OutputString(st->ConOut, L"Generating Argon2id key hash...\r\n");

    uint8_t hash[GCM_KEY_LEN];
    int32_t rc = argon2id_hash_raw(
        KDF_T_COST,
        KDF_M_COST,
        1,
        key,
        key_len,
        kdf_salt,
        sizeof(kdf_salt),
        hash,
        sizeof(hash)
    );

    memset(key, 0, key_len);
    if (rc != ARGON2_OK) {
        *status = EFI_ACCESS_DENIED;
        return NULL;
    }

    st->ConOut->OutputString(
        st->ConOut,
        L"Allocating pages for SealCore...\r\n"
    );
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

    st->ConOut->OutputString(
        st->ConOut,
        L"Reading encrypted SealCore from disk...\r\n"
    );
    uint32_t filesize;
    *status = read_file(
        st,
        SEALCORE_BIN,
        &filesize,
        (void*) sc_phys,
        SEALCORE_SIZE
    );
    if (EFI_ERROR(*status)) {
        return NULL;
    }

    st->ConOut->OutputString(st->ConOut, L"Decrypting SealCore...\r\n");
    if (gcm_decrypt(
            hash,
            gcm_tag,
            (void*) sc_phys,
            filesize,
            (uint8_t*) sc_phys
        ) != 0) {
        *status = EFI_ACCESS_DENIED;
        return NULL;
    }

    return (sealcore_entry_fn_t) sc_phys;
}

void free_sealcore(EFI_SYSTEM_TABLE* st, sealcore_entry_fn_t fn) {
    memset(fn, 0, SEALCORE_SIZE);

    UINTN sc_pages = EFI_SIZE_TO_PAGES(SEALCORE_SIZE);
    st->BootServices->FreePages((EFI_PHYSICAL_ADDRESS) fn, sc_pages);
}
