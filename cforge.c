#include "cforge.h"

#if CF_VERSION_BELOW(1, 1, 0)
#error "CForge too old!"
#endif

#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Winfinite-recursion"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Winfinite-recursion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#endif

double pow(double base, double exp) {
    return __builtin_pow(base, exp);
}

double ldexp(double x, int exp) {
    return __builtin_scalbn(x, exp);
}

#define _MATH_H 1
#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb/stb_image.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

/* This sucks and differs per distro... Torvalds, why allow distros, why?! */
#define OVMF_CODE "/usr/share/edk2/x64/OVMF_CODE.secboot.4m.fd"
#define OVMF_VARS_TEMPLATE "/usr/share/edk2/x64/OVMF_VARS.4m.fd"
#define SHIM "/usr/share/shim-signed/shimx64.efi"
#define MM "/usr/share/shim-signed/mmx64.efi"

#define BUILD_DIR "build"
#define OBJ_DIR BUILD_DIR "/obj"
#define ASSETS_DIR "assets"
#define IPP_LIB_DIR "libs/cryptography-primitives"

#define IPP_LIB_STATIC IPP_LIB_DIR "/_install/lib/libippcp_s_y8.a"

#define KEY_FILE BUILD_DIR "/MOK.key"
#define CRT_FILE BUILD_DIR "/MOK.crt"
#define DER_FILE BUILD_DIR "/MOK.der"
#define MOK_SUBJECT "/CN=SealCore MOK/"
#define MOK_GUID "77fa9abd-0359-4d32-bd60-28f4e78f784b"

#define SBAT_CSV BUILD_DIR "/sbat.csv"
#define SBAT_OBJ BUILD_DIR "/sbat.o"

#define SEALBOOT_EFI BUILD_DIR "/sealboot.efi"
#define SEALCORE_BIN BUILD_DIR "/sealcore.bin"
#define SEALCORE_LD "src/core/sealcore.ld"
#define SEALCORE_ENTRY_OFFSET 0x0

#define SEALCORE_KEY_FILE "sealcore.key"
#define SC_KDF_T_COST 3
#define SC_KDF_M_COST (1 << 20)
#define SC_KDF_SALT_LEN 16
#define SC_GCM_KEY_LEN 32
#define SC_GCM_TAG_LEN 16
#define SC_GCM_IV_LEN 12

#define GRUB_EFI BUILD_DIR "/grubx64.efi"

#define ESP_IMG BUILD_DIR "/esp.img"
#define ESP_SIZE_MB 64

#define VARS_FD BUILD_DIR "/my_VARS.fd"
#define VARS_CLEAN_FD BUILD_DIR "/my_VARS.clean.fd"

#define ESP_BOOT_DIR "::/EFI/BOOT"
#define ESP_SEALCORE_DIR "::/EFI/SEALCORE"
#define ESP_BOOTX64 ESP_BOOT_DIR "/BOOTX64.EFI"
#define ESP_MM64 ESP_BOOT_DIR "/mmx64.efi"
#define ESP_GRUB64 ESP_BOOT_DIR "/grubx64.efi"
#define ESP_SEALCORE64 ESP_SEALCORE_DIR "/sealcore.bin"
#define ESP_MOK_DER "::/MOK.der"

#define LOGO_PNG ASSETS_DIR "/sealcore_164x164.png"
#define LOGO_BIN "includes/boot/logo.hex"

