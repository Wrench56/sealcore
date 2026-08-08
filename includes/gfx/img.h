#ifndef SEALCORE_IMG_H
#define SEALCORE_IMG_H

#include <efi.h>
#include <stdint.h>

typedef union __attribute__((packed)) {
    struct __attribute__((packed)) {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };
    uint32_t raw;
} pixel_data_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    pixel_data_t* data;
} image_t;

void setup_gfx(EFI_SYSTEM_TABLE* st);
void draw_img(image_t* image, uint32_t x, uint32_t y);

#endif // SEALCORE_IMG_H
