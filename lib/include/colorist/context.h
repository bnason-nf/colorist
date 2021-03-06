// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#ifndef COLORIST_CONTEXT_H
#define COLORIST_CONTEXT_H

#include "colorist/types.h"

// for cmsContext
#include "lcms2.h"

// for va_list
#include <stdarg.h>

struct clContext;
struct clImage;
struct clProfilePrimaries;
struct clRaw;
struct cJSON;

typedef enum clAction
{
    CL_ACTION_NONE = 0,
    CL_ACTION_CALC,
    CL_ACTION_CONVERT,
    CL_ACTION_GENERATE,
    CL_ACTION_IDENTIFY,
    CL_ACTION_MODIFY,
    CL_ACTION_REPORT,

    CL_ACTION_ERROR
} clAction;

clAction clActionFromString(struct clContext * C, const char * str);
const char * clActionToString(struct clContext * C, clAction action);

typedef struct clWriteParams
{
    int quality;
    int rate;
} clWriteParams;
typedef struct clImage * (* clFormatReadFunc)(struct clContext * C, const char * formatName, struct clRaw * input);
typedef clBool (* clFormatWriteFunc)(struct clContext * C, struct clImage * image, const char * formatName, struct clRaw * output, struct clWriteParams * writeParams);

typedef enum clFormatDepth
{
    CL_FORMAT_DEPTH_8 = 0,
    CL_FORMAT_DEPTH_8_OR_10,
    CL_FORMAT_DEPTH_8_OR_16,
    CL_FORMAT_DEPTH_8_TO_16
} clFormatDepth;

#define CL_FORMAT_MAX_EXTENSIONS 4
#define CL_FORMAT_MAX_SIGNATURES 4
typedef struct clFormat
{
    const char * name;
    const char * description;
    const char * mimeType;
    const char * extensions[CL_FORMAT_MAX_EXTENSIONS];
    const unsigned char * signatures[CL_FORMAT_MAX_SIGNATURES];
    size_t signatureLengths[CL_FORMAT_MAX_SIGNATURES];
    clFormatDepth depth;
    clBool usesQuality;
    clBool usesRate;
    clFormatReadFunc readFunc;
    clFormatWriteFunc writeFunc;
} clFormat;

clBool clFormatExists(struct clContext * C, const char * formatName);
int clFormatMaxDepth(struct clContext * C, const char * formatName);
int clFormatBestDepth(struct clContext * C, const char * formatName, int reqDepth);
const char * clFormatDetect(struct clContext * C, const char * filename);

typedef enum clTonemap
{
    CL_TONEMAP_AUTO = 0,
    CL_TONEMAP_ON,
    CL_TONEMAP_OFF
} clTonemap;

clTonemap clTonemapFromString(struct clContext * C, const char * str);
const char * clTonemapToString(struct clContext * C, clTonemap tonemap);

// Filter enumeration and comments taken directly from stb_image_resize
// (with minor tweaks like DEFAULT -> AUTO, addition of NEAREST)
typedef enum clFilter
{
    CL_FILTER_AUTO         = 0, // Choose best based on upsampling or downsampling
    CL_FILTER_BOX          = 1, // A trapezoid w/1-pixel wide ramps, same result as box for integer scale ratios
    CL_FILTER_TRIANGLE     = 2, // On upsampling, produces same results as bilinear texture filtering
    CL_FILTER_CUBICBSPLINE = 3, // The cubic b-spline (aka Mitchell-Netrevalli with B=1,C=0), gaussian-esque
    CL_FILTER_CATMULLROM   = 4, // An interpolating cubic spline
    CL_FILTER_MITCHELL     = 5, // Mitchell-Netrevalli filter with B=1/3, C=1/3
    CL_FILTER_NEAREST      = 6, // Doesn't use stb_image_resize, just does an obvious nearest neighbor

    CL_FILTER_INVALID      = -1
} clFilter;

clFilter clFilterFromString(struct clContext * C, const char * str);
const char * clFilterToString(struct clContext * C, clFilter filter);

typedef void *(* clContextAllocFunc)(struct clContext * C, size_t bytes); // C will be NULL when allocating the clContext itself
typedef void (* clContextFreeFunc)(struct clContext * C, void * ptr);
typedef void (* clContextLogFunc)(struct clContext * C, const char * section, int indent, const char * format, va_list args);
typedef void (* clContextLogErrorFunc)(struct clContext * C, const char * format, va_list args);

