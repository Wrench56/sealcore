#include "cforge.h"

#if CF_VERSION_BELOW(1, 1, 0)
#error "CForge too old!"
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

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

#define KEY_FILE BUILD_DIR "/MOK.key"
#define CRT_FILE BUILD_DIR "/MOK.crt"
#define DER_FILE BUILD_DIR "/MOK.der"
#define MOK_SUBJECT "/CN=SealCore MOK/"
#define MOK_GUID "77fa9abd-0359-4d32-bd60-28f4e78f784b"

#define SBAT_CSV BUILD_DIR "/sbat.csv"
#define SBAT_OBJ BUILD_DIR "/sbat.o"

#define SEALCORE_EFI BUILD_DIR "/sealcore.efi"
#define GRUB_EFI BUILD_DIR "/grubx64.efi"

#define ESP_IMG BUILD_DIR "/esp.img"
#define ESP_SIZE_MB 64

#define VARS_FD BUILD_DIR "/my_VARS.fd"
#define VARS_CLEAN_FD BUILD_DIR "/my_VARS.clean.fd"

#define ESP_BOOT_DIR "::/EFI/BOOT"
#define ESP_BOOTX64 ESP_BOOT_DIR "/BOOTX64.EFI"
#define ESP_MM64 ESP_BOOT_DIR "/mmx64.efi"
#define ESP_GRUB64 ESP_BOOT_DIR "/grubx64.efi"
#define ESP_MOK_DER "::/MOK.der"

#define LOGO_PNG ASSETS_DIR "/sealcore_164x164.png"
#define LOGO_BIN "includes/logo.hex"

#define KY_TAG "[  " CF_GREEN "KY" CF_RESET "  ] "
#define LG_TAG "[  " CF_BLUE "LG" CF_RESET "  ] "
#define CC_TAG "[  " CF_YELLOW "CC" CF_RESET "  ] "
#define SB_TAG "[  " CF_YELLOW "SB" CF_RESET "  ] "
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

static void compile_sources(const char* pattern) {
    for CF_GLOBS_EACH(pattern, file) {
        char* output = CF_MAP(
            file,
            CF_MAP_EXT("o"),
            CF_MAP_DIRS(OBJ_DIR "/")
        );
        printf(CC_TAG "  %s -> %s\n", file, output);
        CF_RUNP(
            "clang -target x86_64-unknown-windows -ffreestanding "
            "-fno-stack-protector -fno-zero-initialized-in-bss "
            "-mno-red-zone -maes -Wall -Wextra -Iincludes/ "
            "-I/usr/include/efi -I/usr/include/efi/x86_64 -c %s -o %s",
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

CF_TARGET(compile, CF_DEPENDS(logo), CF_HIDDEN) {
    CF_MKDIR(OBJ_DIR);
    CF_BANNER(CC_TAG "Compiling...");

    compile_sources("src/*.c");
    compile_sources("src/**/*.c");
    compile_sources("src/**/**/*.c");
}

CF_TARGET(link, CF_DEPENDS(compile), CF_HIDDEN) {
    char* objs = CF_JOIN_GLOB(CF_GLOB(OBJ_DIR "/*.o"), " ");

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

    CF_BANNER(LD_TAG "Linking...");
    CF_RUN(
        "lld-link -subsystem:efi_application -entry:efi_main -out:" SEALCORE_EFI
        " %s " SBAT_OBJ,
        objs
    );
    printf(LD_TAG "  %s\n", SEALCORE_EFI);
}

CF_TARGET(esp, CF_DEPENDS(link), CF_HIDDEN) {
    CF_BANNER(SN_TAG "Signing...");
    CF_RUN(
        "out=$(sbsign --key " KEY_FILE " --cert " CRT_FILE " --output " GRUB_EFI
        " " SEALCORE_EFI " 2>&1) || { echo \"$out\"; exit 1; }"
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

    printf(FS_TAG "  %s -> %s\n", SHIM, ESP_BOOTX64);
    CF_RUN("mcopy -i " ESP_IMG " " SHIM " " ESP_BOOTX64);
    printf(FS_TAG "  %s -> %s\n", MM, ESP_MM64);
    CF_RUN("mcopy -i " ESP_IMG " " MM " " ESP_MM64);
    printf(FS_TAG "  %s -> %s\n", GRUB_EFI, ESP_GRUB64);
    CF_RUN("mcopy -i " ESP_IMG " " GRUB_EFI " " ESP_GRUB64);
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