#define KY_TAG "[  " CF_GREEN "KY" CF_RESET "  ] "
#define LG_TAG "[  " CF_BLUE "LG" CF_RESET "  ] "
#define CC_TAG "[  " CF_YELLOW "CC" CF_RESET "  ] "
#define SB_TAG "[  " CF_YELLOW "SB" CF_RESET "  ] "
#define EN_TAG "[  " CF_MAGENTA "EN" CF_RESET "  ] "
#define LD_TAG "[  " CF_CYAN "LD" CF_RESET "  ] "
#define SN_TAG "[  " CF_CYAN "SN" CF_RESET "  ] "
#define FS_TAG "[  " CF_GREEN "FS" CF_RESET "  ] "
#define VR_TAG "[  " CF_MAGENTA "VR" CF_RESET "  ] "
#define QM_TAG "[  " CF_RED "QM" CF_RESET "  ] "
#define OK_TAG "[  " CF_GREEN "OK" CF_RESET "  ] "
#define FA_TAG "[ " CF_RED "FAIL" CF_RESET " ] "

typedef union __attribute__((packed)) {
    struct __attribute__((packed)) {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };
    uint32_t raw;
} pixel_data_t;

/* TODO: Generate random salt per compilation */
static const uint8_t sc_kdf_salt[SC_KDF_SALT_LEN] = { 0 };
static uint8_t sc_gcm_iv[SC_GCM_IV_LEN];
static uint8_t sc_gcm_tag[SC_GCM_TAG_LEN];

#define SC_OSSL_FUNCS(X)                                                       \
    X(sc_EVP_KDF_fetch, EVP_KDF*, (OSSL_LIB_CTX*, const char*, const char*) )  \
    X(sc_EVP_KDF_CTX_new, EVP_KDF_CTX*, (EVP_KDF*) )                           \
    X(sc_EVP_KDF_free, void, (EVP_KDF*) )                                      \
    X(sc_EVP_KDF_CTX_free, void, (EVP_KDF_CTX*) )                              \
    X(sc_EVP_KDF_derive,                                                       \
      int,                                                                     \
      (EVP_KDF_CTX*, unsigned char*, size_t, const OSSL_PARAM*) )              \
    X(sc_OSSL_PARAM_construct_octet_string,                                    \
      OSSL_PARAM,                                                              \
      (const char*, void*, size_t) )                                           \
    X(sc_OSSL_PARAM_construct_uint32, OSSL_PARAM, (const char*, uint32_t*) )   \
    X(sc_OSSL_PARAM_construct_end, OSSL_PARAM, (void) )                        \
    X(sc_EVP_CIPHER_CTX_new, EVP_CIPHER_CTX*, (void) )                         \
    X(sc_EVP_CIPHER_CTX_free, void, (EVP_CIPHER_CTX*) )                        \
    X(sc_EVP_CIPHER_CTX_ctrl, int, (EVP_CIPHER_CTX*, int, int, void*) )        \
    X(sc_EVP_EncryptInit_ex,                                                   \
      int,                                                                     \
      (EVP_CIPHER_CTX*,                                                        \
       const EVP_CIPHER*,                                                      \
       ENGINE*,                                                                \
       const unsigned char*,                                                   \
       const unsigned char*) )                                                 \
    X(sc_EVP_EncryptUpdate,                                                    \
      int,                                                                     \
      (EVP_CIPHER_CTX*, unsigned char*, int*, const unsigned char*, int) )     \
    X(sc_EVP_EncryptFinal_ex, int, (EVP_CIPHER_CTX*, unsigned char*, int*) )   \
    X(sc_EVP_aes_256_gcm, const EVP_CIPHER*, (void) )

#define SC_DECL(fn, ret, args) static ret(*fn) args;
SC_OSSL_FUNCS(SC_DECL)

