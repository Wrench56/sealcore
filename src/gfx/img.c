#include "img.h"

static const EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GRAPHICS_OUTPUT_PROTOCOL* prot = NULL;
static EFI_SYSTEM_TABLE* syst = NULL;

void setup_gfx(EFI_SYSTEM_TABLE* st) {
    st->BootServices
        ->LocateProtocol((EFI_GUID*) &gop_guid, NULL, (VOID**) &prot);
    syst = st;
}

void draw_img(image_t* image, uint32_t x, uint32_t y) {
    const EFI_GRAPHICS_PIXEL_FORMAT pform = prot->Mode->Info->PixelFormat;
    const uint32_t width = prot->Mode->Info->PixelsPerScanLine;
    uint32_t* const fb = (uint32_t*) prot->Mode->FrameBufferBase;

    switch (pform) {
        case PixelBlueGreenRedReserved8BitPerColor:
            for (uint32_t i = 0; i < image->height; i++) {
                for (uint32_t j = 0; j < image->width; j++) {
                    uint32_t color = image->data[i * image->width + j].raw;

                    if (__builtin_expect((color >> 24) == 0, 0)) {
                        continue;
                    }

                    fb[(y + i) * width + (x + j)] = color;
                }
            }
            break;
        default:
            syst->ConOut->OutputString(
                syst->ConOut,
                L"Error: Invalid pixel format!\r\n"
            );
            break;
    }
}
