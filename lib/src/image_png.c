// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/image.h"

#include "colorist/context.h"
#include "colorist/profile.h"

#include "png.h"

#include <string.h>

clImage * clImageReadPNG(struct clContext * C, const char * filename)
{
    clImage * image;
    clProfile * profile = NULL;

    png_structp png;
    png_infop info;
    png_byte header[8];

    char * iccpProfileName;
    int iccpCompression;
    unsigned char * iccpData;
    png_uint_32 iccpDataLen;

    int rawWidth, rawHeight;
    png_byte rawColorType;
    png_byte rawBitDepth;

    int imgBitDepth = 8;
    int imgBytesPerChannel = 1;

    png_bytep * rowPointers;
    int y;

    FILE * fp = fopen(filename, "rb");
    if (!fp) {
        clContextLogError(C, "cannot open PNG: '%s'", filename);
        return NULL;
    }

    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8)) {
        fclose(fp);
        clContextLogError(C, "not a PNG: '%s'", filename);
        return NULL;
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png_create_info_struct(png);
    setjmp(png_jmpbuf(png));
    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    if (png_get_iCCP(png, info, &iccpProfileName, &iccpCompression, &iccpData, &iccpDataLen) == PNG_INFO_iCCP) {
        profile = clProfileParse(C, iccpData, iccpDataLen, iccpProfileName);
    }

    rawWidth = png_get_image_width(png, info);
    rawHeight = png_get_image_height(png, info);
    rawColorType = png_get_color_type(png, info);
    rawBitDepth = png_get_bit_depth(png, info);

    if (rawColorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    if ((rawColorType == PNG_COLOR_TYPE_GRAY) && (rawBitDepth < 8)) {
        png_set_expand_gray_1_2_4_to_8(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }

    if ((rawColorType == PNG_COLOR_TYPE_RGB) ||
        (rawColorType == PNG_COLOR_TYPE_GRAY) ||
        (rawColorType == PNG_COLOR_TYPE_PALETTE))
    {
        png_set_filler(png, 0xFFFF, PNG_FILLER_AFTER);
    }

    if ((rawColorType == PNG_COLOR_TYPE_GRAY) ||
        (rawColorType == PNG_COLOR_TYPE_GRAY_ALPHA))
    {
        png_set_gray_to_rgb(png);
    }

    if (rawBitDepth == 16) {
        png_set_swap(png);
        imgBitDepth = 16;
        imgBytesPerChannel = 2;
    }

    png_read_update_info(png, info);
    setjmp(png_jmpbuf(png));

    clImageLogCreate(C, rawWidth, rawHeight, imgBitDepth, profile);
    image = clImageCreate(C, rawWidth, rawHeight, imgBitDepth, profile);
    if (profile) {
        clProfileDestroy(C, profile);
    }
    rowPointers = (png_bytep *)clAllocate(sizeof(png_bytep) * rawHeight);
    if (imgBytesPerChannel == 1) {
        uint8_t * pixels = (uint8_t *)image->pixels;
        for (y = 0; y < rawHeight; ++y) {
            rowPointers[y] = &pixels[4 * y * rawWidth];
        }
    } else {
        uint16_t * pixels = (uint16_t *)image->pixels;
        for (y = 0; y < rawHeight; ++y) {
            rowPointers[y] = (png_byte *)&pixels[4 * y * rawWidth];
        }
    }
    png_read_image(png, rowPointers);
    png_destroy_read_struct(&png, &info, NULL);
    clFree(rowPointers);
    fclose(fp);
    return image;
}

struct writeInfo
{
    struct clContext * C;
    clRaw * dst;
    uint32_t offset;
};

static void writeCallback(png_structp png, png_bytep data, png_size_t length)
{
    struct writeInfo * wi = (struct writeInfo *)png_get_io_ptr(png);
    if ((wi->offset + length) > wi->dst->size) {
        uint32_t newSize = wi->dst->size;
        if (!newSize)
            newSize = 8;
        do {
            newSize *= 2;
        } while (newSize < (wi->offset + length));
        clRawRealloc(wi->C, wi->dst, newSize);
    }
    memcpy(wi->dst->ptr + wi->offset, data, length);
    wi->offset += length;
}

clBool clImageWritePNGRaw(struct clContext * C, clImage * image, struct clRaw * dst)
{
    int y;
    png_bytep * rowPointers;
    int imgBytesPerChannel = (image->depth == 16) ? 2 : 1;
    clRaw rawProfile;
    struct writeInfo wi;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);

    memset(&rawProfile, 0, sizeof(rawProfile));
    if (!clProfilePack(C, image->profile, &rawProfile)) {
        return clFalse;
    }

    COLORIST_ASSERT(png && info);
    setjmp(png_jmpbuf(png));

    wi.C = C;
    wi.offset = 0;
    wi.dst = dst;
    png_set_write_fn(png, &wi, writeCallback, NULL);

    png_set_IHDR(
        png,
        info,
        image->width, image->height,
        image->depth,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
        );
    png_set_iCCP(png, info, image->profile->description, 0, rawProfile.ptr, rawProfile.size);
    png_write_info(png, info);

    rowPointers = (png_bytep *)clAllocate(sizeof(png_bytep) * image->height);
    if (imgBytesPerChannel == 1) {
        uint8_t * pixels = (uint8_t *)image->pixels;
        for (y = 0; y < image->height; ++y) {
            rowPointers[y] = &pixels[4 * y * image->width];
        }
    } else {
        uint16_t * pixels = (uint16_t *)image->pixels;
        for (y = 0; y < image->height; ++y) {
            rowPointers[y] = (png_byte *)&pixels[4 * y * image->width];
        }
        png_set_swap(png);
    }

    png_write_image(png, rowPointers);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);

    clFree(rowPointers);
    clRawFree(C, &rawProfile);
    dst->size = wi.offset;
    return clTrue;
}

clBool clImageWritePNG(struct clContext * C, clImage * image, const char * filename)
{
    FILE * outfile = NULL;
    clRaw dst;

    memset(&dst, 0, sizeof(dst));
    if (!clImageWritePNGRaw(C, image, &dst)) {
        goto writePNGCleanup;
    }

    outfile = fopen(filename, "wb");
    if (!outfile) {
        clContextLogError(C, "ERROR: can't open PNG for write: %s", filename);
        goto writePNGCleanup;
    }

    if (fwrite(dst.ptr, dst.size, 1, outfile) != 1) {
        clContextLogError(C, "ERROR: can't write %d bytes to PNG: %s", dst.size, filename);
        goto writePNGCleanup;
    }

writePNGCleanup:
    if (outfile)
        fclose(outfile);
    clRawFree(C, &dst);
    return clTrue;
}

char * clImageWritePNGURI(struct clContext * C, clImage * image)
{
    clRaw dst;
    char * b64;
    int b64Len;
    char * output;
    static const char prefixURI[] = "data:image/png;base64,";
    static int prefixURILen = sizeof(prefixURI) - 1;

    memset(&dst, 0, sizeof(dst));
    if (!clImageWritePNGRaw(C, image, &dst)) {
        return NULL;
    }

    b64 = clRawToBase64(C, &dst);
    if (!b64) {
        clRawFree(C, &dst);
        return NULL;
    }
    b64Len = strlen(b64);

    output = clAllocate(prefixURILen + b64Len + 1);
    memcpy(output, prefixURI, prefixURILen);
    memcpy(output + prefixURILen, b64, b64Len);
    output[prefixURILen + b64Len] = 0;

    clFree(b64);
    clRawFree(C, &dst);
    return output;
}