static void sc_load_libcrypto(void) {
    void* h = dlopen("libcrypto.so.3", RTLD_NOW | RTLD_GLOBAL);
    if (h == NULL) {
        h = dlopen("libcrypto.so", RTLD_NOW | RTLD_GLOBAL);
    }
    if (h == NULL) {
        fprintf(
            stderr,
            FA_TAG "Error: dlopen(libcrypto) failed: %s\n",
            dlerror()
        );
        exit(1);
    }

#define SC_LOAD(fn, ret, args)                                                 \
    *(void**) &fn = dlsym(h, #fn + 3);                                         \
    if (fn == NULL) {                                                          \
        fprintf(stderr, FA_TAG "Error: libcrypto missing %s\n", #fn + 3);      \
        exit(1);                                                               \
    }
    SC_OSSL_FUNCS(SC_LOAD)
#undef SC_LOAD
}

static char* sc_hex_init(const uint8_t* d, size_t n) {
    char* out = (char*) malloc(n * 5 + 1);
    char* p = out;
    for (size_t i = 0; i < n; i++) {
        p += sprintf(p, i ? ",0x%02x" : "0x%02x", d[i]);
    }
    return out;
}

static void encrypt_sealcore(void) {
    sc_load_libcrypto();

    if (getrandom(sc_gcm_iv, sizeof(sc_gcm_iv), 0) != sizeof(sc_gcm_iv)) {
        fprintf(stderr, FA_TAG "Error: Could not generate AES256-GCM IV\n");
        exit(1);
    }

    char* pass = CF_READ(SEALCORE_KEY_FILE);
    if (pass == NULL) {
        fprintf(stderr, FA_TAG "Error: Could not read " SEALCORE_KEY_FILE "\n");
        exit(1);
    }
    size_t pass_len = strlen(pass);
    while (pass_len &&
           (pass[pass_len - 1] == '\n' || pass[pass_len - 1] == '\r')) {
        pass[--pass_len] = '\0';
    }

    uint8_t key[SC_GCM_KEY_LEN];
    EVP_KDF* kdf = sc_EVP_KDF_fetch(NULL, "ARGON2ID", NULL);
    EVP_KDF_CTX* kctx = kdf ? sc_EVP_KDF_CTX_new(kdf) : NULL;
    sc_EVP_KDF_free(kdf);
    uint32_t one = 1, t_cost = SC_KDF_T_COST, m_cost = SC_KDF_M_COST;
    OSSL_PARAM params[] = {
        sc_OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_PASSWORD,
            pass,
            pass_len + 1
        ),
        sc_OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_SALT,
            (void*) sc_kdf_salt,
            sizeof(sc_kdf_salt)
        ),
        sc_OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &t_cost),
        sc_OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &one),
        sc_OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_THREADS, &one),
        sc_OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &m_cost),
        sc_OSSL_PARAM_construct_end()
    };
    if (kctx == NULL ||
        sc_EVP_KDF_derive(kctx, key, sizeof(key), params) != 1) {
        fprintf(stderr, FA_TAG "Error: Argon2id key derivation failed\n");
        exit(1);
    }
    sc_EVP_KDF_CTX_free(kctx);

    FILE* fp = fopen(SEALCORE_BIN, "rb");
    if (fp == NULL) {
        fprintf(stderr, FA_TAG "Error: Could not open " SEALCORE_BIN "\n");
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);
    uint8_t* buf = (uint8_t*) malloc((size_t) sz);
    if (buf == NULL || fread(buf, 1, (size_t) sz, fp) != (size_t) sz) {
        fprintf(stderr, FA_TAG "Error: Could not read " SEALCORE_BIN "\n");
        exit(1);
    }
    fclose(fp);

    EVP_CIPHER_CTX* ctx = sc_EVP_CIPHER_CTX_new();
    int len, ct_len;
    if (sc_EVP_EncryptInit_ex(ctx, sc_EVP_aes_256_gcm(), NULL, NULL, NULL) !=
            1 ||
        sc_EVP_CIPHER_CTX_ctrl(
            ctx,
            EVP_CTRL_AEAD_SET_IVLEN,
            SC_GCM_IV_LEN,
            NULL
        ) != 1 ||
        sc_EVP_EncryptInit_ex(ctx, NULL, NULL, key, sc_gcm_iv) != 1 ||
        sc_EVP_EncryptUpdate(ctx, buf, &len, buf, (int) sz) != 1) {
        fprintf(stderr, FA_TAG "Error: AES-256-GCM encryption failed\n");
        exit(1);
    }
    ct_len = len;
    sc_EVP_EncryptFinal_ex(ctx, buf + len, &len);
    ct_len += len;
    sc_EVP_CIPHER_CTX_ctrl(
        ctx,
        EVP_CTRL_AEAD_GET_TAG,
        SC_GCM_TAG_LEN,
        sc_gcm_tag
    );
    sc_EVP_CIPHER_CTX_free(ctx);

    fp = fopen(SEALCORE_BIN, "wb");
    if (fp == NULL || fwrite(buf, 1, (size_t) ct_len, fp) != (size_t) ct_len) {
        fprintf(stderr, FA_TAG "Error: Could not write " SEALCORE_BIN "\n");
        exit(1);
    }
    fclose(fp);
    free(buf);

    printf(EN_TAG "  %s encrypted (AES-256-GCM)\n", SEALCORE_BIN);
}

