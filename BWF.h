#pragma once

#ifndef BWF_H
#define BWF_H

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned short u_int16;
typedef unsigned long u_long;
typedef short int int16;

#define PF_TABLE_BITS 12
#define PF_TABLE_SZ_16 4096
#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"

#ifdef AE_OS_WIN
    typedef unsigned short PixelType;
    #include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"

#include "BWF_Strings.h"

#define MAJOR_VERSION 5
#define MINOR_VERSION 0
#define BUG_VERSION 0
#define STAGE_VERSION PF_Stage_DEVELOP
#define BUILD_VERSION 2

enum {
    BWF_INPUT = 0,
    BWF_THRESHOLD,
    BWF_CONTRAST,
    BWF_EDGE_INTENSITY,
    BWF_CENTER,
    BWF_LIGHT_INTENSITY,
    BWF_LIGHT_LENGTH,
    BWF_CONTOUR_SIMPLIFY,
    BWF_LINE_DENSITY,
    BWF_BLUR_STRENGTH,
    BWF_BLUR_QUALITY,
    BWF_FLASH_INTENSITY,
    BWF_FLASH_COLOR,
    BWF_BG_COLOR,
    BWF_NUM_PARAMS
};

enum {
    THRESHOLD_DISK_ID = 1,
    CONTRAST_DISK_ID,
    EDGE_INTENSITY_DISK_ID,
    CENTER_DISK_ID,
    LIGHT_INTENSITY_DISK_ID,
    LIGHT_LENGTH_DISK_ID,
    CONTOUR_SIMPLIFY_DISK_ID,
    LINE_DENSITY_DISK_ID,
    BLUR_STRENGTH_DISK_ID,
    BLUR_QUALITY_DISK_ID,
    FLASH_INTENSITY_DISK_ID,
    FLASH_COLOR_DISK_ID,
    BG_COLOR_DISK_ID,
};

typedef struct BWFInfo {
    PF_FpLong threshold;
    PF_FpLong contrast;
    PF_FpLong edgeIntensity;
    PF_FpLong centerX;
    PF_FpLong centerY;
    PF_FpLong lightIntensity;
    PF_FpLong lightLength;
    PF_FpLong contourSimplify;
    A_long lineDensity;
    PF_FpLong blurStrength;
    A_long blurQuality;
    PF_FpLong flashIntensity;
    PF_Pixel8 flashColor8;
    PF_Pixel8 bgColor8;
} BWFInfo, *BWFInfoP, **BWFInfoH;

extern "C" {
    DllExport PF_Err EffectMain(
        PF_Cmd cmd,
        PF_InData *in_data,
        PF_OutData *out_data,
        PF_ParamDef *params[],
        PF_LayerDef *output,
        void *extra);
}

#endif