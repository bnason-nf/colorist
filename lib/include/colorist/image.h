// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_IMAGE_H
#define COLORIST_IMAGE_H

#include "colorist/context.h"
#include "colorist/types.h"

struct clProfile;
struct clRaw;
struct cJSON;

typedef struct clImage
{
    int width;
    int height;
    int depth;
    int size;
    uint8_t * pixels;
    struct clProfile * profile;
} clImage;

clImage * clImageCreate(struct clContext * C, int width, int height, int depth, struct clProfile * profile);
clImage * clImageRotate(struct clContext * C, clImage * image, int cwTurns);
clImage * clImageConvert(struct clContext * C, clImage * srcImage, int taskCount, int width, int height, int depth, struct clProfile * dstProfile, clTonemap tonemap);
clImage * clImageCrop(struct clContext * C, clImage * srcImage, int x, int y, int w, int h, clBool keepSrc);
clImage * clImageApplyHALD(struct clContext * C, clImage * image, clImage * hald, int haldDims);
clImage * clImageResize(struct clContext * C, clImage * image, int width, int height, clFilter resizeFilter);
clBool clImageAdjustRect(struct clContext * C, clImage * image, int * x, int * y, int * w, int * h);
void clImageColorGrade(struct clContext * C, clImage * image, int taskCount, int dstColorDepth, int * outLuminance, float * outGamma, clBool verbose);
void clImageSetPixel(struct clContext * C, clImage * image, int x, int y, int r, int g, int b, int a);
void clImageDebugDump(struct clContext * C, clImage * image, int x, int y, int w, int h, int extraIndent);
void clImageDebugDumpJSON(struct clContext * C, struct cJSON * jsonOutput, clImage * image, int x, int y, int w, int h);
void clImageDestroy(struct clContext * C, clImage * image);
void clImageLogCreate(struct clContext * C, int width, int height, int depth, struct clProfile * profile);
clImage * clImageParseString(struct clContext * C, const char * str, int depth, struct clProfile * profile);

int clDepthToBytes(clContext * C, int depth);

#endif // ifndef COLORIST_IMAGE_H