CF_CONFIG(pe) {
    CF_SET_ENV(CC_TARGET_FLAGS, "-target x86_64-unknown-windows");
}

CF_CONFIG(pie) {
    /* 
       Dumb efibind.h turns EFIAPI into __attribute__((ms_abi))
       only when HAVE_USE_MS_ABI is set. This sucks.
       HAVE_USE_MS_ABI is derived from GNU_EFI_USE_MS_ABI.
    */
    CF_SET_ENV(
        CC_TARGET_FLAGS,
        "-target x86_64-unknown-none -DGNU_EFI_USE_MS_ABI -fshort-wchar "
        "-fpic -fpie"
    );
}

static void compile_sources(
    const char* pattern,
    const char* out_dir,
    const char* extra
) {
    for CF_GLOBS_EACH(pattern, file) {
        char* output = CF_MAP(
            file,
            CF_MAP_EXT("o"),
            CF_MAP_DIRS((char*) out_dir)
        );
        printf(CC_TAG "  %s -> %s\n", file, output);
        CF_RUNP(
            "clang %s -ffreestanding "
            "-fno-stack-protector -fno-zero-initialized-in-bss "
            "-mno-red-zone -maes -Wall -Wextra -Iincludes/ -Ilibs/ "
            "-I/usr/include/efi -I/usr/include/efi/x86_64 %s -c %s -o %s",
            CF_ENV(CC_TARGET_FLAGS),
            extra,
            file,
            output
        );
    }
}

CF_TARGET(keys, CF_HIDDEN) {
    CF_MKDIR(BUILD_DIR);
    CF_BANNER(KY_TAG "Generating keys...");
    if (CF_FILE_EXISTS(KEY_FILE)) {
        printf(KY_TAG "  %s exists\n", KEY_FILE);
        return;
    }

    printf(KY_TAG "  Creating %s and %s\n", KEY_FILE, CRT_FILE);
    CF_RUN(
        "out=$(openssl req -new -x509 -newkey rsa:2048 -subj '" MOK_SUBJECT "' "
        "-keyout " KEY_FILE " -out " CRT_FILE
        " -days 3650 -nodes 2>&1) || { echo \"$out\"; exit 1; }"
    );
    printf(KY_TAG "  Enrolling %s\n", DER_FILE);
    CF_RUN(
        "out=$(openssl x509 -in " CRT_FILE " -outform DER -out " DER_FILE
        " 2>&1) || { echo \"$out\"; exit 1; }"
    );
    printf(KY_TAG "  Keys ready\n");
}

