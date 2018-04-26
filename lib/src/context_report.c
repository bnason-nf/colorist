// ---------------------------------------------------------------------------
//                         Copyright Joe Drago 2018.
//         Distributed under the Boost Software License, Version 1.0.
//            (See accompanying file LICENSE_1_0.txt or copy at
//                  http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include "colorist/context.h"

#include "colorist/embedded.h"
#include "colorist/image.h"
#include "colorist/pixelmath.h"
#include "colorist/profile.h"
#include "colorist/task.h"

#include "cJSON.h"

#include <string.h>

#define FAIL() { returnCode = 1; goto reportCleanup; }

static clBool reportBasicInfo(clContext * C, clImage * image, cJSON * payload)
{
    clProfilePrimaries primaries;
    clProfileCurve curve;
    int maxLuminance;
    cJSON * jsonICC;
    cJSON * jsonPrimaries;
    char * text;

    if (!clProfileQuery(C, image->profile, &primaries, &curve, &maxLuminance)) {
        return clFalse;
    }

    cJSON_AddItemToObject(payload, "width", cJSON_CreateNumber(image->width));
    cJSON_AddItemToObject(payload, "height", cJSON_CreateNumber(image->height));

    jsonICC = cJSON_CreateObject();
    {
        text = clProfileGetMLU(C, image->profile, "desc", "en", "US");
        if (text == NULL) {
            text = clContextStrdup(C, "Unknown");
        }
        cJSON_AddItemToObject(jsonICC, "description", cJSON_CreateString(text));
        clFree(text);

        jsonPrimaries = cJSON_CreateArray();
        {
            cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.red[0]));
            cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.red[1]));
            cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.green[0]));
            cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.green[1]));
            cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.blue[0]));
            cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.blue[1]));
            cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.white[0]));
            cJSON_AddItemToArray(jsonPrimaries, cJSON_CreateNumber(primaries.white[1]));
        }

        cJSON_AddItemToObject(jsonICC, "primaries", jsonPrimaries);
        cJSON_AddItemToObject(jsonICC, "luminance", cJSON_CreateNumber(maxLuminance));
    }
    cJSON_AddItemToObject(payload, "icc", jsonICC);

    return clTrue;
}

static clBool reportHeatMap(clContext * C, clImage * image, cJSON * payload)
{
    cJSON_AddItemToObject(payload, "heatmap_example_number", cJSON_CreateNumber(42));
    return clTrue;
}

int clContextReport(clContext * C)
{
    Timer overall, t;
    int returnCode = 0;

    clImage * image = NULL;

    cJSON * payload = cJSON_CreateObject();
    FILE * outf = NULL;

    clContextLog(C, "action", 0, "Report: %s -> %s", C->inputFilename, C->outputFilename);
    timerStart(&overall);

    clContextLog(C, "decode", 0, "Reading: %s (%d bytes)", C->inputFilename, clFileSize(C->inputFilename));
    timerStart(&t);
    image = clContextRead(C, C->inputFilename, NULL);
    if (image == NULL) {
        return 1;
    }
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

    // Basic Info
    {
        timerStart(&t);
        cJSON_AddItemToObject(payload, "filename", cJSON_CreateString(C->inputFilename));
        if (!reportBasicInfo(C, image, payload)) {
            FAIL();
        }
    }

    // if "create heat map report" ...
    {
        clContextLog(C, "heatmap", 0, "Generating heatmap...");
        timerStart(&t);
        if (!reportHeatMap(C, image, payload)) {
            FAIL();
        }
        clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));
    }

    timerStart(&t);
    {
        static const char payloadPrefix[] = "var COLORIST_DATA = ";
        static const char coloristDataMarker[] = "__COLORIST_DATA__";
        const char * coloristDataInjectLoc = strstr((const char *)reportTemplateBinaryData, coloristDataMarker);
        const char * afterPtr;
        int beforeLen, afterLen;
        char * payloadString;
        if (!coloristDataInjectLoc) {
            clContextLogError(C, "Template does not contain the string \"%s\", bailing out", coloristDataMarker);
            FAIL();
        }

        beforeLen = (int)(coloristDataInjectLoc - (const char *)reportTemplateBinaryData);
        afterPtr = coloristDataInjectLoc + strlen(coloristDataMarker);
        afterLen = strlen(afterPtr);

        outf = fopen(C->outputFilename, "wb");
        if (!outf) {
            clContextLogError(C, "Cant open report file for write: %s", C->outputFilename);
            FAIL();
        }

        payloadString = cJSON_Print(payload);
        if (!payloadString) {
            clContextLogError(C, "failed to create payload string!");
            FAIL();
        }

        fwrite(reportTemplateBinaryData, beforeLen, 1, outf);
        fwrite(payloadPrefix, strlen(payloadPrefix), 1, outf);
        fwrite(payloadString, strlen(payloadString), 1, outf);
        fwrite(afterPtr, afterLen, 1, outf);
        fclose(outf);
        outf = NULL;
    }

    clContextLog(C, "encode", 1, "Wrote %d bytes.", clFileSize(C->outputFilename));
    clContextLog(C, "timing", -1, TIMING_FORMAT, timerElapsedSeconds(&t));

reportCleanup:
    if (image)
        clImageDestroy(C, image);

    cJSON_Delete(payload);
    if (outf)
        fclose(outf);

    if (returnCode == 0) {
        clContextLog(C, "action", 0, "Conversion complete.");
        clContextLog(C, "timing", -1, OVERALL_TIMING_FORMAT, timerElapsedSeconds(&overall));
    }
    return returnCode;
}