// Internal defaults for clContextSystem, use clContextLog*() / clAllocate / clFree below
void * clContextDefaultAlloc(struct clContext * C, size_t bytes);
void clContextDefaultFree(struct clContext * C, void * ptr);
void clContextDefaultLog(struct clContext * C, const char * section, int indent, const char * format, va_list args);
void clContextDefaultLogError(struct clContext * C, const char * format, va_list args);

typedef struct clContextSystem
{
    clContextAllocFunc alloc;
    clContextFreeFunc free;
    clContextLogFunc log;
    clContextLogErrorFunc error;
} clContextSystem;

typedef struct clConversionParams
{
    clBool autoGrade;            // -a
    int bpp;                     // -b
    const char * copyright;      // -c
    const char * description;    // -d
    const char * formatName;     // -f
    float gamma;                 // -g
    const char * hald;           // --hald
    int jobs;                    // -j
    int luminance;               // -l
    const char * iccOverrideOut; // -o
    float primaries[8];          // -p
    int quality;                 // -q
    int resizeW;                 // -r
    int resizeH;                 // -r
    clFilter resizeFilter;       // -r
    const char * stripTags;      // -s
    clTonemap tonemap;           // -t
    int rect[4];                 // -z
    int jp2rate;                 // -2
} clConversionParams;

void clConversionParamsSetDefaults(struct clContext * C, clConversionParams * params);

typedef struct clFormatRecord
{
    clFormat format;
    struct clFormatRecord * next;
} clFormatRecord;

struct clFormat * clContextFindFormat(struct clContext * C, const char * formatName);
void clContextRegisterBuiltinFormats(struct clContext * C);

typedef struct clContext
{
    clContextSystem system;

    cmsContext lcms;

    clFormatRecord * formats;

    clAction action;
    clConversionParams params;   // see above
    clBool help;                 // -h
    const char * iccOverrideIn;  // -i
    clBool verbose;              // -v
    clBool ccmmAllowed;          // --ccmm
    const char * inputFilename;  // index 0
    const char * outputFilename; // index 1
} clContext;

struct clImage;

#define clAllocate(BYTES) C->system.alloc(C, BYTES)
#define clAllocateStruct(T) (T *)C->system.alloc(C, sizeof(T))
#define clFree(P) C->system.free(C, P)
char * clContextStrdup(clContext * C, const char * str);

// Any/all of the clContextSystem struct can be NULL, including the struct itself. Any NULL values will use the default.
// No need to allocate the clContextSystem structure; just put it on the stack. Any values will be shallow copied.
clContext * clContextCreate(clContextSystem * system);
void clContextDestroy(clContext * C);
void clContextRegisterFormat(clContext * C, clFormat * format);

void clContextLog(clContext * C, const char * section, int indent, const char * format, ...);
void clContextLogError(clContext * C, const char * format, ...);

void clContextPrintSyntax(clContext * C);
void clContextPrintVersions(clContext * C);
void clContextPrintArgs(clContext * C);
clBool clContextParseArgs(clContext * C, int argc, const char * argv[]);

struct clImage * clContextRead(clContext * C, const char * filename, const char * iccOverride, const char ** outFormatName);
clBool clContextWrite(clContext * C, struct clImage * image, const char * filename, const char * formatName, int quality, int rate);
char * clContextWriteURI(struct clContext * C, struct clImage * image, const char * formatName, int quality, int rate);

clBool clContextGetStockPrimaries(struct clContext * C, const char * name, struct clProfilePrimaries * outPrimaries);
clBool clContextGetRawStockPrimaries(struct clContext * C, const char * name, float outPrimaries[8]);

int clContextConvert(clContext * C);
int clContextGenerate(clContext * C, struct cJSON * output); // output here only used in ACTION_CALC
int clContextIdentify(clContext * C, struct cJSON * output);
int clContextModify(clContext * C);
int clContextReport(clContext * C);

#define TIMING_FORMAT "--> %g sec"
#define OVERALL_TIMING_FORMAT "==> %g sec"

// Enable on VS builds to dump memory leaks at the end of a debug session
// #define WIN32_MEMORY_LEAK_DETECTION

#endif // ifndef MAIN_H