CF_TARGET(logo, CF_HIDDEN) {
    CF_MKDIR(BUILD_DIR);
    CF_BANNER(LG_TAG "Generating logo...");

    if (CF_FILE_EXISTS(LOGO_BIN)) {
        printf(LG_TAG "  %s exists\n", LOGO_BIN);
        return;
    }

    int width;
    int height;
    int channels;
    int des_channels = 4;
    uint8_t*
        imgdata = stbi_load(LOGO_PNG, &width, &height, &channels, des_channels);
    if (!imgdata) {
        fprintf(stderr, FA_TAG "Error: Could not load logo\n");
        fflush(stderr);
        exit(1);
    }

    pixel_data_t* pxheap = (pixel_data_t*) malloc(
        (size_t) (height * width) * sizeof(pixel_data_t)
    );
    if (pxheap == NULL) {
        fprintf(stderr, FA_TAG "Error: malloc() failed!\n");
        fflush(stderr);
        stbi_image_free(imgdata);
        exit(1);
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint32_t pixel_index = (uint32_t) ((y * width + x) *
                                                     channels);

            pixel_data_t* pixel = &pxheap[y * width + x];
            pixel->r = imgdata[pixel_index];
            pixel->g = imgdata[pixel_index + 1];
            pixel->b = imgdata[pixel_index + 2];
            pixel->a = imgdata[pixel_index + 3];
        }
    }

    FILE* fp = fopen(LOGO_BIN, "w");

    const size_t total_pixels = (size_t) (height * width);
    for (size_t i = 0; i < total_pixels; ++i) {
        fprintf(
            fp,
            "0x%02X, 0x%02X, 0x%02X, 0x%02X,\n",
            pxheap[i].b,
            pxheap[i].g,
            pxheap[i].r,
            pxheap[i].a
        );
    }

    fclose(fp);

    free(pxheap);
    stbi_image_free(imgdata);

    printf(LG_TAG "  %s ready\n", LOGO_BIN);
}

CF_TARGET(
    compile_boot,
    CF_WITH_CONFIG(pe),
    CF_DEPENDS(logo),
    CF_DEPENDS(encrypt_core),
    CF_HIDDEN
) {
    CF_MKDIR(OBJ_DIR);
    CF_MKDIR(OBJ_DIR "/boot");
    CF_BANNER(CC_TAG "Compiling sealboot...");

    char* crypto_macros;
    asprintf(
        &crypto_macros,
        "-DLOADER_GCM_IV='(const uint8_t[]){%s}' -DGCM_TAG='{%s}' "
        "-DKDF_SALT='{%s}'",
        sc_hex_init(sc_gcm_iv, sizeof(sc_gcm_iv)),
        sc_hex_init(sc_gcm_tag, sizeof(sc_gcm_tag)),
        sc_hex_init(sc_kdf_salt, sizeof(sc_kdf_salt))
    );

    compile_sources("src/boot/*.c", OBJ_DIR "/boot/", crypto_macros);
    compile_sources("src/*.c", OBJ_DIR "/", "");
    compile_sources("src/gfx/*.c", OBJ_DIR "/", "");
    compile_sources("src/shim/*.c", OBJ_DIR "/", "");
    compile_sources("src/crypto/*.c", OBJ_DIR "/", "");
    compile_sources("src/crypto/**/*.c", OBJ_DIR "/", "");
}

CF_TARGET(compile_core, CF_WITH_CONFIG(pie), CF_HIDDEN) {
    CF_MKDIR(OBJ_DIR);
    CF_MKDIR(OBJ_DIR "/core");
    CF_BANNER(CC_TAG "Compiling sealcore...");

    compile_sources("src/core/*.c", OBJ_DIR "/core/", "");
}

CF_TARGET(compile_ipp, CF_HIDDEN) {
    CF_BANNER(CC_TAG "Compiling Intel Cryptography Primitives Library...");
    /*
       The platform Y8 is probably good for any current Intel machine.
       It only uses AES-NI and SSE. Probably would be fine to bump this
       to AVX2, which would also likely bring some performance. Meh.
    */
    CF_RUN(
        "cd %s; cmake CMakeLists.txt -B_build "
        "-DARCH=intel64 -DMERGED_BLD:BOOL=off -DPLATFORM_LIST=\"y8\" "
        "-DIPPCP_CUSTOM_BUILD=\"IPPCP_AES_ON;IPPCP_CLMUL_ON\" "
        "-DCMAKE_INSTALL_PREFIX=\"$PWD/_install\" ",
        IPP_LIB_DIR
    );
    CF_RUN(
        "cd %s; cmake --build _build --target ippcp_s_y8 ippcp_dyn_y8 -j",
        IPP_LIB_DIR
    );
}

CF_TARGET(
    link_core,
    CF_DEPENDS(compile_core),
    CF_DEPENDS(compile_ipp),
    CF_HIDDEN
) {
    char* core_objs = CF_JOIN_GLOB(CF_GLOB(OBJ_DIR "/core/*.o"), " ");

    CF_BANNER(LD_TAG "Linking sealcore...");
    CF_RUN(
        "ld.lld -T " SEALCORE_LD
        " --defsym=SEALCORE_ENTRY_OFFSET=%d -o " SEALCORE_BIN " %s",
        SEALCORE_ENTRY_OFFSET,
        core_objs
    );
    printf(LD_TAG "  %s\n", SEALCORE_BIN);
}

CF_TARGET(encrypt_core, CF_DEPENDS(link_core), CF_HIDDEN) {
    CF_BANNER(EN_TAG "Encrypting sealcore...");
    encrypt_sealcore();
}

CF_TARGET(link_boot, CF_DEPENDS(compile_boot), CF_HIDDEN) {
    char* shared_objs = CF_JOIN_GLOB(CF_GLOB(OBJ_DIR "/*.o"), " ");
    char* boot_objs = CF_JOIN_GLOB(CF_GLOB(OBJ_DIR "/boot/*.o"), " ");

    if (!CF_FILE_EXISTS(SBAT_CSV)) {
        printf(SB_TAG "  Creating %s\n", SBAT_CSV);
        CF_WRITE(
            SBAT_CSV,
            "sbat,1,SBAT "
            "Version,sbat,1,https://github.com/rhboot/shim/blob/main/SBAT.md\n"
            "sealcore,1,SealCore,sealcore,1,https://github.com/Wrench56/"
            "sealcore\n"
        );
    }

    printf(SB_TAG "%s -> %s\n", SBAT_CSV, SBAT_OBJ);
    CF_RUN(
        "objcopy -I binary -O pe-x86-64 -B i386:x86-64 "
        "--rename-section "
        ".data=.sbat,contents,alloc,load,readonly,data " SBAT_CSV " " SBAT_OBJ
    );

    CF_BANNER(LD_TAG "Linking sealboot...");
    CF_RUN(
        "lld-link -subsystem:efi_application -entry:efi_main -out:" SEALBOOT_EFI
        " %s %s %s " SBAT_OBJ,
        IPP_LIB_STATIC,
        shared_objs,
        boot_objs
    );
    printf(LD_TAG "  %s\n", SEALBOOT_EFI);
}

CF_TARGET(esp, CF_DEPENDS(link_boot), CF_HIDDEN) {
    CF_BANNER(SN_TAG "Signing...");
    CF_RUN(
        "out=$(sbsign --key " KEY_FILE " --cert " CRT_FILE " --output " GRUB_EFI
        " " SEALBOOT_EFI " 2>&1) || { echo \"$out\"; exit 1; }"
    );
    printf(SN_TAG "  %s\n", GRUB_EFI);

    CF_BANNER(FS_TAG "EFI filesystem operations...");
    printf(FS_TAG "  Formatting %s (%dM)\n", ESP_IMG, ESP_SIZE_MB);
    CF_RM(ESP_IMG);
    CF_RUN(
        "dd if=/dev/zero of=" ESP_IMG " bs=1M count=%d status=none",
        ESP_SIZE_MB
    );
    CF_RUN("mkfs.fat " ESP_IMG " >/dev/null");
    CF_RUN("mmd -i " ESP_IMG " ::/EFI " ESP_BOOT_DIR);
    CF_RUN("mmd -i " ESP_IMG " " ESP_SEALCORE_DIR);

    printf(FS_TAG "  %s -> %s\n", SHIM, ESP_BOOTX64);
    CF_RUN("mcopy -i " ESP_IMG " " SHIM " " ESP_BOOTX64);
    printf(FS_TAG "  %s -> %s\n", MM, ESP_MM64);
    CF_RUN("mcopy -i " ESP_IMG " " MM " " ESP_MM64);
    printf(FS_TAG "  %s -> %s\n", GRUB_EFI, ESP_GRUB64);
    CF_RUN("mcopy -i " ESP_IMG " " GRUB_EFI " " ESP_GRUB64);
    printf(FS_TAG "  %s -> %s\n", SEALCORE_BIN, ESP_SEALCORE64);
    CF_RUN("mcopy -i " ESP_IMG " " SEALCORE_BIN " " ESP_SEALCORE64);
    printf(FS_TAG "  %s -> %s\n", DER_FILE, ESP_MOK_DER);
    CF_RUN("mcopy -i " ESP_IMG " " DER_FILE " " ESP_MOK_DER);
    printf(FS_TAG "  %s ready\n", ESP_IMG);
}

CF_TARGET(vars, CF_HIDDEN) {
    CF_MKDIR(BUILD_DIR);
    CF_BANNER(VR_TAG "Enrolling MS keys...");
    CF_CP(OVMF_VARS_TEMPLATE, VARS_FD);
    CF_RUN(
        "out=$(virt-fw-vars --input " VARS_FD " --output " VARS_FD " "
        "--enroll-redhat --secure-boot 2>&1) || { echo \"$out\"; exit 1; }"
    );
    printf(VR_TAG "  Snapshot %s\n", VARS_CLEAN_FD);
    CF_CP(VARS_FD, VARS_CLEAN_FD);
    printf(VR_TAG "  %s\n", VARS_FD);
}

CF_TARGET(reset, CF_HELP_STRING("Restore pre-enrollment VARS")) {
    CF_BANNER(VR_TAG "Resetting...");
    printf(VR_TAG "  %s -> %s\n", VARS_CLEAN_FD, VARS_FD);
    CF_CP(VARS_CLEAN_FD, VARS_FD);
    printf(VR_TAG "  Reset\n");
}

CF_TARGET(run, CF_HELP_STRING("Launch QEMU")) {
    CF_BANNER(QM_TAG "Running QEMU...");
    CF_RUN(
        "qemu-system-x86_64 "
        "-machine q35,accel=kvm -cpu host -m 2G "
        "-drive if=pflash,format=raw,readonly=on,file=" OVMF_CODE " "
        "-drive if=pflash,format=raw,file=" VARS_FD " "
        "-drive format=raw,file=" ESP_IMG " "
        "-net none"
    );
}

CF_TARGET(
    all,
    CF_DEPENDS(keys),
    CF_DEPENDS(esp),
    CF_DEPENDS(vars),
    CF_HELP_STRING("Build and sign SealCore")
) {
    printf(OK_TAG "All done!\n");
}

CF_TARGET(
    sb,
    CF_DEPENDS(keys),
    CF_DEPENDS(esp),
    CF_DEPENDS(vars),
    CF_HELP_STRING("Same as 'all' but enrolls MOK key")
) {
    CF_BANNER(SB_TAG "Enrolling MOK...");
    CF_RUN(
        "out=$(virt-fw-vars --input " VARS_FD " --output " VARS_FD " "
        "--add-mok " MOK_GUID " " CRT_FILE
        " 2>&1) || { echo \"$out\"; exit 1; }"
    );
    printf(SB_TAG "  Snapshot %s\n", VARS_CLEAN_FD);
    CF_CP(VARS_FD, VARS_CLEAN_FD);
    printf(OK_TAG "All done!\n");
}
