//
// Copyright (C) 2024 nihui
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


#include "capture_cvi.h"

#if defined __linux__
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#if __riscv_vector
#include <riscv_vector.h>
#endif

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/prctl.h>

// 0 = unknown
// 1 = milkv-duo
// 2 = milkv-duo256m
static int get_device_model()
{
    static int device_model = -1;

    if (device_model >= 0)
        return device_model;

    device_model = 0;

    FILE* fp = fopen("/proc/device-tree/model", "rb");
    if (fp)
    {
        char buf[1024];
        fgets(buf, 1024, fp);
        fclose(fp);

        if (strncmp(buf, "Cvitek. CV180X ASIC. C906.", 36) == 0)
        {
            // milkv duo
            device_model = 1;
        }
        if (strncmp(buf, "Cvitek. CV181X ASIC. C906.", 36) == 0)
        {
            // milkv duo 256
            device_model = 2;
        }
    }

    return device_model;
}

static bool is_device_whitelisted()
{
    const int device_model = get_device_model();

    if (device_model == 1)
    {
        // milkv duo
        return true;
    }
    if (device_model == 2)
    {
        // milkv duo 256
        return true;
    }

    return false;
}

extern "C"
{

typedef unsigned char           CVI_UCHAR;
typedef unsigned char           CVI_U8;
typedef unsigned short          CVI_U16;
typedef unsigned int            CVI_U32;
typedef unsigned int            CVI_HANDLE;

typedef signed char             CVI_S8;
typedef char                    CVI_CHAR;
typedef short                   CVI_S16;
typedef int                     CVI_S32;

typedef unsigned long           CVI_UL;
typedef signed long             CVI_SL;

typedef float                   CVI_FLOAT;
typedef double                  CVI_DOUBLE;

typedef void                    CVI_VOID;
typedef bool                    CVI_BOOL;

typedef uint64_t                CVI_U64;
typedef int64_t                 CVI_S64;

typedef size_t                  CVI_SIZE_T;

#define ALIGN(x, a)      (((x) + ((a)-1)) & ~((a)-1))

#define CVI_NULL                0L
#define CVI_SUCCESS             0
#define CVI_FAILURE             (-1)
#define CVI_FAILURE_ILLEGAL_PARAM (-2)
#define CVI_TRUE                1
#define CVI_FALSE               0

#define VB_INVALID_POOLID (-1U)

typedef CVI_U32 VB_POOL;

typedef enum _VB_REMAP_MODE_E {
    VB_REMAP_MODE_NONE = 0,
    VB_REMAP_MODE_NOCACHE = 1,
    VB_REMAP_MODE_CACHED = 2,
    VB_REMAP_MODE_BUTT
} VB_REMAP_MODE_E;

#define MAX_VB_POOL_NAME_LEN (32)
typedef struct _VB_POOL_CONFIG_S {
    CVI_U32 u32BlkSize;
    CVI_U32 u32BlkCnt;
    VB_REMAP_MODE_E enRemapMode;
    CVI_CHAR acName[MAX_VB_POOL_NAME_LEN];
} VB_POOL_CONFIG_S;

#define VB_MAX_COMM_POOLS (16)
typedef struct _VB_CONFIG_S {
    CVI_U32 u32MaxPoolCnt;
    VB_POOL_CONFIG_S astCommPool[VB_MAX_COMM_POOLS];
} VB_CONFIG_S;

typedef CVI_S32 (*PFN_CVI_SYS_Exit)();
typedef CVI_S32 (*PFN_CVI_SYS_Init)();
typedef CVI_S32 (*PFN_CVI_SYS_IonFlushCache)(CVI_U64 u64PhyAddr, CVI_VOID* pVirAddr, CVI_U32 u32Len);
typedef CVI_S32 (*PFN_CVI_SYS_IonInvalidateCache)(CVI_U64 u64PhyAddr, CVI_VOID* pVirAddr, CVI_U32 u32Len);
typedef CVI_S32 (*PFN_CVI_SYS_Munmap)(void* pVirAddr, CVI_U32 u32Size);
typedef void* (*PFN_CVI_SYS_Mmap)(CVI_U64 u64PhyAddr, CVI_U32 u32Size);
typedef void* (*PFN_CVI_SYS_MmapCache)(CVI_U64 u64PhyAddr, CVI_U32 u32Size);

typedef CVI_S32 (*PFN_CVI_VB_DestroyPool)(VB_POOL Pool);
typedef CVI_S32 (*PFN_CVI_VB_Exit)();
typedef CVI_S32 (*PFN_CVI_VB_Init)();
typedef CVI_S32 (*PFN_CVI_VB_SetConfig)(const VB_CONFIG_S* pstVbConfig);
typedef VB_POOL (*PFN_CVI_VB_CreatePool)(VB_POOL_CONFIG_S* pstVbPoolCfg);

typedef CVI_S32 (*PFN_CVI_SYS_VI_Open)();
typedef CVI_S32 (*PFN_CVI_SYS_VI_Close)();
}

static void* libsys_atomic = 0;
static void* libsys = 0;

static PFN_CVI_SYS_Exit CVI_SYS_Exit = 0;
static PFN_CVI_SYS_Init CVI_SYS_Init = 0;
static PFN_CVI_SYS_IonFlushCache CVI_SYS_IonFlushCache = 0;
static PFN_CVI_SYS_IonInvalidateCache CVI_SYS_IonInvalidateCache = 0;
static PFN_CVI_SYS_Munmap CVI_SYS_Munmap = 0;
static PFN_CVI_SYS_Mmap CVI_SYS_Mmap = 0;
static PFN_CVI_SYS_MmapCache CVI_SYS_MmapCache = 0;

static PFN_CVI_VB_DestroyPool CVI_VB_DestroyPool = 0;
static PFN_CVI_VB_Exit CVI_VB_Exit = 0;
static PFN_CVI_VB_Init CVI_VB_Init = 0;
static PFN_CVI_VB_SetConfig CVI_VB_SetConfig = 0;
static PFN_CVI_VB_CreatePool CVI_VB_CreatePool = 0;

static PFN_CVI_SYS_VI_Open CVI_SYS_VI_Open = 0;
static PFN_CVI_SYS_VI_Close CVI_SYS_VI_Close = 0;

static int unload_sys_library()
{
    if (libsys_atomic)
    {
        dlclose(libsys_atomic);
        libsys_atomic = 0;
    }

    if (libsys)
    {
        dlclose(libsys);
        libsys = 0;
    }

    CVI_SYS_Exit = 0;
    CVI_SYS_Init = 0;
    CVI_SYS_IonFlushCache = 0;
    CVI_SYS_IonInvalidateCache = 0;
    CVI_SYS_Munmap = 0;
    CVI_SYS_Mmap = 0;
    CVI_SYS_MmapCache = 0;

    CVI_VB_DestroyPool = 0;
    CVI_VB_Exit = 0;
    CVI_VB_Init = 0;
    CVI_VB_SetConfig = 0;
    CVI_VB_CreatePool = 0;

    CVI_SYS_VI_Open = 0;
    CVI_SYS_VI_Close = 0;

    return 0;
}

static int load_sys_library()
{
    if (libsys)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        fprintf(stderr, "this device is not whitelisted for capture cvi\n");
        return -1;
    }

    libsys_atomic = dlopen("libatomic.so.1", RTLD_GLOBAL | RTLD_LAZY);
    if (!libsys_atomic)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libsys = dlopen("libsys.so", RTLD_LOCAL | RTLD_NOW);
    if (!libsys)
    {
        libsys = dlopen("/mnt/system/lib/libsys.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libsys)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    CVI_SYS_Exit = (PFN_CVI_SYS_Exit)dlsym(libsys, "CVI_SYS_Exit");
    CVI_SYS_Init = (PFN_CVI_SYS_Init)dlsym(libsys, "CVI_SYS_Init");
    CVI_SYS_IonFlushCache = (PFN_CVI_SYS_IonFlushCache)dlsym(libsys, "CVI_SYS_IonFlushCache");
    CVI_SYS_IonInvalidateCache = (PFN_CVI_SYS_IonInvalidateCache)dlsym(libsys, "CVI_SYS_IonInvalidateCache");
    CVI_SYS_Munmap = (PFN_CVI_SYS_Munmap)dlsym(libsys, "CVI_SYS_Munmap");
    CVI_SYS_Mmap = (PFN_CVI_SYS_Mmap)dlsym(libsys, "CVI_SYS_Mmap");
    CVI_SYS_MmapCache = (PFN_CVI_SYS_MmapCache)dlsym(libsys, "CVI_SYS_MmapCache");

    CVI_VB_DestroyPool = (PFN_CVI_VB_DestroyPool)dlsym(libsys, "CVI_VB_DestroyPool");
    CVI_VB_Exit = (PFN_CVI_VB_Exit)dlsym(libsys, "CVI_VB_Exit");
    CVI_VB_Init = (PFN_CVI_VB_Init)dlsym(libsys, "CVI_VB_Init");
    CVI_VB_SetConfig = (PFN_CVI_VB_SetConfig)dlsym(libsys, "CVI_VB_SetConfig");
    CVI_VB_CreatePool = (PFN_CVI_VB_CreatePool)dlsym(libsys, "CVI_VB_CreatePool");

    CVI_SYS_VI_Open = (PFN_CVI_SYS_VI_Open)dlsym(libsys, "CVI_SYS_VI_Open");
    CVI_SYS_VI_Close = (PFN_CVI_SYS_VI_Close)dlsym(libsys, "CVI_SYS_VI_Close");

    return 0;

OUT:
    unload_sys_library();

    return -1;
}

class sys_library_loader
{
public:
    bool ready;

    sys_library_loader()
    {
        ready = (load_sys_library() == 0);
    }

    ~sys_library_loader()
    {
        unload_sys_library();
    }
};

static sys_library_loader sys;


extern "C"
{
typedef CVI_S32 VI_DEV;
typedef CVI_S32 VI_PIPE;
typedef CVI_S32 VI_CHN;

typedef struct _SIZE_S {
    CVI_U32 u32Width;
    CVI_U32 u32Height;
} SIZE_S;

typedef struct _RECT_S {
    CVI_S32 s32X;
    CVI_S32 s32Y;
    CVI_U32 u32Width;
    CVI_U32 u32Height;
} RECT_S;

typedef enum _VI_INTF_MODE_E {
    VI_MODE_BT656 = 0, /* ITU-R BT.656 YUV4:2:2 */
    VI_MODE_BT601, /* ITU-R BT.601 YUV4:2:2 */
    VI_MODE_DIGITAL_CAMERA, /* digatal camera mode */
    VI_MODE_BT1120_STANDARD, /* BT.1120 progressive mode */
    VI_MODE_BT1120_INTERLEAVED, /* BT.1120 interstage mode */
    VI_MODE_MIPI, /* MIPI RAW mode */
    VI_MODE_MIPI_YUV420_NORMAL, /* MIPI YUV420 normal mode */
    VI_MODE_MIPI_YUV420_LEGACY, /* MIPI YUV420 legacy mode */
    VI_MODE_MIPI_YUV422, /* MIPI YUV422 mode */
    VI_MODE_LVDS, /* LVDS mode */
    VI_MODE_HISPI, /* HiSPi mode */
    VI_MODE_SLVS, /* SLVS mode */

    VI_MODE_BUTT
} VI_INTF_MODE_E;

typedef enum _VI_WORK_MODE_E {
    VI_WORK_MODE_1Multiplex = 0, /* 1 Multiplex mode */
    VI_WORK_MODE_2Multiplex, /* 2 Multiplex mode */
    VI_WORK_MODE_3Multiplex, /* 3 Multiplex mode */
    VI_WORK_MODE_4Multiplex, /* 4 Multiplex mode */

    VI_WORK_MODE_BUTT
} VI_WORK_MODE_E;

typedef enum _VI_SCAN_MODE_E {
    VI_SCAN_INTERLACED = 0, /* interlaced mode */
    VI_SCAN_PROGRESSIVE, /* progressive mode */

    VI_SCAN_BUTT
} VI_SCAN_MODE_E;

typedef enum _VI_YUV_DATA_SEQ_E {
    VI_DATA_SEQ_VUVU = 0,
    VI_DATA_SEQ_UVUV,

    VI_DATA_SEQ_UYVY, /* The input sequence of YUV is UYVY */
    VI_DATA_SEQ_VYUY, /* The input sequence of YUV is VYUY */
    VI_DATA_SEQ_YUYV, /* The input sequence of YUV is YUYV */
    VI_DATA_SEQ_YVYU, /* The input sequence of YUV is YVYU */

    VI_DATA_SEQ_BUTT
} VI_YUV_DATA_SEQ_E;

typedef enum _VI_VSYNC_E {
    VI_VSYNC_FIELD = 0, /* Field/toggle mode:a signal reversal means a new frame or a field */
    VI_VSYNC_PULSE, /* Pusle/effective mode:a pusle or an effective signal means a new frame or a field */

    VI_VSYNC_BUTT
} VI_VSYNC_E;

typedef enum _VI_VSYNC_NEG_E {
    VI_VSYNC_NEG_HIGH = 0,
    VI_VSYNC_NEG_LOW,
    VI_VSYNC_NEG_BUTT
} VI_VSYNC_NEG_E;

typedef enum _VI_HSYNC_E {
    VI_HSYNC_VALID_SINGNAL = 0, /* the h-sync is valid signal mode */
    VI_HSYNC_PULSE, /* the h-sync is pulse mode, a new pulse means the beginning of a new line */

    VI_HSYNC_BUTT
} VI_HSYNC_E;

typedef enum _VI_HSYNC_NEG_E {
    VI_HSYNC_NEG_HIGH = 0,
    VI_HSYNC_NEG_LOW,
    VI_HSYNC_NEG_BUTT
} VI_HSYNC_NEG_E;

typedef enum _VI_VSYNC_VALID_E {
    VI_VSYNC_NORM_PULSE = 0,
    VI_VSYNC_VALID_SIGNAL,

    VI_VSYNC_VALID_BUTT
} VI_VSYNC_VALID_E;

typedef enum _VI_VSYNC_VALID_NEG_E {
    VI_VSYNC_VALID_NEG_HIGH = 0,
    VI_VSYNC_VALID_NEG_LOW,
    VI_VSYNC_VALID_NEG_BUTT
} VI_VSYNC_VALID_NEG_E;

typedef struct _VI_TIMING_BLANK_S {
    CVI_U32 u32HsyncHfb; /* RW;Horizontal front blanking width */
    CVI_U32 u32HsyncAct; /* RW;Horizontal effetive width */
    CVI_U32 u32HsyncHbb; /* RW;Horizontal back blanking width */
    CVI_U32 u32VsyncVfb;
    CVI_U32 u32VsyncVact;
    CVI_U32 u32VsyncVbb;
    CVI_U32 u32VsyncVbfb;
    CVI_U32 u32VsyncVbact;
    CVI_U32 u32VsyncVbbb;
} VI_TIMING_BLANK_S;

typedef struct _VI_SYNC_CFG_S {
    VI_VSYNC_E enVsync;
    VI_VSYNC_NEG_E enVsyncNeg;
    VI_HSYNC_E enHsync;
    VI_HSYNC_NEG_E enHsyncNeg;
    VI_VSYNC_VALID_E enVsyncValid;
    VI_VSYNC_VALID_NEG_E enVsyncValidNeg;
    VI_TIMING_BLANK_S stTimingBlank;
} VI_SYNC_CFG_S;

typedef enum _VI_DATA_TYPE_E {
    VI_DATA_TYPE_YUV = 0,
    VI_DATA_TYPE_RGB,
    VI_DATA_TYPE_YUV_EARLY,

    VI_DATA_TYPE_BUTT
} VI_DATA_TYPE_E;

typedef enum _WDR_MODE_E {
    WDR_MODE_NONE = 0,
    WDR_MODE_BUILT_IN,
    WDR_MODE_QUDRA,

    WDR_MODE_2To1_LINE,
    WDR_MODE_2To1_FRAME,
    WDR_MODE_2To1_FRAME_FULL_RATE,

    WDR_MODE_3To1_LINE,
    WDR_MODE_3To1_FRAME,
    WDR_MODE_3To1_FRAME_FULL_RATE,

    WDR_MODE_4To1_LINE,
    WDR_MODE_4To1_FRAME,
    WDR_MODE_4To1_FRAME_FULL_RATE,

    WDR_MODE_MAX,
} WDR_MODE_E;

typedef struct _VI_WDR_ATTR_S {
    WDR_MODE_E enWDRMode; /* RW; WDR mode.*/
    CVI_U32 u32CacheLine; /* RW; WDR cache line.*/
    CVI_BOOL bSyntheticWDR; /* RW; Synthetic WDR mode.*/
} VI_WDR_ATTR_S;

typedef enum _BAYER_FORMAT_E {
    BAYER_FORMAT_BG = 0,
    BAYER_FORMAT_GB,
    BAYER_FORMAT_GR,
    BAYER_FORMAT_RG,
    BAYER_FORMAT_MAX
} BAYER_FORMAT_E;

#define VI_MAX_ADCHN_NUM (4UL)
typedef struct _VI_DEV_ATTR_S {
    VI_INTF_MODE_E enIntfMode; /* RW;Interface mode */
    VI_WORK_MODE_E enWorkMode; /* RW;Work mode */

    VI_SCAN_MODE_E enScanMode; /* RW;Input scanning mode (progressive or interlaced) */
    CVI_S32 as32AdChnId[VI_MAX_ADCHN_NUM]; /* RW;AD channel ID. Typically, the default value -1 is recommended */

    /* The below members must be configured in BT.601 mode or DC mode and are invalid in other modes */
    VI_YUV_DATA_SEQ_E enDataSeq; /* RW;Input data sequence (only the YUV format is supported) */
    VI_SYNC_CFG_S stSynCfg; /* RW;Sync timing. This member must be configured in BT.601 mode or DC mode */

    VI_DATA_TYPE_E enInputDataType;

    SIZE_S stSize; /* RW;Input size */

    VI_WDR_ATTR_S stWDRAttr; /* RW;Attribute of WDR */

    BAYER_FORMAT_E enBayerFormat; /* RW;Bayer format of Device */

    CVI_U32 chn_num; /* R; total chnannels sended from dev */

    CVI_U32 snrFps; /* R; snr init fps from isp pub attr */
} VI_DEV_ATTR_S;

typedef enum _VI_PIPE_BYPASS_MODE_E {
    VI_PIPE_BYPASS_NONE,
    VI_PIPE_BYPASS_FE,
    VI_PIPE_BYPASS_BE,

    VI_PIPE_BYPASS_BUTT
} VI_PIPE_BYPASS_MODE_E;

typedef enum _PIXEL_FORMAT_E {
    PIXEL_FORMAT_RGB_888 = 0,
    PIXEL_FORMAT_BGR_888,
    PIXEL_FORMAT_RGB_888_PLANAR,
    PIXEL_FORMAT_BGR_888_PLANAR,

    PIXEL_FORMAT_ARGB_1555, // 4,
    PIXEL_FORMAT_ARGB_4444,
    PIXEL_FORMAT_ARGB_8888,

    PIXEL_FORMAT_RGB_BAYER_8BPP, // 7,
    PIXEL_FORMAT_RGB_BAYER_10BPP,
    PIXEL_FORMAT_RGB_BAYER_12BPP,
    PIXEL_FORMAT_RGB_BAYER_14BPP,
    PIXEL_FORMAT_RGB_BAYER_16BPP,

    PIXEL_FORMAT_YUV_PLANAR_422, // 12,
    PIXEL_FORMAT_YUV_PLANAR_420,
    PIXEL_FORMAT_YUV_PLANAR_444,
    PIXEL_FORMAT_YUV_400,

    PIXEL_FORMAT_HSV_888, // 16,
    PIXEL_FORMAT_HSV_888_PLANAR,

    PIXEL_FORMAT_NV12, // 18,
    PIXEL_FORMAT_NV21,
    PIXEL_FORMAT_NV16,
    PIXEL_FORMAT_NV61,
    PIXEL_FORMAT_YUYV,
    PIXEL_FORMAT_UYVY,
    PIXEL_FORMAT_YVYU,
    PIXEL_FORMAT_VYUY,

    PIXEL_FORMAT_FP32_C1 = 32, // 32
    PIXEL_FORMAT_FP32_C3_PLANAR,
    PIXEL_FORMAT_INT32_C1,
    PIXEL_FORMAT_INT32_C3_PLANAR,
    PIXEL_FORMAT_UINT32_C1,
    PIXEL_FORMAT_UINT32_C3_PLANAR,
    PIXEL_FORMAT_BF16_C1,
    PIXEL_FORMAT_BF16_C3_PLANAR,
    PIXEL_FORMAT_INT16_C1,
    PIXEL_FORMAT_INT16_C3_PLANAR,
    PIXEL_FORMAT_UINT16_C1,
    PIXEL_FORMAT_UINT16_C3_PLANAR,
    PIXEL_FORMAT_INT8_C1,
    PIXEL_FORMAT_INT8_C3_PLANAR,
    PIXEL_FORMAT_UINT8_C1,
    PIXEL_FORMAT_UINT8_C3_PLANAR,

    PIXEL_FORMAT_8BIT_MODE = 48, //48

    PIXEL_FORMAT_MAX
} PIXEL_FORMAT_E;

typedef enum _COMPRESS_MODE_E {
    COMPRESS_MODE_NONE = 0,
    COMPRESS_MODE_TILE,
    COMPRESS_MODE_LINE,
    COMPRESS_MODE_FRAME,
    COMPRESS_MODE_BUTT
} COMPRESS_MODE_E;

typedef enum _DATA_BITWIDTH_E {
    DATA_BITWIDTH_8 = 0,
    DATA_BITWIDTH_10,
    DATA_BITWIDTH_12,
    DATA_BITWIDTH_14,
    DATA_BITWIDTH_16,
    DATA_BITWIDTH_MAX
} DATA_BITWIDTH_E;

typedef struct _FRAME_RATE_CTRL_S {
    CVI_S32 s32SrcFrameRate; /* RW; source frame rate */
    CVI_S32 s32DstFrameRate; /* RW; dest frame rate */
} FRAME_RATE_CTRL_S;

typedef struct _VI_PIPE_ATTR_S {
    VI_PIPE_BYPASS_MODE_E enPipeBypassMode;
    CVI_BOOL bYuvSkip; /* RW;YUV skip enable */
    CVI_BOOL bIspBypass; /* RW;ISP bypass enable */
    CVI_U32 u32MaxW; /* RW;Range[VI_PIPE_MIN_WIDTH,VI_PIPE_MAX_WIDTH];Maximum width */
    CVI_U32 u32MaxH; /* RW;Range[VI_PIPE_MIN_HEIGHT,VI_PIPE_MAX_HEIGHT];Maximum height */
    PIXEL_FORMAT_E enPixFmt; /* RW;Pixel format */
    COMPRESS_MODE_E enCompressMode; /* RW;Compress mode.*/
    DATA_BITWIDTH_E enBitWidth; /* RW;Bit width*/
    CVI_BOOL bNrEn; /* RW;3DNR enable */
    CVI_BOOL bSharpenEn; /* RW;Sharpen enable*/
    FRAME_RATE_CTRL_S stFrameRate; /* RW;Frame rate */
    CVI_BOOL bDiscardProPic;
    CVI_BOOL bYuvBypassPath; /* RW;ISP YUV bypass enable */
} VI_PIPE_ATTR_S;






typedef enum _DYNAMIC_RANGE_E {
    DYNAMIC_RANGE_SDR8 = 0,
    DYNAMIC_RANGE_SDR10,
    DYNAMIC_RANGE_HDR10,
    DYNAMIC_RANGE_HLG,
    DYNAMIC_RANGE_SLF,
    DYNAMIC_RANGE_XDR,
    DYNAMIC_RANGE_MAX
} DYNAMIC_RANGE_E;

typedef enum _VIDEO_FORMAT_E {
    VIDEO_FORMAT_LINEAR = 0,
    VIDEO_FORMAT_MAX
} VIDEO_FORMAT_E;

typedef struct _VI_CHN_ATTR_S {
    SIZE_S stSize; /* RW;Channel out put size */
    PIXEL_FORMAT_E enPixelFormat; /* RW;Pixel format */
    DYNAMIC_RANGE_E enDynamicRange; /* RW;Dynamic Range */
    VIDEO_FORMAT_E enVideoFormat; /* RW;Video format */
    COMPRESS_MODE_E enCompressMode; /* RW;256B Segment compress or no compress. */
    CVI_BOOL bMirror; /* RW;Mirror enable */
    CVI_BOOL bFlip; /* RW;Flip enable */
    CVI_U32 u32Depth; /* RW;Range [0,8];Depth */
    FRAME_RATE_CTRL_S stFrameRate; /* RW;Frame rate */
    CVI_U32 u32BindVbPool; /*chn bind vb*/
} VI_CHN_ATTR_S;



typedef enum _VI_CROP_COORDINATE_E {
    VI_CROP_RATIO_COOR = 0, /* Ratio coordinate */
    VI_CROP_ABS_COOR, /* Absolute coordinate */
    VI_CROP_BUTT
} VI_CROP_COORDINATE_E;

typedef struct _VI_CROP_INFO_S {
    CVI_BOOL bEnable; /* RW;CROP enable*/
    VI_CROP_COORDINATE_E enCropCoordinate; /* RW;Coordinate mode of the crop start point*/
    RECT_S stCropRect; /* RW;CROP rectangular*/
} VI_CROP_INFO_S;


typedef enum _COLOR_GAMUT_E {
    COLOR_GAMUT_BT601 = 0,
    COLOR_GAMUT_BT709,
    COLOR_GAMUT_BT2020,
    COLOR_GAMUT_USER,
    COLOR_GAMUT_MAX
} COLOR_GAMUT_E;

typedef struct _VIDEO_FRAME_S {
    CVI_U32 u32Width;
    CVI_U32 u32Height;
    PIXEL_FORMAT_E enPixelFormat;
    BAYER_FORMAT_E enBayerFormat;
    VIDEO_FORMAT_E enVideoFormat;
    COMPRESS_MODE_E enCompressMode;
    DYNAMIC_RANGE_E enDynamicRange;
    COLOR_GAMUT_E enColorGamut;
    CVI_U32 u32Stride[3];

    CVI_U64 u64PhyAddr[3];
    CVI_U8 *pu8VirAddr[3];
#ifdef __arm__
    CVI_U32 u32VirAddrPadding[3];
#endif
    CVI_U32 u32Length[3];

    CVI_S16 s16OffsetTop;
    CVI_S16 s16OffsetBottom;
    CVI_S16 s16OffsetLeft;
    CVI_S16 s16OffsetRight;

    CVI_U32 u32TimeRef;
    CVI_U64 u64PTS;

    void *pPrivateData;
#ifdef __arm__
    CVI_U32 u32PrivateDataPadding;
#endif
    CVI_U32 u32FrameFlag;
} VIDEO_FRAME_S;

typedef struct _VIDEO_FRAME_INFO_S {
    VIDEO_FRAME_S stVFrame;
    CVI_U32 u32PoolId;
} VIDEO_FRAME_INFO_S;

typedef CVI_S32 (*PFN_CVI_VI_SetDevAttr)(VI_DEV ViDev, const VI_DEV_ATTR_S *pstDevAttr);
typedef CVI_S32 (*PFN_CVI_VI_EnableDev)(VI_DEV ViDev);
typedef CVI_S32 (*PFN_CVI_VI_DisableDev)(VI_DEV ViDev);
typedef CVI_S32 (*PFN_CVI_VI_CreatePipe)(VI_PIPE ViPipe, const VI_PIPE_ATTR_S *pstPipeAttr);
typedef CVI_S32 (*PFN_CVI_VI_DestroyPipe)(VI_PIPE ViPipe);
typedef CVI_S32 (*PFN_CVI_VI_SetPipeAttr)(VI_PIPE ViPipe, const VI_PIPE_ATTR_S *pstPipeAttr);
typedef CVI_S32 (*PFN_CVI_VI_StartPipe)(VI_PIPE ViPipe);
typedef CVI_S32 (*PFN_CVI_VI_StopPipe)(VI_PIPE ViPipe);
typedef CVI_S32 (*PFN_CVI_VI_AttachVbPool)(VI_PIPE ViPipe, VI_CHN ViChn, VB_POOL VbPool);
typedef CVI_S32 (*PFN_CVI_VI_DetachVbPool)(VI_PIPE ViPipe, VI_CHN ViChn);
typedef CVI_S32 (*PFN_CVI_VI_SetChnAttr)(VI_PIPE ViPipe, VI_CHN ViChn, VI_CHN_ATTR_S *pstChnAttr);
typedef CVI_S32 (*PFN_CVI_VI_EnableChn)(VI_PIPE ViPipe, VI_CHN ViChn);
typedef CVI_S32 (*PFN_CVI_VI_DisableChn)(VI_PIPE ViPipe, VI_CHN ViChn);
typedef CVI_S32 (*PFN_CVI_VI_SetChnCrop)(VI_PIPE ViPipe, VI_CHN ViChn, const VI_CROP_INFO_S  *pstCropInfo);
typedef CVI_S32 (*PFN_CVI_VI_GetChnFrame)(VI_PIPE ViPipe, VI_CHN ViChn, VIDEO_FRAME_INFO_S *pstFrameInfo, CVI_S32 s32MilliSec);
typedef CVI_S32 (*PFN_CVI_VI_ReleaseChnFrame)(VI_PIPE ViPipe, VI_CHN ViChn, const VIDEO_FRAME_INFO_S *pstFrameInfo);
}

extern "C"
{

#define VPSS_INVALID_FRMRATE -1
#define VPSS_CHN0             0
#define VPSS_CHN1             1
#define VPSS_CHN2             2
#define VPSS_CHN3             3
#define VPSS_INVALID_CHN     -1
#define VPSS_INVALID_GRP     -1

typedef CVI_S32 VPSS_GRP;
typedef CVI_S32 VPSS_CHN;

typedef struct _VPSS_GRP_ATTR_S {
    CVI_U32 u32MaxW;
    CVI_U32 u32MaxH;
    PIXEL_FORMAT_E enPixelFormat;
    FRAME_RATE_CTRL_S stFrameRate;
    CVI_U8 u8VpssDev;
} VPSS_GRP_ATTR_S;

typedef enum _ASPECT_RATIO_E {
    ASPECT_RATIO_NONE = 0,
    ASPECT_RATIO_AUTO,
    ASPECT_RATIO_MANUAL,
    ASPECT_RATIO_MAX
} ASPECT_RATIO_E;

typedef struct _ASPECT_RATIO_S {
    ASPECT_RATIO_E enMode;
    CVI_BOOL bEnableBgColor;
    CVI_U32 u32BgColor;
    RECT_S stVideoRect;
} ASPECT_RATIO_S;

typedef enum _VPSS_ROUNDING_E {
    VPSS_ROUNDING_TO_EVEN = 0,
    VPSS_ROUNDING_AWAY_FROM_ZERO,
    VPSS_ROUNDING_TRUNCATE,
    VPSS_ROUNDING_MAX,
} VPSS_ROUNDING_E;

typedef struct _VPSS_NORMALIZE_S {
    CVI_BOOL bEnable;
    CVI_FLOAT factor[3];
    CVI_FLOAT mean[3];
    VPSS_ROUNDING_E rounding;
} VPSS_NORMALIZE_S;

typedef struct _VPSS_CHN_ATTR_S {
    CVI_U32 u32Width;
    CVI_U32 u32Height;
    VIDEO_FORMAT_E enVideoFormat;
    PIXEL_FORMAT_E enPixelFormat;
    FRAME_RATE_CTRL_S stFrameRate;
    CVI_BOOL bMirror;
    CVI_BOOL bFlip;
    CVI_U32 u32Depth;
    ASPECT_RATIO_S stAspectRatio;
    VPSS_NORMALIZE_S stNormalize;
} VPSS_CHN_ATTR_S;

// typedef enum _ROTATION_E {
//     ROTATION_0 = 0,
//     ROTATION_90,
//     ROTATION_180,
//     ROTATION_270,
//     ROTATION_XY_FLIP,
//     ROTATION_MAX
// } ROTATION_E;

typedef enum _VPSS_CROP_COORDINATE_E {
    VPSS_CROP_RATIO_COOR = 0,
    VPSS_CROP_ABS_COOR,
} VPSS_CROP_COORDINATE_E;

typedef struct _VPSS_CROP_INFO_S {
    CVI_BOOL bEnable;
    VPSS_CROP_COORDINATE_E enCropCoordinate;
    RECT_S stCropRect;
} VPSS_CROP_INFO_S;

typedef CVI_S32 (*PFN_CVI_VPSS_AttachVbPool)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VB_POOL hVbPool);
typedef CVI_S32 (*PFN_CVI_VPSS_CreateGrp)(VPSS_GRP VpssGrp, const VPSS_GRP_ATTR_S* pstGrpAttr);
typedef CVI_S32 (*PFN_CVI_VPSS_DestroyGrp)(VPSS_GRP VpssGrp);
typedef CVI_S32 (*PFN_CVI_VPSS_DetachVbPool)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn);
typedef CVI_S32 (*PFN_CVI_VPSS_DisableChn)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn);
typedef CVI_S32 (*PFN_CVI_VPSS_EnableChn)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn);
typedef CVI_S32 (*PFN_CVI_VPSS_GetChnAttr)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VPSS_CHN_ATTR_S* pstChnAttr);
typedef CVI_S32 (*PFN_CVI_VPSS_GetChnFrame)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VIDEO_FRAME_INFO_S* pstVideoFrame, CVI_S32 s32MilliSec);
typedef CVI_S32 (*PFN_CVI_VPSS_ReleaseChnFrame)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, const VIDEO_FRAME_INFO_S* pstVideoFrame);
typedef CVI_S32 (*PFN_CVI_VPSS_ResetGrp)(VPSS_GRP VpssGrp);
typedef CVI_S32 (*PFN_CVI_VPSS_SendFrame)(VPSS_GRP VpssGrp, const VIDEO_FRAME_INFO_S* pstVideoFrame, CVI_S32 s32MilliSec);
typedef CVI_S32 (*PFN_CVI_VPSS_SetChnAttr)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, const VPSS_CHN_ATTR_S* pstChnAttr);
// typedef CVI_S32 (*PFN_CVI_VPSS_SetChnRotation)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, ROTATION_E enRotation);
typedef CVI_S32 (*PFN_CVI_VPSS_SetChnCrop)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, const VPSS_CROP_INFO_S *pstCropInfo);
typedef CVI_S32 (*PFN_CVI_VPSS_StartGrp)(VPSS_GRP VpssGrp);
typedef CVI_S32 (*PFN_CVI_VPSS_StopGrp)(VPSS_GRP VpssGrp);

}

static void* libvpu_sys = 0;
static void* libvpu_awb = 0;
static void* libvpu_ae = 0;
static void* libvpu_isp_algo = 0;
static void* libvpu_isp = 0;
static void* libvpu_cvi_bin_isp = 0;
static void* libvpu_cvi_bin = 0;
static void* libvpu = 0;

static PFN_CVI_VI_SetDevAttr CVI_VI_SetDevAttr = 0;
static PFN_CVI_VI_EnableDev CVI_VI_EnableDev = 0;
static PFN_CVI_VI_DisableDev CVI_VI_DisableDev = 0;
static PFN_CVI_VI_CreatePipe CVI_VI_CreatePipe = 0;
static PFN_CVI_VI_DestroyPipe CVI_VI_DestroyPipe = 0;
static PFN_CVI_VI_SetPipeAttr CVI_VI_SetPipeAttr = 0;
static PFN_CVI_VI_StartPipe CVI_VI_StartPipe = 0;
static PFN_CVI_VI_StopPipe CVI_VI_StopPipe = 0;
static PFN_CVI_VI_AttachVbPool CVI_VI_AttachVbPool = 0;
static PFN_CVI_VI_DetachVbPool CVI_VI_DetachVbPool = 0;
static PFN_CVI_VI_SetChnAttr CVI_VI_SetChnAttr = 0;
static PFN_CVI_VI_EnableChn CVI_VI_EnableChn = 0;
static PFN_CVI_VI_DisableChn CVI_VI_DisableChn = 0;
static PFN_CVI_VI_SetChnCrop CVI_VI_SetChnCrop = 0;
static PFN_CVI_VI_GetChnFrame CVI_VI_GetChnFrame = 0;
static PFN_CVI_VI_ReleaseChnFrame CVI_VI_ReleaseChnFrame = 0;

static PFN_CVI_VPSS_AttachVbPool CVI_VPSS_AttachVbPool = 0;
static PFN_CVI_VPSS_CreateGrp CVI_VPSS_CreateGrp = 0;
static PFN_CVI_VPSS_DestroyGrp CVI_VPSS_DestroyGrp = 0;
static PFN_CVI_VPSS_DetachVbPool CVI_VPSS_DetachVbPool = 0;
static PFN_CVI_VPSS_DisableChn CVI_VPSS_DisableChn = 0;
static PFN_CVI_VPSS_EnableChn CVI_VPSS_EnableChn = 0;
static PFN_CVI_VPSS_GetChnAttr CVI_VPSS_GetChnAttr = 0;
static PFN_CVI_VPSS_GetChnFrame CVI_VPSS_GetChnFrame = 0;
static PFN_CVI_VPSS_ReleaseChnFrame CVI_VPSS_ReleaseChnFrame = 0;
static PFN_CVI_VPSS_ResetGrp CVI_VPSS_ResetGrp = 0;
static PFN_CVI_VPSS_SendFrame CVI_VPSS_SendFrame = 0;
static PFN_CVI_VPSS_SetChnAttr CVI_VPSS_SetChnAttr = 0;
// static PFN_CVI_VPSS_SetChnRotation CVI_VPSS_SetChnRotation = 0;
static PFN_CVI_VPSS_SetChnCrop CVI_VPSS_SetChnCrop = 0;
static PFN_CVI_VPSS_StartGrp CVI_VPSS_StartGrp = 0;
static PFN_CVI_VPSS_StopGrp CVI_VPSS_StopGrp = 0;

static int unload_vpu_library()
{
    if (libvpu_sys)
    {
        dlclose(libvpu_sys);
        libvpu_sys = 0;
    }

    if (libvpu_awb)
    {
        dlclose(libvpu_awb);
        libvpu_awb = 0;
    }

    if (libvpu_ae)
    {
        dlclose(libvpu_ae);
        libvpu_ae = 0;
    }

    if (libvpu_isp_algo)
    {
        dlclose(libvpu_isp_algo);
        libvpu_isp_algo = 0;
    }

    if (libvpu_isp)
    {
        dlclose(libvpu_isp);
        libvpu_isp = 0;
    }

    if (libvpu_cvi_bin_isp)
    {
        dlclose(libvpu_cvi_bin_isp);
        libvpu_cvi_bin_isp = 0;
    }

    if (libvpu_cvi_bin)
    {
        dlclose(libvpu_cvi_bin);
        libvpu_cvi_bin = 0;
    }

    if (libvpu)
    {
        dlclose(libvpu);
        libvpu = 0;
    }

    CVI_VI_SetDevAttr = 0;
    CVI_VI_EnableDev = 0;
    CVI_VI_DisableDev = 0;
    CVI_VI_CreatePipe = 0;
    CVI_VI_DestroyPipe = 0;
    CVI_VI_SetPipeAttr = 0;
    CVI_VI_StartPipe = 0;
    CVI_VI_StopPipe = 0;
    CVI_VI_AttachVbPool = 0;
    CVI_VI_DetachVbPool = 0;
    CVI_VI_SetChnAttr = 0;
    CVI_VI_EnableChn = 0;
    CVI_VI_DisableChn = 0;
    CVI_VI_SetChnCrop = 0;
    CVI_VI_GetChnFrame = 0;
    CVI_VI_ReleaseChnFrame = 0;

    CVI_VPSS_AttachVbPool = 0;
    CVI_VPSS_CreateGrp = 0;
    CVI_VPSS_DestroyGrp = 0;
    CVI_VPSS_DetachVbPool = 0;
    CVI_VPSS_DisableChn = 0;
    CVI_VPSS_EnableChn = 0;
    CVI_VPSS_GetChnAttr = 0;
    CVI_VPSS_GetChnFrame = 0;
    CVI_VPSS_ReleaseChnFrame = 0;
    CVI_VPSS_ResetGrp = 0;
    CVI_VPSS_SendFrame = 0;
    CVI_VPSS_SetChnAttr = 0;
    // CVI_VPSS_SetChnRotation = 0;
    CVI_VPSS_SetChnCrop = 0;
    CVI_VPSS_StartGrp = 0;
    CVI_VPSS_StopGrp = 0;

    return 0;
}

static int load_vpu_library()
{
    if (libvpu)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        fprintf(stderr, "this device is not whitelisted for capture cvi\n");
        return -1;
    }

    libvpu_sys = dlopen("libsys.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libvpu_sys)
    {
        libvpu_sys = dlopen("/mnt/system/lib/libsys.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libvpu_sys)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libvpu_awb = dlopen("libawb.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libvpu_awb)
    {
        libvpu_awb = dlopen("/mnt/system/lib/libawb.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libvpu_awb)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libvpu_ae = dlopen("libae.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libvpu_ae)
    {
        libvpu_ae = dlopen("/mnt/system/lib/libae.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libvpu_ae)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libvpu_isp_algo = dlopen("libisp_algo.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libvpu_isp_algo)
    {
        libvpu_isp_algo = dlopen("/mnt/system/lib/libisp_algo.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libvpu_isp_algo)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libvpu_isp = dlopen("libisp.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libvpu_isp)
    {
        libvpu_isp = dlopen("/mnt/system/lib/libisp.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libvpu_isp)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libvpu_cvi_bin_isp = dlopen("libcvi_bin_isp.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libvpu_cvi_bin_isp)
    {
        libvpu_cvi_bin_isp = dlopen("/mnt/system/lib/libcvi_bin_isp.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libvpu_cvi_bin_isp)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libvpu_cvi_bin = dlopen("libcvi_bin.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libvpu_cvi_bin)
    {
        libvpu_cvi_bin = dlopen("/mnt/system/lib/libcvi_bin.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libvpu_cvi_bin)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libvpu = dlopen("libvpu.so", RTLD_LOCAL | RTLD_NOW);
    if (!libvpu)
    {
        libvpu = dlopen("/mnt/system/lib/libvpu.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libvpu)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    CVI_VI_SetDevAttr = (PFN_CVI_VI_SetDevAttr)dlsym(libvpu, "CVI_VI_SetDevAttr");
    CVI_VI_EnableDev = (PFN_CVI_VI_EnableDev)dlsym(libvpu, "CVI_VI_EnableDev");
    CVI_VI_DisableDev = (PFN_CVI_VI_DisableDev)dlsym(libvpu, "CVI_VI_DisableDev");
    CVI_VI_CreatePipe = (PFN_CVI_VI_CreatePipe)dlsym(libvpu, "CVI_VI_CreatePipe");
    CVI_VI_DestroyPipe = (PFN_CVI_VI_DestroyPipe)dlsym(libvpu, "CVI_VI_DestroyPipe");
    CVI_VI_SetPipeAttr = (PFN_CVI_VI_SetPipeAttr)dlsym(libvpu, "CVI_VI_SetPipeAttr");
    CVI_VI_StartPipe = (PFN_CVI_VI_StartPipe)dlsym(libvpu, "CVI_VI_StartPipe");
    CVI_VI_StopPipe = (PFN_CVI_VI_StopPipe)dlsym(libvpu, "CVI_VI_StopPipe");
    CVI_VI_AttachVbPool = (PFN_CVI_VI_AttachVbPool)dlsym(libvpu, "CVI_VI_AttachVbPool");
    CVI_VI_DetachVbPool = (PFN_CVI_VI_DetachVbPool)dlsym(libvpu, "CVI_VI_DetachVbPool");
    CVI_VI_SetChnAttr = (PFN_CVI_VI_SetChnAttr)dlsym(libvpu, "CVI_VI_SetChnAttr");
    CVI_VI_EnableChn = (PFN_CVI_VI_EnableChn)dlsym(libvpu, "CVI_VI_EnableChn");
    CVI_VI_DisableChn = (PFN_CVI_VI_DisableChn)dlsym(libvpu, "CVI_VI_DisableChn");
    CVI_VI_SetChnCrop = (PFN_CVI_VI_SetChnCrop)dlsym(libvpu, "CVI_VI_SetChnCrop");
    CVI_VI_GetChnFrame = (PFN_CVI_VI_GetChnFrame)dlsym(libvpu, "CVI_VI_GetChnFrame");
    CVI_VI_ReleaseChnFrame = (PFN_CVI_VI_ReleaseChnFrame)dlsym(libvpu, "CVI_VI_ReleaseChnFrame");

    CVI_VPSS_AttachVbPool = (PFN_CVI_VPSS_AttachVbPool)dlsym(libvpu, "CVI_VPSS_AttachVbPool");
    CVI_VPSS_CreateGrp = (PFN_CVI_VPSS_CreateGrp)dlsym(libvpu, "CVI_VPSS_CreateGrp");
    CVI_VPSS_DestroyGrp = (PFN_CVI_VPSS_DestroyGrp)dlsym(libvpu, "CVI_VPSS_DestroyGrp");
    CVI_VPSS_DetachVbPool = (PFN_CVI_VPSS_DetachVbPool)dlsym(libvpu, "CVI_VPSS_DetachVbPool");
    CVI_VPSS_DisableChn = (PFN_CVI_VPSS_DisableChn)dlsym(libvpu, "CVI_VPSS_DisableChn");
    CVI_VPSS_EnableChn = (PFN_CVI_VPSS_EnableChn)dlsym(libvpu, "CVI_VPSS_EnableChn");
    CVI_VPSS_GetChnAttr = (PFN_CVI_VPSS_GetChnAttr)dlsym(libvpu, "CVI_VPSS_GetChnAttr");
    CVI_VPSS_GetChnFrame = (PFN_CVI_VPSS_GetChnFrame)dlsym(libvpu, "CVI_VPSS_GetChnFrame");
    CVI_VPSS_ReleaseChnFrame = (PFN_CVI_VPSS_ReleaseChnFrame)dlsym(libvpu, "CVI_VPSS_ReleaseChnFrame");
    CVI_VPSS_ResetGrp = (PFN_CVI_VPSS_ResetGrp)dlsym(libvpu, "CVI_VPSS_ResetGrp");
    CVI_VPSS_SendFrame = (PFN_CVI_VPSS_SendFrame)dlsym(libvpu, "CVI_VPSS_SendFrame");
    CVI_VPSS_SetChnAttr = (PFN_CVI_VPSS_SetChnAttr)dlsym(libvpu, "CVI_VPSS_SetChnAttr");
    // CVI_VPSS_SetChnRotation = (PFN_CVI_VPSS_SetChnRotation)dlsym(libvpu, "CVI_VPSS_SetChnRotation");
    CVI_VPSS_SetChnCrop = (PFN_CVI_VPSS_SetChnCrop)dlsym(libvpu, "CVI_VPSS_SetChnCrop");
    CVI_VPSS_StartGrp = (PFN_CVI_VPSS_StartGrp)dlsym(libvpu, "CVI_VPSS_StartGrp");
    CVI_VPSS_StopGrp = (PFN_CVI_VPSS_StopGrp)dlsym(libvpu, "CVI_VPSS_StopGrp");

    return 0;

OUT:
    unload_vpu_library();

    return -1;
}

class vpu_library_loader
{
public:
    bool ready;

    vpu_library_loader()
    {
        ready = (load_vpu_library() == 0);
    }

    ~vpu_library_loader()
    {
        unload_vpu_library();
    }
};

static vpu_library_loader vpu;

extern "C"
{

#define MIPI_DEMUX_NUM    4
#define MIPI_LANE_NUM    4
#define WDR_VC_NUM    2
#define SYNC_CODE_NUM    4
#define BT_DEMUX_NUM    4

enum input_mode_e {
    INPUT_MODE_MIPI = 0,
    INPUT_MODE_SUBLVDS,
    INPUT_MODE_HISPI,
    INPUT_MODE_CMOS,
    INPUT_MODE_BT1120,
    INPUT_MODE_BT601,
    INPUT_MODE_BT656_9B,
    INPUT_MODE_CUSTOM_0,
    INPUT_MODE_BT_DEMUX,
    INPUT_MODE_BUTT
};

enum rx_mac_clk_e {
    RX_MAC_CLK_200M = 0,
    RX_MAC_CLK_300M,
    RX_MAC_CLK_400M,
    RX_MAC_CLK_500M,
    RX_MAC_CLK_600M,
    RX_MAC_CLK_BUTT,
};

enum cam_pll_freq_e {
    CAMPLL_FREQ_NONE = 0,
    CAMPLL_FREQ_37P125M,
    CAMPLL_FREQ_25M,
    CAMPLL_FREQ_27M,
    CAMPLL_FREQ_24M,
    CAMPLL_FREQ_26M,
    CAMPLL_FREQ_NUM
};

struct mclk_pll_s {
    unsigned int        cam;
    enum cam_pll_freq_e    freq;
};

enum raw_data_type_e {
    RAW_DATA_8BIT = 0,
    RAW_DATA_10BIT,
    RAW_DATA_12BIT,
    YUV422_8BIT,    /* MIPI-CSI only */
    YUV422_10BIT,   /* MIPI-CSI only*/
    RAW_DATA_BUTT
};

enum mipi_wdr_mode_e {
    CVI_MIPI_WDR_MODE_NONE = 0,
    CVI_MIPI_WDR_MODE_VC,
    CVI_MIPI_WDR_MODE_DT,
    CVI_MIPI_WDR_MODE_DOL,
    CVI_MIPI_WDR_MODE_MANUAL,  /* SOI case */
    CVI_MIPI_WDR_MODE_BUTT
};

struct dphy_s {
    unsigned char        enable;
    unsigned char        hs_settle;
};

struct mipi_demux_info_s {
    unsigned int            demux_en;
    unsigned char            vc_mapping[MIPI_DEMUX_NUM];
};

struct mipi_dev_attr_s {
    enum raw_data_type_e        raw_data_type;
    short                lane_id[MIPI_LANE_NUM+1];
    enum mipi_wdr_mode_e        wdr_mode;
    short                data_type[WDR_VC_NUM];
    char                pn_swap[MIPI_LANE_NUM+1];
    struct dphy_s            dphy;
    struct mipi_demux_info_s    demux;
};

enum wdr_mode_e {
    CVI_WDR_MODE_NONE = 0,
    CVI_WDR_MODE_2F,
    CVI_WDR_MODE_3F,
    CVI_WDR_MODE_DOL_2F,
    CVI_WDR_MODE_DOL_3F,
    CVI_WDR_MODE_DOL_BUTT
};

enum lvds_sync_mode_e {
    LVDS_SYNC_MODE_SOF = 0,
    LVDS_SYNC_MODE_SAV,
    LVDS_SYNC_MODE_BUTT
};

enum lvds_bit_endian {
    LVDS_ENDIAN_LITTLE = 0,
    LVDS_ENDIAN_BIG,
    LVDS_ENDIAN_BUTT
};

enum lvds_vsync_type_e {
    LVDS_VSYNC_NORMAL = 0,
    LVDS_VSYNC_SHARE,
    LVDS_VSYNC_HCONNECT,
    LVDS_VSYNC_BUTT
};

struct lvds_vsync_type_s {
    enum lvds_vsync_type_e    sync_type;
    unsigned short            hblank1;
    unsigned short            hblank2;
};

enum lvds_fid_type_e {
    LVDS_FID_NONE = 0,
    LVDS_FID_IN_SAV,
    LVDS_FID_BUTT
};

struct lvds_fid_type_s {
    enum lvds_fid_type_e        fid;
};

struct lvds_dev_attr_s {
    enum wdr_mode_e            wdr_mode;
    enum lvds_sync_mode_e        sync_mode;
    enum raw_data_type_e        raw_data_type;
    enum lvds_bit_endian        data_endian;
    enum lvds_bit_endian        sync_code_endian;
    short                lane_id[MIPI_LANE_NUM+1];
    short        sync_code[MIPI_LANE_NUM][WDR_VC_NUM+1][SYNC_CODE_NUM];
/*
 * sublvds:
 * sync_code[x][0][0] sync_code[x][0][1] sync_code[x][0][2] sync_code[x][0][3]
 *    n0_lef_sav       n0_lef_eav          n1_lef_sav     n1_lef_eav
 * sync_code[x][1][0] sync_code[x][1][1] sync_code[x][1][2] sync_code[x][1][3]
 *    n0_sef_sav       n0_sef_eav          n1_sef_sav     n1_sef_eav
 * sync_code[x][2][0] sync_code[x][2][1] sync_code[x][2][2] sync_code[x][2][3]
 *    n0_lsef_sav       n0_lsef_eav          n1_lsef_sav     n1_lsef_eav
 *
 * hispi:
 * sync_code[x][0][0] sync_code[x][0][1] sync_code[x][0][2] sync_code[x][0][3]
 *    t1_sol           tl_eol          t1_sof         t1_eof
 * sync_code[x][1][0] sync_code[x][1][1] sync_code[x][1][2] sync_code[x][1][3]
 *    t2_sol           t2_eol          t2_sof         t2_eof
 */
    struct lvds_vsync_type_s    vsync_type;
    struct lvds_fid_type_s        fid_type;
    char                pn_swap[MIPI_LANE_NUM+1];
};

enum ttl_pin_func_e {
    TTL_PIN_FUNC_VS,
    TTL_PIN_FUNC_HS,
    TTL_PIN_FUNC_VDE,
    TTL_PIN_FUNC_HDE,
    TTL_PIN_FUNC_D0,
    TTL_PIN_FUNC_D1,
    TTL_PIN_FUNC_D2,
    TTL_PIN_FUNC_D3,
    TTL_PIN_FUNC_D4,
    TTL_PIN_FUNC_D5,
    TTL_PIN_FUNC_D6,
    TTL_PIN_FUNC_D7,
    TTL_PIN_FUNC_D8,
    TTL_PIN_FUNC_D9,
    TTL_PIN_FUNC_D10,
    TTL_PIN_FUNC_D11,
    TTL_PIN_FUNC_D12,
    TTL_PIN_FUNC_D13,
    TTL_PIN_FUNC_D14,
    TTL_PIN_FUNC_D15,
    TTL_PIN_FUNC_NUM,
};

enum ttl_src_e {
    TTL_VI_SRC_VI0 = 0,
    TTL_VI_SRC_VI1,
    TTL_VI_SRC_VI2,        /* BT demux */
    TTL_VI_SRC_NUM
};

enum ttl_fmt_e {
    TTL_SYNC_PAT = 0,
    TTL_VHS_11B,
    TTL_VHS_19B,
    TTL_VDE_11B,
    TTL_VDE_19B,
    TTL_VSDE_11B,
    TTL_VSDE_19B,
};

struct ttl_dev_attr_s {
    enum ttl_src_e            vi;
    enum ttl_fmt_e            ttl_fmt;
    enum raw_data_type_e        raw_data_type;
    signed char            func[TTL_PIN_FUNC_NUM];
    unsigned short            v_bp;
    unsigned short            h_bp;
};

enum bt_demux_mode_e {
    BT_DEMUX_DISABLE = 0,
    BT_DEMUX_2,
    BT_DEMUX_3,
    BT_DEMUX_4,
};

struct bt_demux_sync_s {
    unsigned char        sav_vld;
    unsigned char        sav_blk;
    unsigned char        eav_vld;
    unsigned char        eav_blk;
};

struct bt_demux_attr_s {
    signed char            func[TTL_PIN_FUNC_NUM];
    unsigned short            v_fp;
    unsigned short            h_fp;
    unsigned short            v_bp;
    unsigned short            h_bp;
    enum bt_demux_mode_e        mode;
    unsigned char            sync_code_part_A[3];    /* sync code 0~2 */
    struct bt_demux_sync_s        sync_code_part_B[BT_DEMUX_NUM];    /* sync code 3 */
    char                yc_exchg;
};

struct img_size_s {
    unsigned int    width;
    unsigned int    height;
};

struct manual_wdr_attr_s {
    unsigned int            manual_en;
    unsigned short            l2s_distance;
    unsigned short            lsef_length;
    unsigned int            discard_padding_lines;
    unsigned int            update;
};

struct combo_dev_attr_s {
    enum input_mode_e        input_mode;
    enum rx_mac_clk_e        mac_clk;
    struct mclk_pll_s        mclk;
    union {
        struct mipi_dev_attr_s    mipi_attr;
        struct lvds_dev_attr_s    lvds_attr;
        struct ttl_dev_attr_s    ttl_attr;
        struct bt_demux_attr_s    bt_demux_attr;
    };
    unsigned int            devno;
    struct img_size_s        img_size;
    struct manual_wdr_attr_s    wdr_manu;
};

typedef struct combo_dev_attr_s SNS_COMBO_DEV_ATTR_S;

#define ALG_LIB_NAME_SIZE_MAX (20)
typedef struct _ALG_LIB_S {
    CVI_S32 s32Id;
    CVI_CHAR acLibName[ALG_LIB_NAME_SIZE_MAX];
} ALG_LIB_S;

typedef union _ISP_SNS_COMMBUS_U {
    CVI_S8 s8I2cDev;
    struct {
        CVI_S8 bit4SspDev : 4;
        CVI_S8 bit4SspCs : 4;
    } s8SspDev;
} ISP_SNS_COMMBUS_U;

typedef enum _ISP_SNS_MIRRORFLIP_TYPE_E {
    ISP_SNS_NORMAL      = 0,
    ISP_SNS_MIRROR      = 1,
    ISP_SNS_FLIP        = 2,
    ISP_SNS_MIRROR_FLIP = 3,
    ISP_SNS_BUTT
} ISP_SNS_MIRRORFLIP_TYPE_E;

typedef enum _ISP_SNS_GAIN_MODE_E {
    SNS_GAIN_MODE_SHARE = 0,    /* gain setting for all wdr frames*/
    SNS_GAIN_MODE_WDR_2F,        /* separate gain for 2-frame wdr mode*/
    SNS_GAIN_MODE_WDR_3F,        /* separate gain for 3-frame wdr mode*/
    SNS_GAIN_MODE_ONLY_LEF        /* gain setting only apply to lef and sef is fixed to 1x */
} ISP_SNS_GAIN_MODE_E;

typedef enum _ISP_SNS_L2S_MODE_E {
    SNS_L2S_MODE_AUTO = 0,    /* sensor l2s distance varies by the inttime of sef. */
    SNS_L2S_MODE_FIX,    /* sensor l2s distance is fixed. */
} ISP_SNS_INTTIME_MODE_E;

typedef enum _SNS_BDG_MUX_MODE_E {
    SNS_BDG_MUX_NONE = 0,    /* sensor bridge mux is disabled */
    SNS_BDG_MUX_2,        /* sensor bridge mux 2 input */
    SNS_BDG_MUX_3,        /* sensor bridge mux 3 input */
    SNS_BDG_MUX_4,        /* sensor bridge mux 4 input */
} SNS_BDG_MUX_MODE_E;

typedef struct _ISP_INIT_ATTR_S {
    CVI_U32 u32ExpTime;
    CVI_U32 u32AGain;
    CVI_U32 u32DGain;
    CVI_U32 u32ISPDGain;
    CVI_U32 u32Exposure;
    CVI_U32 u32LinesPer500ms;
    CVI_U32 u32PirisFNO;
    CVI_U16 u16WBRgain;
    CVI_U16 u16WBGgain;
    CVI_U16 u16WBBgain;
    CVI_U16 u16SampleRgain;
    CVI_U16 u16SampleBgain;
    CVI_U16 u16UseHwSync;
    ISP_SNS_GAIN_MODE_E enGainMode;
    ISP_SNS_INTTIME_MODE_E enL2SMode;
    SNS_BDG_MUX_MODE_E enSnsBdgMuxMode;
} ISP_INIT_ATTR_S;

typedef struct _MCLK_ATTR_S {
    CVI_U8 u8Mclk;
    CVI_BOOL bMclkEn;
} MCLK_ATTR_S;

typedef struct _RX_INIT_ATTR_S {
    CVI_U32 MipiDev;
    CVI_S16 as16LaneId[5];
    CVI_S8  as8PNSwap[5];
    MCLK_ATTR_S stMclkAttr;
} RX_INIT_ATTR_S;

typedef struct _ISP_CMOS_SENSOR_IMAGE_MODE_S {
    CVI_U16 u16Width;
    CVI_U16 u16Height;
    CVI_FLOAT f32Fps;
    CVI_U8 u8SnsMode;
} ISP_CMOS_SENSOR_IMAGE_MODE_S;

#define NOISE_PROFILE_ISO_NUM 16
#define NOISE_PROFILE_CHANNEL_NUM 4
#define NOISE_PROFILE_LEVEL_NUM 2
typedef struct cviISP_CMOS_NOISE_CALIBRATION_S {
    CVI_FLOAT CalibrationCoef[NOISE_PROFILE_ISO_NUM][NOISE_PROFILE_CHANNEL_NUM][NOISE_PROFILE_LEVEL_NUM];
} ISP_CMOS_NOISE_CALIBRATION_S;

typedef struct _ISP_CMOS_DEFAULT_S {
    ISP_CMOS_NOISE_CALIBRATION_S stNoiseCalibration;
} ISP_CMOS_DEFAULT_S;

typedef enum _ISP_OP_TYPE_E {
    OP_TYPE_AUTO,
    OP_TYPE_MANUAL,
    OP_TYPE_BUTT
} ISP_OP_TYPE_E;

typedef struct _ISP_BLACK_LEVEL_MANUAL_ATTR_S {
    CVI_U16 OffsetR; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetGr; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetGb; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetB; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetR2; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetGr2; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetGb2; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetB2; /*RW; Range:[0x0, 0xfff]*/
} ISP_BLACK_LEVEL_MANUAL_ATTR_S;

#define ISP_AUTO_ISO_STRENGTH_NUM (16)
typedef struct _ISP_BLACK_LEVEL_AUTO_ATTR_S {
    CVI_U16 OffsetR[ISP_AUTO_ISO_STRENGTH_NUM]; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetGr[ISP_AUTO_ISO_STRENGTH_NUM]; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetGb[ISP_AUTO_ISO_STRENGTH_NUM]; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetB[ISP_AUTO_ISO_STRENGTH_NUM]; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetR2[ISP_AUTO_ISO_STRENGTH_NUM]; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetGr2[ISP_AUTO_ISO_STRENGTH_NUM]; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetGb2[ISP_AUTO_ISO_STRENGTH_NUM]; /*RW; Range:[0x0, 0xfff]*/
    CVI_U16 OffsetB2[ISP_AUTO_ISO_STRENGTH_NUM]; /*RW; Range:[0x0, 0xfff]*/
} ISP_BLACK_LEVEL_AUTO_ATTR_S;

typedef struct _ISP_BLACK_LEVEL_ATTR_S {
    CVI_BOOL Enable; /*RW; Range:[0x0, 0x1]*/
    ISP_OP_TYPE_E enOpType;
    CVI_U8 UpdateInterval; /*RW; Range:[0x1, 0xFF]*/
    ISP_BLACK_LEVEL_MANUAL_ATTR_S stManual;
    ISP_BLACK_LEVEL_AUTO_ATTR_S stAuto;
} ISP_BLACK_LEVEL_ATTR_S;

typedef struct _ISP_CMOS_BLACK_LEVEL_S {
    CVI_BOOL bUpdate;
    ISP_BLACK_LEVEL_ATTR_S blcAttr;
} ISP_CMOS_BLACK_LEVEL_S;

typedef enum _ISP_SNS_TYPE_E {
    SNS_I2C_TYPE,
    SNS_SSP_TYPE,
    SNS_TYPE_BUTT,
} ISP_SNS_TYPE_E;

typedef struct _ISP_I2C_DATA_S {
    CVI_BOOL bUpdate;
    CVI_BOOL bDropFrm;
    CVI_BOOL bvblankUpdate;
    CVI_U8 u8DelayFrmNum; /*RW; Number of delayed frames for the sensor register*/
    CVI_U8 u8DropFrmNum; /*RW; Number of frame to drop*/
    CVI_U8 u8IntPos; /*RW;Position where the configuration of the sensor register takes effect */
    CVI_U8 u8DevAddr; /*RW;Sensor device address*/
    CVI_U32 u32RegAddr; /*RW;Sensor register address*/
    CVI_U32 u32AddrByteNum; /*RW;Bit width of the sensor register address*/
    CVI_U32 u32Data; /*RW;Sensor register data*/
    CVI_U32 u32DataByteNum; /*RW;Bit width of sensor register data*/
} ISP_I2C_DATA_S;

typedef struct _ISP_SSP_DATA_S {
    CVI_BOOL bUpdate;
    CVI_U8 u8DelayFrmNum; /*RW; Number of delayed frames for the sensor register*/
    CVI_U8 u8IntPos; /*RW;Position where the configuration of the sensor register takes effect */
    CVI_U32 u32DevAddr; /*RW;Sensor device address*/
    CVI_U32 u32DevAddrByteNum; /*RW;Bit width of the sensor device address*/
    CVI_U32 u32RegAddr; /*RW;Sensor register address*/
    CVI_U32 u32RegAddrByteNum; /*RW;Bit width of the sensor register address*/
    CVI_U32 u32Data; /*RW;Sensor register data*/
    CVI_U32 u32DataByteNum; /*RW;Bit width of sensor register data*/
} ISP_SSP_DATA_S;

#define ISP_MAX_SNS_REGS 32
typedef struct _ISP_SNS_REGS_INFO_S {
    ISP_SNS_TYPE_E enSnsType;
    CVI_U32 u32RegNum;
    CVI_U8 u8Cfg2ValidDelayMax;
    ISP_SNS_COMMBUS_U unComBus;
    union {
        ISP_I2C_DATA_S astI2cData[ISP_MAX_SNS_REGS];
        ISP_SSP_DATA_S astSspData[ISP_MAX_SNS_REGS];
    };

    struct {
        CVI_BOOL bUpdate;
        CVI_U8 u8DelayFrmNum;
        CVI_U32 u32SlaveVsTime; /* RW;time of vsync. Unit: inck clock cycle */
        CVI_U32 u32SlaveBindDev;
    } stSlvSync;

    CVI_BOOL bConfig;
    CVI_U8 use_snsr_sram;
    CVI_U8 need_update;
} ISP_SNS_REGS_INFO_S;

typedef struct _ISP_WDR_SIZE_S {
    RECT_S stWndRect;
    SIZE_S stSnsSize;
    SIZE_S stMaxSize;
} ISP_WDR_SIZE_S;

#define ISP_MAX_WDR_FRAME_NUM 2
typedef struct _ISP_SNS_ISP_INFO_S {
    CVI_U32 frm_num;
    ISP_WDR_SIZE_S img_size[ISP_MAX_WDR_FRAME_NUM];
    CVI_U8 u8DelayFrmNum; /*RW; Number of delayed frames for the isp setting */
    CVI_U8 need_update;
} ISP_SNS_ISP_INFO_S;

typedef struct _ISP_MANUAL_WDR_ATTR_S {
    CVI_S32 devno;
    CVI_S32 manual_en;
    CVI_S16 l2s_distance;
    CVI_S16 lsef_length;
    CVI_S32 discard_padding_lines;
    CVI_S32 update;
} ISP_MANUAL_WDR_ATTR_S;

typedef struct _ISP_SNS_CIF_INFO_S {
    ISP_MANUAL_WDR_ATTR_S wdr_manual;
    CVI_U8 u8DelayFrmNum; /*RW; Number of delayed frames for the cif setting */
    CVI_U8 need_update;
} ISP_SNS_CIF_INFO_S;

typedef struct _ISP_SNS_SYNC_INFO_S {
    ISP_SNS_REGS_INFO_S snsCfg;
    ISP_SNS_ISP_INFO_S ispCfg;
    ISP_SNS_CIF_INFO_S cifCfg;
} ISP_SNS_SYNC_INFO_S;

typedef struct _ISP_SENSOR_EXP_FUNC_S {
    CVI_VOID (*pfn_cmos_sensor_init)(VI_PIPE ViPipe);
    CVI_VOID (*pfn_cmos_sensor_exit)(VI_PIPE ViPipe);
    CVI_VOID (*pfn_cmos_sensor_global_init)(VI_PIPE ViPipe);
    CVI_S32 (*pfn_cmos_set_image_mode)(VI_PIPE ViPipe, ISP_CMOS_SENSOR_IMAGE_MODE_S *pstSensorImageMode);
    CVI_S32 (*pfn_cmos_set_wdr_mode)(VI_PIPE ViPipe, CVI_U8 u8Mode);

    /* the algs get data which is associated with sensor, except 3a */
    CVI_S32 (*pfn_cmos_get_isp_default)(VI_PIPE ViPipe, ISP_CMOS_DEFAULT_S *pstDef);
    CVI_S32 (*pfn_cmos_get_isp_black_level)(VI_PIPE ViPipe, ISP_CMOS_BLACK_LEVEL_S *pstBlackLevel);
    CVI_S32 (*pfn_cmos_get_sns_reg_info)(VI_PIPE ViPipe, ISP_SNS_SYNC_INFO_S *pstSnsRegsInfo);

    /* the function of sensor set pixel detect */
    //CVI_VOID (*pfn_cmos_set_pixel_detect)(VI_PIPE ViPipe, bool bEnable);
} ISP_SENSOR_EXP_FUNC_S;

typedef enum _AE_ACCURACY_E {
    AE_ACCURACY_DB = 0,
    AE_ACCURACY_LINEAR,
    AE_ACCURACY_TABLE,

    AE_ACCURACY_BUTT,
} AE_ACCURACY_E;

typedef struct _AE_ACCURACY_S {
    AE_ACCURACY_E enAccuType;
    float f32Accuracy;
    float f32Offset;
} AE_ACCURACY_S;

typedef enum _ISP_AE_STRATEGY_E {
    AE_EXP_HIGHLIGHT_PRIOR = 0,
    AE_EXP_LOWLIGHT_PRIOR = 1,
    AE_STRATEGY_MODE_BUTT
} ISP_AE_STRATEGY_E;

typedef enum _AE_BLC_TYPE_E {
    AE_BLC_TYPE_LINEAR = 0,
    AE_BLC_TYPE_LADDER,

    AE_BLC_TYPE_BUTT,
} AE_BLC_TYPE_E;

#define HIST_THRESH_NUM (4)
typedef struct _AE_SENSOR_DEFAULT_S {
    CVI_U8 au8HistThresh[HIST_THRESH_NUM];
    CVI_U8 u8AeCompensation;

    CVI_U32 u32LinesPer500ms;
    CVI_U32 u32FlickerFreq;
    CVI_U32 u32HmaxTimes; /* unit is ns */
    CVI_U32 u32InitExposure;
    CVI_U32 u32InitAESpeed;
    CVI_U32 u32InitAETolerance;

    CVI_U32 u32FullLinesStd;
    CVI_U32 u32FullLinesMax;
    CVI_U32 u32FullLines;
    CVI_U32 u32MaxIntTime; /* RW;unit is line */
    CVI_U32 u32MinIntTime;
    CVI_U32 u32MaxIntTimeTarget;
    CVI_U32 u32MinIntTimeTarget;
    AE_ACCURACY_S stIntTimeAccu;

    CVI_U32 u32MaxAgain;
    CVI_U32 u32MinAgain;
    CVI_U32 u32MaxAgainTarget;
    CVI_U32 u32MinAgainTarget;
    AE_ACCURACY_S stAgainAccu;

    CVI_U32 u32MaxDgain;
    CVI_U32 u32MinDgain;
    CVI_U32 u32MaxDgainTarget;
    CVI_U32 u32MinDgainTarget;
    AE_ACCURACY_S stDgainAccu;

    CVI_U32 u32MaxISPDgainTarget;
    CVI_U32 u32MinISPDgainTarget;
    CVI_U32 u32ISPDgainShift;

    CVI_U32 u32MaxIntTimeStep;
    CVI_U32 u32LFMaxShortTime;
    CVI_U32 u32LFMinExposure;
#if 0
    ISP_AE_ROUTE_S stAERouteAttr;
    CVI_BOOL bAERouteExValid;
    ISP_AE_ROUTE_EX_S stAERouteAttrEx;

    CVI_U16 u16ManRatioEnable;
    CVI_U32 au32Ratio[EXP_RATIO_NUM];

    ISP_IRIS_TYPE_E  enIrisType;
    ISP_PIRIS_ATTR_S stPirisAttr;
    ISP_IRIS_F_NO_E  enMaxIrisFNO;
    ISP_IRIS_F_NO_E  enMinIrisFNO;
#endif
    ISP_AE_STRATEGY_E enAeExpMode;

    CVI_U16 u16ISOCalCoef;
    CVI_U8 u8AERunInterval;
    CVI_FLOAT f32Fps;
    CVI_FLOAT f32MinFps;
    CVI_U32 denom;
    CVI_U32 u32AEResponseFrame;
    CVI_U32 u32SnsStableFrame;    /* delay for stable statistic after sensor init. (unit: frame) */
    AE_BLC_TYPE_E enBlcType;
    ISP_SNS_GAIN_MODE_E    enWDRGainMode;
} AE_SENSOR_DEFAULT_S;

typedef enum _ISP_FSWDR_MODE_E {
    ISP_FSWDR_NORMAL_MODE = 0x0,
    ISP_FSWDR_LONG_FRAME_MODE = 0x1,
    ISP_FSWDR_AUTO_LONG_FRAME_MODE = 0x2,
    ISP_FSWDR_MODE_BUTT
} ISP_FSWDR_MODE_E;

typedef struct _AE_FSWDR_ATTR_S {
    ISP_FSWDR_MODE_E enFSWDRMode;
} AE_FSWDR_ATTR_S;

typedef struct _AE_SENSOR_EXP_FUNC_S {
    CVI_S32 (*pfn_cmos_get_ae_default)(VI_PIPE ViPipe, AE_SENSOR_DEFAULT_S *pstAeSnsDft);

    /* the function of sensor set fps */
    CVI_S32 (*pfn_cmos_fps_set)(VI_PIPE ViPipe, CVI_FLOAT f32Fps, AE_SENSOR_DEFAULT_S *pstAeSnsDft);
    CVI_S32 (*pfn_cmos_slow_framerate_set)(VI_PIPE ViPipe, CVI_U32 u32FullLines, AE_SENSOR_DEFAULT_S *pstAeSnsDft);

    /* while isp notify ae to update sensor regs, ae call these funcs. */
    CVI_S32 (*pfn_cmos_inttime_update)(VI_PIPE ViPipe, CVI_U32 *u32IntTime);
    CVI_S32 (*pfn_cmos_gains_update)(VI_PIPE ViPipe, CVI_U32 *u32Again, CVI_U32 *u32Dgain);

    CVI_S32 (*pfn_cmos_again_calc_table)(VI_PIPE ViPipe, CVI_U32 *pu32AgainLin, CVI_U32 *pu32AgainDb);
    CVI_S32 (*pfn_cmos_dgain_calc_table)(VI_PIPE ViPipe, CVI_U32 *pu32DgainLin, CVI_U32 *pu32DgainDb);

    CVI_S32 (*pfn_cmos_get_inttime_max)
    (VI_PIPE ViPipe, CVI_U16 u16ManRatioEnable, CVI_U32 *au32Ratio, CVI_U32 *au32IntTimeMax,
     CVI_U32 *au32IntTimeMin, CVI_U32 *pu32LFMaxIntTime);

    /* long frame mode set */
    CVI_S32 (*pfn_cmos_ae_fswdr_attr_set)(VI_PIPE ViPipe, AE_FSWDR_ATTR_S *pstAeFSWDRAttr);
} AE_SENSOR_EXP_FUNC_S;

typedef struct _ISP_SNS_OBJ_S {
    CVI_S32 (*pfnRegisterCallback)(VI_PIPE ViPipe, ALG_LIB_S *, ALG_LIB_S *);
    CVI_S32 (*pfnUnRegisterCallback)(VI_PIPE ViPipe, ALG_LIB_S *, ALG_LIB_S *);
    CVI_S32 (*pfnSetBusInfo)(VI_PIPE ViPipe, ISP_SNS_COMMBUS_U unSNSBusInfo);
    CVI_VOID (*pfnStandby)(VI_PIPE ViPipe);
    CVI_VOID (*pfnRestart)(VI_PIPE ViPipe);
    CVI_VOID (*pfnMirrorFlip)(VI_PIPE ViPipe, ISP_SNS_MIRRORFLIP_TYPE_E eSnsMirrorFlip);
    CVI_S32 (*pfnWriteReg)(VI_PIPE ViPipe, CVI_S32 s32Addr, CVI_S32 s32Data);
    CVI_S32 (*pfnReadReg)(VI_PIPE ViPipe, CVI_S32 s32Addr);
    CVI_S32 (*pfnSetInit)(VI_PIPE ViPipe, ISP_INIT_ATTR_S *);
    CVI_S32 (*pfnPatchRxAttr)(RX_INIT_ATTR_S *);
    CVI_VOID (*pfnPatchI2cAddr)(CVI_S32 s32I2cAddr);
    CVI_S32 (*pfnGetRxAttr)(VI_PIPE ViPipe, SNS_COMBO_DEV_ATTR_S *);
    CVI_S32 (*pfnExpSensorCb)(ISP_SENSOR_EXP_FUNC_S *);
    CVI_S32 (*pfnExpAeCb)(AE_SENSOR_EXP_FUNC_S *);
    CVI_S32 (*pfnSnsProbe)(VI_PIPE ViPipe);
} ISP_SNS_OBJ_S;

}

static void* libsns_gc2083_sys = 0;
static void* libsns_gc2083_ae = 0;
static void* libsns_gc2083_awb = 0;
static void* libsns_gc2083_isp = 0;
static void* libsns_gc2083 = 0;

static ISP_SNS_OBJ_S* pstSnsObj = 0;

static int unload_sns_gc2083_library()
{
    if (libsns_gc2083_sys)
    {
        dlclose(libsns_gc2083_sys);
        libsns_gc2083_sys = 0;
    }

    if (libsns_gc2083_ae)
    {
        dlclose(libsns_gc2083_ae);
        libsns_gc2083_ae = 0;
    }

    if (libsns_gc2083_awb)
    {
        dlclose(libsns_gc2083_awb);
        libsns_gc2083_awb = 0;
    }

    if (libsns_gc2083_isp)
    {
        dlclose(libsns_gc2083_isp);
        libsns_gc2083_isp = 0;
    }

    if (libsns_gc2083)
    {
        dlclose(libsns_gc2083);
        libsns_gc2083 = 0;
    }

    pstSnsObj = 0;

    return 0;
}

static int load_sns_gc2083_library()
{
    if (libsns_gc2083)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        fprintf(stderr, "this device is not whitelisted for capture cvi\n");
        return -1;
    }

    libsns_gc2083_sys = dlopen("libsys.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libsns_gc2083_sys)
    {
        libsns_gc2083_sys = dlopen("/mnt/system/lib/libsys.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libsns_gc2083_sys)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libsns_gc2083_ae = dlopen("libae.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libsns_gc2083_ae)
    {
        libsns_gc2083_ae = dlopen("/mnt/system/lib/libae.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libsns_gc2083_ae)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libsns_gc2083_awb = dlopen("libawb.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libsns_gc2083_awb)
    {
        libsns_gc2083_awb = dlopen("/mnt/system/lib/libawb.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libsns_gc2083_awb)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libsns_gc2083_isp = dlopen("libisp.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libsns_gc2083_isp)
    {
        libsns_gc2083_isp = dlopen("/mnt/system/lib/libisp.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libsns_gc2083_isp)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libsns_gc2083 = dlopen("libsns_gc2083.so", RTLD_LOCAL | RTLD_NOW);
    if (!libsns_gc2083)
    {
        libsns_gc2083 = dlopen("/mnt/system/lib/libsns_gc2083.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libsns_gc2083)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    pstSnsObj = (ISP_SNS_OBJ_S*)dlsym(libsns_gc2083, "stSnsGc2083_Obj");

    return 0;

OUT:
    unload_sns_gc2083_library();

    return -1;
}

class sns_gc2083_library_loader
{
public:
    bool ready;

    sns_gc2083_library_loader()
    {
        ready = (load_sns_gc2083_library() == 0);
    }

    ~sns_gc2083_library_loader()
    {
        unload_sns_gc2083_library();
    }
};

static sns_gc2083_library_loader sns_gc2083;

extern "C"
{

typedef CVI_S32 SENSOR_ID;
typedef struct _ISP_BIND_ATTR_S {
    SENSOR_ID sensorId;
    ALG_LIB_S stAeLib;
    ALG_LIB_S stAfLib;
    ALG_LIB_S stAwbLib;
} ISP_BIND_ATTR_S;

typedef enum _ISP_BAYER_FORMAT_E {
    BAYER_BGGR,
    BAYER_GBRG,
    BAYER_GRBG,
    BAYER_RGGB,
    //for RGBIR sensor
    BAYER_GRGBI = 8,
    BAYER_RGBGI,
    BAYER_GBGRI,
    BAYER_BGRGI,
    BAYER_IGRGB,
    BAYER_IRGBG,
    BAYER_IBGRG,
    BAYER_IGBGR,
    BAYER_BUTT
} ISP_BAYER_FORMAT_E;

typedef struct _ISP_PUB_ATTR_S {
    RECT_S stWndRect;
    SIZE_S stSnsSize;
    CVI_FLOAT f32FrameRate;
    ISP_BAYER_FORMAT_E enBayer;
    WDR_MODE_E enWDRMode;
    CVI_U8 u8SnsMode;
} ISP_PUB_ATTR_S;

typedef union _ISP_STATISTICS_CTRL_U {
    CVI_U64 u64Key;
    struct {
        CVI_U64 bit1FEAeGloStat : 1; /* [0] */
        CVI_U64 bit1FEAeLocStat : 1; /* [1] */
        CVI_U64 bit1AwbStat1 : 1; /* [2] Awb Stat1 means global awb data. */
        CVI_U64 bit1AwbStat2 : 1; /* [3] Awb Stat2 means local awb data. */
        CVI_U64 bit1FEAfStat : 1; /* [4] */
        CVI_U64 bit14Rsv : 59; /* [5:63] */
    };
} ISP_STATISTICS_CTRL_U;

typedef struct _ISP_AE_CROP_S {
    CVI_BOOL bEnable; /*RW; Range:[0x0,0x1]*/
    CVI_U16 u16X; /*RW; Range:[0x00,0x1FFF]*/
    CVI_U16 u16Y; /*RW; Range:[0x00,0x1FFF]*/
    CVI_U16 u16W; /*RW; Range:[0x00,0x1FFF]*/
    CVI_U16 u16H; /*RW; Range:[0x00,0x1FFF]*/
} ISP_AE_CROP_S;

typedef struct _ISP_AE_FACE_CROP_S {
    CVI_BOOL bEnable; /*RW; Range:[0x0,0x1]*/
    CVI_U16 u16X; /*RW; Range:[0x00,0x1FFF]*/
    CVI_U16 u16Y; /*RW; Range:[0x00,0x1FFF]*/
    CVI_U8 u16W; /*RW; Range:[0x00,0xFF]*/
    CVI_U8 u16H; /*RW; Range:[0x00,0xFF]*/
} ISP_AE_FACE_CROP_S;

#define AE_MAX_NUM (1)
#define FACE_WIN_NUM 4
#define AE_WEIGHT_ZONE_ROW    15
#define AE_WEIGHT_ZONE_COLUMN    17
typedef struct _ISP_AE_STATISTICS_CFG_S {
    CVI_BOOL bHisStatisticsEnable; /*RW; Range:[0x0,0x1]*/
    ISP_AE_CROP_S stCrop[AE_MAX_NUM];
    ISP_AE_FACE_CROP_S stFaceCrop[FACE_WIN_NUM];
    CVI_BOOL fast2A_ena; /*RW; Range:[0x0,0x1]*/
    CVI_U8 fast2A_ae_low; /*RW; Range:[0x0,0xFF]*/
    CVI_U8 fast2A_ae_high; /*RW; Range:[0x0,0xFF]*/
    CVI_U16 fast2A_awb_top; /*RW; Range:[0x0,0xFFF]*/
    CVI_U16 fast2A_awb_bot; /*RW; Range:[0x0,0xFFF]*/
    CVI_U16 over_exp_thr; /*RW; Range:[0x0,0x3FF]*/
    CVI_U8 au8Weight[AE_WEIGHT_ZONE_ROW][AE_WEIGHT_ZONE_COLUMN]; /*RW; Range:[0x0, 0xF]*/
} ISP_AE_STATISTICS_CFG_S;

typedef enum _ISP_AWB_SWITCH_E {
    ISP_AWB_AFTER_DG,
    ISP_AWB_AFTER_DRC,
    ISP_AWB_SWITCH_BUTT,
} ISP_AWB_SWITCH_E;

typedef struct _ISP_AWB_CROP_S {
    CVI_BOOL bEnable;
    CVI_U16 u16X; /*RW; Range:[0x0, 0x1000]*/
    CVI_U16 u16Y; /*RW; Range:[0x0, 0x1000]*/
    CVI_U16 u16W; /*RW; Range:[0x0, 0x1000]*/
    CVI_U16 u16H; /*RW; Range:[0x0, 0x1000]*/
} ISP_AWB_CROP_S;

typedef struct _ISP_WB_STATISTICS_CFG_S {
    ISP_AWB_SWITCH_E enAWBSwitch;
    CVI_U16 u16ZoneRow; /*RW; Range:[0x0, AWB_ZONE_ORIG_ROW]*/
    CVI_U16 u16ZoneCol; /*RW; Range:[0x0, AWB_ZONE_ORIG_COLUMN]*/
    CVI_U16 u16ZoneBin;
    CVI_U16 au16HistBinThresh[4];
    CVI_U16 u16WhiteLevel; /*RW; Range:[0x0, 0xFFF]*/
    CVI_U16 u16BlackLevel; /*RW; Range:[0x0, 0xFFF]*/
    CVI_U16 u16CbMax;
    CVI_U16 u16CbMin;
    CVI_U16 u16CrMax;
    CVI_U16 u16CrMin;
    ISP_AWB_CROP_S stCrop;
} ISP_WB_STATISTICS_CFG_S;

#define AF_GAMMA_NUM (256)
typedef struct _ISP_AF_RAW_CFG_S {
    CVI_BOOL PreGammaEn;
    CVI_U8 PreGammaTable[AF_GAMMA_NUM]; /*RW; Range:[0x0, 0xFF]*/
} ISP_AF_RAW_CFG_S;

typedef struct _ISP_AF_PRE_FILTER_CFG_S {
    CVI_BOOL PreFltEn;
} ISP_AF_PRE_FILTER_CFG_S;

typedef struct _ISP_AF_CROP_S {
    CVI_BOOL bEnable;
    CVI_U16 u16X; /*RW; Range:[0x8, 0xFFF]*/
    CVI_U16 u16Y; /*RW; Range:[0x2, 0xFFF]*/
    CVI_U16 u16W; /*RW; Range:[0x110, 0xFFF]*/
    CVI_U16 u16H; /*RW; Range:[0xF0, 0xFFF]*/
} ISP_AF_CROP_S;

#define FIR_H_GAIN_NUM (5)
#define FIR_V_GAIN_NUM (3)
typedef struct _ISP_AF_CFG_S {
    CVI_BOOL bEnable;
    CVI_U16 u16Hwnd; /*RW; Range:[0x2, 0x11]*/
    CVI_U16 u16Vwnd; /*RW; Range:[0x2, 0xF]*/
    CVI_U8 u8HFltShift; /*RW; Range:[0x0, 0xF]*/
    CVI_S8 s8HVFltLpCoeff[FIR_H_GAIN_NUM]; /*RW; Range:[0x0, 0x1F]*/
    ISP_AF_RAW_CFG_S stRawCfg;
    ISP_AF_PRE_FILTER_CFG_S stPreFltCfg;
    ISP_AF_CROP_S stCrop;
    CVI_U8 H0FltCoring; /*RW; Range:[0x0, 0xFF]*/
    CVI_U8 H1FltCoring; /*RW; Range:[0x0, 0xFF]*/
    CVI_U8 V0FltCoring; /*RW; Range:[0x0, 0xFF]*/
    CVI_U16 u16HighLumaTh; /*RW; Range:[0x0, 0xFF]*/
    CVI_U8 u8ThLow;
    CVI_U8 u8ThHigh;
    CVI_U8 u8GainLow; /*RW; Range:[0x0, 0xFE]*/
    CVI_U8 u8GainHigh; /*RW; Range:[0x0, 0xFE]*/
    CVI_U8 u8SlopLow; /*RW; Range:[0x0, 0xF]*/
    CVI_U8 u8SlopHigh; /*RW; Range:[0x0, 0xF]*/
} ISP_AF_CFG_S;

typedef struct _ISP_AF_H_PARAM_S {
    CVI_S8 s8HFltHpCoeff[FIR_H_GAIN_NUM]; /*RW; Range:[0x0, 0x1F]*/
} ISP_AF_H_PARAM_S;

typedef struct _ISP_AF_V_PARAM_S {
    CVI_S8 s8VFltHpCoeff[FIR_V_GAIN_NUM]; /*RW; Range:[0x0, 0x1F]*/
} ISP_AF_V_PARAM_S;

typedef struct _ISP_FOCUS_STATISTICS_CFG_S {
    ISP_AF_CFG_S stConfig;
    ISP_AF_H_PARAM_S stHParam_FIR0;
    ISP_AF_H_PARAM_S stHParam_FIR1;
    ISP_AF_V_PARAM_S stVParam_FIR;
} ISP_FOCUS_STATISTICS_CFG_S;

typedef struct _ISP_STATISTICS_CFG_S {
    ISP_STATISTICS_CTRL_U unKey;
    ISP_AE_STATISTICS_CFG_S stAECfg;
    ISP_WB_STATISTICS_CFG_S stWBCfg;
    ISP_FOCUS_STATISTICS_CFG_S stFocusCfg;
} ISP_STATISTICS_CFG_S;

#define CVI_AE_LIB_NAME "cvi_ae_lib"
#define CVI_AWB_LIB_NAME "cvi_awb_lib"

#define AWB_ZONE_ORIG_ROW (30)
#define AWB_ZONE_ORIG_COLUMN (34)
#define AF_XOFFSET_MIN (8)
#define AF_YOFFSET_MIN (2)

typedef CVI_S32 (*PFN_CVI_MIPI_SetMipiReset)(CVI_S32 devno, CVI_U32 reset);
typedef CVI_S32 (*PFN_CVI_MIPI_SetSensorClock)(CVI_S32 devno, CVI_U32 enable);
typedef CVI_S32 (*PFN_CVI_MIPI_SetSensorReset)(CVI_S32 devno, CVI_U32 reset);
typedef CVI_S32 (*PFN_CVI_MIPI_SetMipiAttr)(CVI_S32 ViPipe, const CVI_VOID *devAttr);

typedef CVI_S32 (*PFN_CVI_AE_Register)(VI_PIPE ViPipe, ALG_LIB_S *pstAeLib);
typedef CVI_S32 (*PFN_CVI_AE_UnRegister)(VI_PIPE ViPipe, ALG_LIB_S *pstAeLib);

typedef CVI_S32 (*PFN_CVI_AWB_Register)(VI_PIPE ViPipe, ALG_LIB_S *pstAwbLib);
typedef CVI_S32 (*PFN_CVI_AWB_UnRegister)(VI_PIPE ViPipe, ALG_LIB_S *pstAwbLib);

typedef CVI_S32 (*PFN_CVI_ISP_Init)(VI_PIPE ViPipe);
typedef CVI_S32 (*PFN_CVI_ISP_MemInit)(VI_PIPE ViPipe);
typedef CVI_S32 (*PFN_CVI_ISP_Run)(VI_PIPE ViPipe);
typedef CVI_S32 (*PFN_CVI_ISP_Exit)(VI_PIPE ViPipe);

typedef CVI_S32 (*PFN_CVI_ISP_SetBindAttr)(VI_PIPE ViPipe, const ISP_BIND_ATTR_S *pstBindAttr);
typedef CVI_S32 (*PFN_CVI_ISP_SetPubAttr)(VI_PIPE ViPipe, const ISP_PUB_ATTR_S *pstPubAttr);

typedef CVI_S32 (*PFN_CVI_ISP_SetStatisticsConfig)(VI_PIPE ViPipe, const ISP_STATISTICS_CFG_S *pstStatCfg);
typedef CVI_S32 (*PFN_CVI_ISP_GetStatisticsConfig)(VI_PIPE ViPipe, ISP_STATISTICS_CFG_S *pstStatCfg);

}

static void* libae = 0;

static PFN_CVI_AE_Register CVI_AE_Register = 0;
static PFN_CVI_AE_UnRegister CVI_AE_UnRegister = 0;

static void* libawb = 0;

static PFN_CVI_AWB_Register CVI_AWB_Register = 0;
static PFN_CVI_AWB_UnRegister CVI_AWB_UnRegister = 0;

static void* libisp = 0;

static PFN_CVI_MIPI_SetMipiReset CVI_MIPI_SetMipiReset = 0;
static PFN_CVI_MIPI_SetSensorClock CVI_MIPI_SetSensorClock = 0;
static PFN_CVI_MIPI_SetSensorReset CVI_MIPI_SetSensorReset = 0;
static PFN_CVI_MIPI_SetMipiAttr CVI_MIPI_SetMipiAttr = 0;

static PFN_CVI_ISP_Init CVI_ISP_Init = 0;
static PFN_CVI_ISP_MemInit CVI_ISP_MemInit = 0;
static PFN_CVI_ISP_Run CVI_ISP_Run = 0;
static PFN_CVI_ISP_Exit CVI_ISP_Exit = 0;

static PFN_CVI_ISP_SetBindAttr CVI_ISP_SetBindAttr = 0;
static PFN_CVI_ISP_SetPubAttr CVI_ISP_SetPubAttr = 0;

static PFN_CVI_ISP_SetStatisticsConfig CVI_ISP_SetStatisticsConfig = 0;
static PFN_CVI_ISP_GetStatisticsConfig CVI_ISP_GetStatisticsConfig = 0;


static int unload_ae_library()
{
    if (libae)
    {
        dlclose(libae);
        libae = 0;
    }

    CVI_AE_Register = 0;
    CVI_AE_UnRegister = 0;

    return 0;
}

static int load_ae_library()
{
    if (libae)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        fprintf(stderr, "this device is not whitelisted for capture cvi\n");
        return -1;
    }

    libae = dlopen("libae.so", RTLD_LOCAL | RTLD_NOW);
    if (!libae)
    {
        libae = dlopen("/mnt/system/lib/libae.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libae)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    CVI_AE_Register = (PFN_CVI_AE_Register)dlsym(libae, "CVI_AE_Register");
    CVI_AE_UnRegister = (PFN_CVI_AE_UnRegister)dlsym(libae, "CVI_AE_UnRegister");

    return 0;

OUT:
    unload_ae_library();

    return -1;
}

class ae_library_loader
{
public:
    bool ready;

    ae_library_loader()
    {
        ready = (load_ae_library() == 0);
    }

    ~ae_library_loader()
    {
        unload_ae_library();
    }
};

static ae_library_loader ae;

static int unload_awb_library()
{
    if (libawb)
    {
        dlclose(libawb);
        libawb = 0;
    }

    CVI_AWB_Register = 0;
    CVI_AWB_UnRegister = 0;

    return 0;
}

static int load_awb_library()
{
    if (libawb)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        fprintf(stderr, "this device is not whitelisted for capture cvi\n");
        return -1;
    }

    libawb = dlopen("libawb.so", RTLD_LOCAL | RTLD_NOW);
    if (!libawb)
    {
        libawb = dlopen("/mnt/system/lib/libawb.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libawb)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    CVI_AWB_Register = (PFN_CVI_AWB_Register)dlsym(libawb, "CVI_AWB_Register");
    CVI_AWB_UnRegister = (PFN_CVI_AWB_UnRegister)dlsym(libawb, "CVI_AWB_UnRegister");

    return 0;

OUT:
    unload_awb_library();

    return -1;
}

class awb_library_loader
{
public:
    bool ready;

    awb_library_loader()
    {
        ready = (load_awb_library() == 0);
    }

    ~awb_library_loader()
    {
        unload_awb_library();
    }
};

static awb_library_loader awb;

static int unload_isp_library()
{
    if (libisp)
    {
        dlclose(libisp);
        libisp = 0;
    }

    CVI_MIPI_SetMipiReset = 0;
    CVI_MIPI_SetSensorClock = 0;
    CVI_MIPI_SetSensorReset = 0;
    CVI_MIPI_SetMipiAttr = 0;

    CVI_ISP_Init = 0;
    CVI_ISP_MemInit = 0;
    CVI_ISP_Run = 0;
    CVI_ISP_Exit = 0;

    CVI_ISP_SetBindAttr = 0;
    CVI_ISP_SetPubAttr = 0;

    CVI_ISP_SetStatisticsConfig = 0;
    CVI_ISP_GetStatisticsConfig = 0;

    return 0;
}

static int load_isp_library()
{
    if (libisp)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        fprintf(stderr, "this device is not whitelisted for capture cvi\n");
        return -1;
    }

    libisp = dlopen("libisp.so", RTLD_LOCAL | RTLD_NOW);
    if (!libisp)
    {
        libisp = dlopen("/mnt/system/lib/libisp.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libisp)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    CVI_MIPI_SetMipiReset = (PFN_CVI_MIPI_SetMipiReset)dlsym(libisp, "CVI_MIPI_SetMipiReset");
    CVI_MIPI_SetSensorClock = (PFN_CVI_MIPI_SetSensorClock)dlsym(libisp, "CVI_MIPI_SetSensorClock");
    CVI_MIPI_SetSensorReset = (PFN_CVI_MIPI_SetSensorReset)dlsym(libisp, "CVI_MIPI_SetSensorReset");
    CVI_MIPI_SetMipiAttr = (PFN_CVI_MIPI_SetMipiAttr)dlsym(libisp, "CVI_MIPI_SetMipiAttr");

    CVI_ISP_Init = (PFN_CVI_ISP_Init)dlsym(libisp, "CVI_ISP_Init");
    CVI_ISP_MemInit = (PFN_CVI_ISP_MemInit)dlsym(libisp, "CVI_ISP_MemInit");
    CVI_ISP_Run = (PFN_CVI_ISP_Run)dlsym(libisp, "CVI_ISP_Run");
    CVI_ISP_Exit = (PFN_CVI_ISP_Exit)dlsym(libisp, "CVI_ISP_Exit");

    CVI_ISP_SetBindAttr = (PFN_CVI_ISP_SetBindAttr)dlsym(libisp, "CVI_ISP_SetBindAttr");
    CVI_ISP_SetPubAttr = (PFN_CVI_ISP_SetPubAttr)dlsym(libisp, "CVI_ISP_SetPubAttr");

    CVI_ISP_SetStatisticsConfig = (PFN_CVI_ISP_SetStatisticsConfig)dlsym(libisp, "CVI_ISP_SetStatisticsConfig");
    CVI_ISP_GetStatisticsConfig = (PFN_CVI_ISP_GetStatisticsConfig)dlsym(libisp, "CVI_ISP_GetStatisticsConfig");

    return 0;

OUT:
    unload_isp_library();

    return -1;
}

class isp_library_loader
{
public:
    bool ready;

    isp_library_loader()
    {
        ready = (load_isp_library() == 0);
    }

    ~isp_library_loader()
    {
        unload_isp_library();
    }
};

static isp_library_loader isp;


extern "C"
{
#define BIN_FILE_LENGTH    256

typedef CVI_S32 (*PFN_CVI_BIN_GetBinName)(CVI_CHAR *binName);
typedef CVI_S32 (*PFN_CVI_BIN_ImportBinData)(CVI_U8 *pu8Buffer, CVI_U32 u32DataLength);
}

static void* libcvi_bin_cvi_bin_isp = 0;
static void* libcvi_bin_vpu = 0;
static void* libcvi_bin = 0;

static PFN_CVI_BIN_GetBinName CVI_BIN_GetBinName = 0;
static PFN_CVI_BIN_ImportBinData CVI_BIN_ImportBinData = 0;

static int unload_cvi_bin_library()
{
    if (libcvi_bin_cvi_bin_isp)
    {
        dlclose(libcvi_bin_cvi_bin_isp);
        libcvi_bin_cvi_bin_isp = 0;
    }

    if (libcvi_bin_vpu)
    {
        dlclose(libcvi_bin_vpu);
        libcvi_bin_vpu = 0;
    }

    if (libcvi_bin)
    {
        dlclose(libcvi_bin);
        libcvi_bin = 0;
    }

    CVI_BIN_GetBinName = 0;
    CVI_BIN_ImportBinData = 0;

    return 0;
}

static int load_cvi_bin_library()
{
    if (libcvi_bin)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        fprintf(stderr, "this device is not whitelisted for capture cvi\n");
        return -1;
    }

    libcvi_bin_cvi_bin_isp = dlopen("libcvi_bin_isp.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libcvi_bin_cvi_bin_isp)
    {
        libcvi_bin_cvi_bin_isp = dlopen("/mnt/system/lib/libcvi_bin_isp.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libcvi_bin_cvi_bin_isp)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libcvi_bin_vpu = dlopen("libvpu.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libcvi_bin_vpu)
    {
        libcvi_bin_vpu = dlopen("/mnt/system/lib/libvpu.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libcvi_bin_vpu)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libcvi_bin = dlopen("libcvi_bin.so", RTLD_LOCAL | RTLD_NOW);
    if (!libcvi_bin)
    {
        libcvi_bin = dlopen("/mnt/system/lib/libcvi_bin.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libcvi_bin)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    CVI_BIN_GetBinName = (PFN_CVI_BIN_GetBinName)dlsym(libcvi_bin, "CVI_BIN_GetBinName");
    CVI_BIN_ImportBinData = (PFN_CVI_BIN_ImportBinData)dlsym(libcvi_bin, "CVI_BIN_ImportBinData");

    return 0;

OUT:
    unload_cvi_bin_library();

    return -1;
}

class cvi_bin_library_loader
{
public:
    bool ready;

    cvi_bin_library_loader()
    {
        ready = (load_cvi_bin_library() == 0);
    }

    ~cvi_bin_library_loader()
    {
        unload_cvi_bin_library();
    }
};

static cvi_bin_library_loader cvi_bin;


struct sns_ini_cfg
{
    int bus_id;
    int sns_i2c_addr;
    int mipi_dev;
    short lane_id[5];
    signed char pn_swap[5];
};

static const struct sns_ini_cfg* get_sns_ini_cfg()
{
    const int device_model = get_device_model();

    if (device_model == 1)
    {
        // milkv duo
        static const struct sns_ini_cfg duo = {
            1,  // bus_id
            37, // sns_i2c_addr
            0,  // mipi_dev
            {3, 2, 4, -1, -1},  // lane_id
            {0, 0, 0, 0, 0}     // pn_swap
        };

        return &duo;
    }
    if (device_model == 2)
    {
        // milkv duo 256
        static const struct sns_ini_cfg duo256m = {
            2,  // bus_id
            37, // sns_i2c_addr
            0,  // mipi_dev
            {1, 0, 2, -1, -1},  // lane_id
            {0, 0, 0, 0, 0}     // pn_swap
        };

        return &duo256m;
    }

    return NULL;
}

// gc2083 info
static const CVI_U16 cap_width = 1920;
static const CVI_U16 cap_height = 1080;
static const float cap_fps = 30;
static const WDR_MODE_E cap_wdr_mode = WDR_MODE_NONE;
static const BAYER_FORMAT_E cap_bayer_format = BAYER_FORMAT_RG;
static const ISP_BAYER_FORMAT_E isp_bayer_format = BAYER_RGGB;

static void* isp_main(void* arg)
{
    VI_PIPE ViPipe = *((VI_PIPE*)arg);

    prctl(PR_SET_NAME, "ISP0_RUN", 0, 0, 0);

    CVI_S32 ret = CVI_ISP_Run(ViPipe);
    if (ret != CVI_SUCCESS)
    {
        fprintf(stderr, "CVI_ISP_Run failed %x\n", ret);
    }

    return NULL;
}

class capture_cvi_impl
{
public:
    capture_cvi_impl();
    ~capture_cvi_impl();

    int open(int width = 1920, int height = 1080, float fps = 30);

    int start_streaming();

    int read_frame(unsigned char* bgrdata);

    int stop_streaming();

    int close();

public:
    int crop_width;
    int crop_height;

    int output_width;
    int output_height;

    // flag
    int b_vb_inited = 0;
    int b_sys_inited = 0;
    int b_sys_vi_opened = 0;

    // vb pool
    int b_vb_pool0_created = 0;
    int b_vb_pool1_created = 0;
    VB_POOL VbPool0 = VB_INVALID_POOLID;
    VB_POOL VbPool1 = VB_INVALID_POOLID;

    // sensor
    int b_sensor_set = 0;

    // mipi
    int b_mipi_sensor_set = 0;

    // vi
    int b_vi_dev_enabled = 0;
    int b_vi_pipe_created = 0;
    int b_vi_pipe_started = 0;
    int b_vi_chn_enabled = 0;
    int b_vi_frame_got = 0;

    VI_DEV ViDev = 0;
    VI_PIPE ViPipe = 0;
    VI_CHN ViChn = 0;
    VIDEO_FRAME_INFO_S stFrameInfo;

    // isp
    int b_isp_ae_registered = 0;
    int b_isp_awb_registered = 0;
    int b_isp_inited = 0;
    int b_isp_thread_created = 0;

    pthread_t isp_thread = 0;

    // vpss
    int b_vpss_grp_created = 0;
    int b_vpss_chn_enabled = 0;
    int b_vpss_vbpool_attached = 0;
    int b_vpss_grp_started = 0;
    int b_vpss_frame_got = 0;

    VPSS_GRP VpssGrp = 0;
    // VPSS_GRP VpssGrp = CVI_VPSS_GetAvailableGrp();
    VPSS_CHN VpssChn = VPSS_CHN0;
    VIDEO_FRAME_INFO_S stFrameInfo_bgr;

};

capture_cvi_impl::capture_cvi_impl()
{
    crop_width = 0;
    crop_height = 0;

    output_width = 0;
    output_height = 0;

    b_vb_inited = 0;
    b_sys_inited = 0;
    b_sys_vi_opened = 0;

    b_vb_pool0_created = 0;
    b_vb_pool1_created = 0;
    b_vb_pool1_created = 0;
    VbPool0 = VB_INVALID_POOLID;
    VbPool1 = VB_INVALID_POOLID;
    VbPool1 = VB_INVALID_POOLID;

    b_sensor_set = 0;

    b_mipi_sensor_set = 0;

    b_vi_dev_enabled = 0;
    b_vi_pipe_created = 0;
    b_vi_pipe_started = 0;
    b_vi_chn_enabled = 0;
    b_vi_frame_got = 0;

    ViDev = 0;
    ViPipe = 0;
    ViChn = 0;

    b_isp_ae_registered = 0;
    b_isp_awb_registered = 0;
    b_isp_inited = 0;
    b_isp_thread_created = 0;

    isp_thread = 0;

    b_vpss_grp_created = 0;
    b_vpss_chn_enabled = 0;
    b_vpss_vbpool_attached = 0;
    b_vpss_grp_started = 0;
    b_vpss_frame_got = 0;

    VpssGrp = 0;
    // VpssGrp = CVI_VPSS_GetAvailableGrp();
    VpssChn = VPSS_CHN0;
}

capture_cvi_impl::~capture_cvi_impl()
{
    close();
}

int capture_cvi_impl::open(int width, int height, float fps)
{
    // resolve output size
    {
        if (width > cap_width && height > cap_height)
        {
            if (cap_width * height == width * cap_height)
            {
                // same ratio, no crop
                output_width = cap_width;
                output_height = cap_height;
            }
            else if (cap_width / (float)cap_height > width / (float)height)
            {
                // fatter
                output_width = width * cap_height / height;
                output_height = cap_height;
            }
            else
            {
                // thinner
                output_width = cap_width;
                output_height = height * cap_width / width;
            }
        }
        else if (width > cap_width)
        {
            output_width = cap_width;
            output_height = height * cap_width / width;
        }
        else if (height > cap_height)
        {
            output_height = cap_height;
            output_width = width * cap_height / height;
        }
        else
        {
            output_width = width;
            output_height = height;
        }
    }

    if (output_width < 8 || output_height < 8)
    {
        fprintf(stderr, "invalid size %d x %d -> %d x %d\n", width, height, output_width, output_height);
        return -1;
    }

    // resolve cap crop
    {
        if (cap_width * height == width * cap_height)
        {
            // same ratio, no crop
            crop_width = cap_width;
            crop_height = cap_height;
        }
        else if (cap_width / (float)cap_height > width / (float)height)
        {
            // fatter
            crop_width = width * cap_height / height;
            crop_height = cap_height;
        }
        else
        {
            // thinner
            crop_width = cap_width;
            crop_height = height * cap_width / width;
        }
    }


#define ALIGN(x, a)      (((x) + ((a)-1)) & ~((a)-1))

    int yuv_buffer_size = ALIGN(cap_width, 64) * ALIGN(cap_height, 64) * 3 / 2;
    int bgr_buffer_size = ALIGN(output_width, 64) * ALIGN(output_height, 64) * 3;

    int ret_val = 0;

    // init vb pool
    {
        VB_CONFIG_S stVbConfig;
        stVbConfig.u32MaxPoolCnt = 1;
        stVbConfig.astCommPool[0].u32BlkSize = yuv_buffer_size;
        stVbConfig.astCommPool[0].u32BlkCnt = 4;
        stVbConfig.astCommPool[0].enRemapMode = VB_REMAP_MODE_NONE;
        snprintf(stVbConfig.astCommPool[0].acName, MAX_VB_POOL_NAME_LEN, "cv-capture-comm0");

        CVI_S32 ret = CVI_VB_SetConfig(&stVbConfig);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VB_SetConfig failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    {
        CVI_S32 ret = CVI_VB_Init();
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VB_Init failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vb_inited = 1;
    }

    {
        CVI_S32 ret = CVI_SYS_Init();
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_SYS_Init failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_sys_inited = 1;
    }

    {
        CVI_S32 ret = CVI_SYS_VI_Open();
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_SYS_VI_Open failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_sys_vi_opened = 1;
    }

    // prepare vb pool
    {
    //     // create vb pool0
    //     {
    //         VB_POOL_CONFIG_S stVbPoolCfg;
    //         stVbPoolCfg.u32BlkSize = yuv_buffer_size;
    //         stVbPoolCfg.u32BlkCnt = 4;
    //         stVbPoolCfg.enRemapMode = VB_REMAP_MODE_NONE;
    //         snprintf(stVbPoolCfg.acName, MAX_VB_POOL_NAME_LEN, "cv-capture-yuv");
    //
    //         VbPool0 = CVI_VB_CreatePool(&stVbPoolCfg);
    //         if (VbPool0 == VB_INVALID_POOLID)
    //         {
    //             fprintf(stderr, "CVI_VB_CreatePool VbPool0 failed %x\n", VbPool0);
    //             ret_val = -1;
    //             goto OUT;
    //         }
    //
    //         b_vb_pool0_created = 1;
    //     }

        // create vb pool1
        {
            VB_POOL_CONFIG_S stVbPoolCfg;
            stVbPoolCfg.u32BlkSize = bgr_buffer_size;
            stVbPoolCfg.u32BlkCnt = 1;
            stVbPoolCfg.enRemapMode = VB_REMAP_MODE_NONE;
            snprintf(stVbPoolCfg.acName, MAX_VB_POOL_NAME_LEN, "cv-capture-bgr");

            VbPool1 = CVI_VB_CreatePool(&stVbPoolCfg);
            if (VbPool1 == VB_INVALID_POOLID)
            {
                fprintf(stderr, "CVI_VB_CreatePool VbPool1 failed %x\n", VbPool1);
                ret_val = -1;
                goto OUT;
            }

            b_vb_pool1_created = 1;
        }
    }

    // prepare sensor
    {
        if (pstSnsObj->pfnPatchRxAttr)
        {
            RX_INIT_ATTR_S stRxInitAttr;
            memset(&stRxInitAttr, 0, sizeof(RX_INIT_ATTR_S));
            stRxInitAttr.MipiDev = get_sns_ini_cfg()->mipi_dev;
            stRxInitAttr.as16LaneId[0] = get_sns_ini_cfg()->lane_id[0];
            stRxInitAttr.as16LaneId[1] = get_sns_ini_cfg()->lane_id[1];
            stRxInitAttr.as16LaneId[2] = get_sns_ini_cfg()->lane_id[2];
            stRxInitAttr.as16LaneId[3] = get_sns_ini_cfg()->lane_id[3];
            stRxInitAttr.as16LaneId[4] = get_sns_ini_cfg()->lane_id[4];
            stRxInitAttr.as8PNSwap[0] = get_sns_ini_cfg()->pn_swap[0];
            stRxInitAttr.as8PNSwap[1] = get_sns_ini_cfg()->pn_swap[1];
            stRxInitAttr.as8PNSwap[2] = get_sns_ini_cfg()->pn_swap[2];
            stRxInitAttr.as8PNSwap[3] = get_sns_ini_cfg()->pn_swap[3];
            stRxInitAttr.as8PNSwap[4] = get_sns_ini_cfg()->pn_swap[4];

            CVI_S32 ret = pstSnsObj->pfnPatchRxAttr(&stRxInitAttr);
            if (ret < 0)
            {
                fprintf(stderr, "pfnPatchRxAttr failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        if (pstSnsObj->pfnSetInit)
        {
            ISP_INIT_ATTR_S stInitAttr;
            memset(&stInitAttr, 0, sizeof(stInitAttr));
            stInitAttr.enGainMode = SNS_GAIN_MODE_SHARE;
            stInitAttr.enSnsBdgMuxMode = SNS_BDG_MUX_NONE;

            CVI_S32 ret = pstSnsObj->pfnSetInit(ViPipe, &stInitAttr);
            if (ret < 0)
            {
                fprintf(stderr, "pfnSetInit failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        if (pstSnsObj->pfnSetBusInfo)
        {
            ISP_SNS_COMMBUS_U unSnsrBusInfo;
            memset(&unSnsrBusInfo, 0, sizeof(unSnsrBusInfo));
            unSnsrBusInfo.s8I2cDev = get_sns_ini_cfg()->bus_id;

            CVI_S32 ret = pstSnsObj->pfnSetBusInfo(ViPipe, unSnsrBusInfo);
            if (ret < 0)
            {
                fprintf(stderr, "pfnSetBusInfo failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        if (pstSnsObj->pfnPatchI2cAddr)
        {
            pstSnsObj->pfnPatchI2cAddr(get_sns_ini_cfg()->sns_i2c_addr);
        }

        if (pstSnsObj->pfnRegisterCallback)
        {
            ALG_LIB_S stAeLib;
            ALG_LIB_S stAwbLib;
            stAeLib.s32Id = ViPipe;
            stAwbLib.s32Id = ViPipe;
            strncpy(stAeLib.acLibName, CVI_AE_LIB_NAME, sizeof(stAeLib.acLibName));
            strncpy(stAwbLib.acLibName, CVI_AWB_LIB_NAME, sizeof(stAwbLib.acLibName));

            CVI_S32 ret = pstSnsObj->pfnRegisterCallback(ViPipe, &stAeLib, &stAwbLib);
            if (ret < 0)
            {
                fprintf(stderr, "pfnRegisterCallback failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        if (pstSnsObj->pfnExpSensorCb)
        {
            ISP_SENSOR_EXP_FUNC_S stSnsrSensorFunc;
            pstSnsObj->pfnExpSensorCb(&stSnsrSensorFunc);

            if (stSnsrSensorFunc.pfn_cmos_set_image_mode)
            {
                ISP_CMOS_SENSOR_IMAGE_MODE_S stSnsrMode;
                stSnsrMode.u16Width = cap_width;
                stSnsrMode.u16Height = cap_height;
                stSnsrMode.f32Fps = cap_fps;

                CVI_S32 ret = stSnsrSensorFunc.pfn_cmos_set_image_mode(ViPipe, &stSnsrMode);
                if (ret != CVI_SUCCESS)
                {
                    fprintf(stderr, "pfn_cmos_set_image_mode failed %x\n", ret);
                    ret_val = -1;
                    goto OUT;
                }
            }

            if (stSnsrSensorFunc.pfn_cmos_set_wdr_mode)
            {
                CVI_S32 ret = stSnsrSensorFunc.pfn_cmos_set_wdr_mode(ViPipe, cap_wdr_mode);
                if (ret != CVI_SUCCESS)
                {
                    fprintf(stderr, "pfn_cmos_set_wdr_mode failed %x\n", ret);
                    ret_val = -1;
                    goto OUT;
                }
            }
        }

        b_sensor_set = 1;
    }

    // prepare vi
    {
        VI_DEV_ATTR_S stViDevAttr;
        stViDevAttr.enIntfMode = VI_MODE_MIPI;
        stViDevAttr.enWorkMode = VI_WORK_MODE_1Multiplex;
        stViDevAttr.enScanMode = VI_SCAN_PROGRESSIVE;
        stViDevAttr.as32AdChnId[0] = -1;
        stViDevAttr.as32AdChnId[1] = -1;
        stViDevAttr.as32AdChnId[2] = -1;
        stViDevAttr.as32AdChnId[3] = -1;
        stViDevAttr.enDataSeq = VI_DATA_SEQ_YUYV;
        stViDevAttr.stSynCfg.enVsync = VI_VSYNC_PULSE;
        stViDevAttr.stSynCfg.enVsyncNeg = VI_VSYNC_NEG_LOW;
        stViDevAttr.stSynCfg.enHsync = VI_HSYNC_VALID_SINGNAL;
        stViDevAttr.stSynCfg.enHsyncNeg = VI_HSYNC_NEG_HIGH;
        stViDevAttr.stSynCfg.enVsyncValid = VI_VSYNC_VALID_SIGNAL;
        stViDevAttr.stSynCfg.enVsyncValidNeg = VI_VSYNC_VALID_NEG_HIGH;
        stViDevAttr.stSynCfg.stTimingBlank.u32HsyncHfb = 0;
        stViDevAttr.stSynCfg.stTimingBlank.u32HsyncAct = cap_width;
        stViDevAttr.stSynCfg.stTimingBlank.u32HsyncHbb = 0;
        stViDevAttr.stSynCfg.stTimingBlank.u32VsyncVfb = 0;
        stViDevAttr.stSynCfg.stTimingBlank.u32VsyncVact = cap_height;
        stViDevAttr.stSynCfg.stTimingBlank.u32VsyncVbb = 0;
        stViDevAttr.stSynCfg.stTimingBlank.u32VsyncVbfb = 0;
        stViDevAttr.stSynCfg.stTimingBlank.u32VsyncVbact = 0;
        stViDevAttr.stSynCfg.stTimingBlank.u32VsyncVbbb = 0;
        stViDevAttr.enInputDataType = VI_DATA_TYPE_RGB;
        stViDevAttr.stSize.u32Width = cap_width;
        stViDevAttr.stSize.u32Height = cap_height;
        stViDevAttr.stWDRAttr.enWDRMode = cap_wdr_mode;
        stViDevAttr.stWDRAttr.u32CacheLine = cap_height;
        stViDevAttr.stWDRAttr.bSyntheticWDR = 0;
        stViDevAttr.enBayerFormat = cap_bayer_format;
        stViDevAttr.chn_num = 0;// FIXME
        stViDevAttr.snrFps = cap_fps;

        CVI_S32 ret = CVI_VI_SetDevAttr(ViDev, &stViDevAttr);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_SetDevAttr failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    {
        CVI_S32 ret = CVI_VI_EnableDev(ViDev);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_EnableDev failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vi_dev_enabled = 1;
    }

    // prepare mipi
    {
        SNS_COMBO_DEV_ATTR_S stDevAttr;
        {
            CVI_S32 ret = pstSnsObj->pfnGetRxAttr(ViPipe, &stDevAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "pfnGetRxAttr failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            CVI_S32 ret = CVI_MIPI_SetSensorReset(get_sns_ini_cfg()->mipi_dev, 1);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_MIPI_SetSensorReset 1 failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            CVI_S32 ret = CVI_MIPI_SetMipiReset(get_sns_ini_cfg()->mipi_dev, 1);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_MIPI_SetMipiReset 1 failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            CVI_S32 ret = CVI_MIPI_SetMipiAttr(ViPipe, (const CVI_VOID*)&stDevAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_MIPI_SetMipiAttr failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            CVI_S32 ret = CVI_MIPI_SetSensorClock(get_sns_ini_cfg()->mipi_dev, 1);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_MIPI_SetSensorClock 1 failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        usleep(20); // FIXME why ?

        {
            CVI_S32 ret = CVI_MIPI_SetSensorReset(get_sns_ini_cfg()->mipi_dev, 0);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_MIPI_SetSensorReset 0 failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        if (pstSnsObj->pfnSnsProbe)
        {
            CVI_S32 ret = pstSnsObj->pfnSnsProbe(ViPipe);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "pfnSnsProbe failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        b_mipi_sensor_set = 1;
    }

    // prepare vipipe
    {
        VI_PIPE_ATTR_S stPipeAttr;
        stPipeAttr.enPipeBypassMode = VI_PIPE_BYPASS_NONE;
        stPipeAttr.bYuvSkip = CVI_FALSE;
        stPipeAttr.bIspBypass = CVI_FALSE;
        stPipeAttr.u32MaxW = cap_width;
        stPipeAttr.u32MaxH = cap_height;
        stPipeAttr.enPixFmt = PIXEL_FORMAT_RGB_BAYER_10BPP;
        stPipeAttr.enCompressMode = COMPRESS_MODE_TILE;
        stPipeAttr.enBitWidth = DATA_BITWIDTH_10;
        stPipeAttr.bNrEn = CVI_TRUE;// noise reduce
        stPipeAttr.bSharpenEn = CVI_FALSE;
        stPipeAttr.stFrameRate.s32SrcFrameRate = -1;
        stPipeAttr.stFrameRate.s32DstFrameRate = -1;
        stPipeAttr.bDiscardProPic = CVI_FALSE;
        stPipeAttr.bYuvBypassPath = CVI_FALSE;

        CVI_S32 ret = CVI_VI_CreatePipe(ViPipe, &stPipeAttr);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_CreatePipe failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vi_pipe_created = 1;
    }

    // prepare isp
    {
        {
            ALG_LIB_S stAeLib;
            stAeLib.s32Id = ViPipe;
            strncpy(stAeLib.acLibName, CVI_AE_LIB_NAME, sizeof(stAeLib.acLibName));

            CVI_S32 ret = CVI_AE_Register(ViPipe, &stAeLib);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_AE_Register failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_isp_ae_registered = 1;
        }

        {
            ALG_LIB_S stAwbLib;
            stAwbLib.s32Id = ViPipe;
            strncpy(stAwbLib.acLibName, CVI_AWB_LIB_NAME, sizeof(stAwbLib.acLibName));

            CVI_S32 ret = CVI_AWB_Register(ViPipe, &stAwbLib);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_AWB_Register failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_isp_awb_registered = 1;
        }

        {
            ISP_BIND_ATTR_S stBindAttr;
            snprintf(stBindAttr.stAeLib.acLibName, sizeof(CVI_AE_LIB_NAME), "%s", CVI_AE_LIB_NAME);
            stBindAttr.stAeLib.s32Id = ViPipe;
            stBindAttr.sensorId = 0;
            snprintf(stBindAttr.stAwbLib.acLibName, sizeof(CVI_AWB_LIB_NAME), "%s", CVI_AWB_LIB_NAME);
            stBindAttr.stAwbLib.s32Id = ViPipe;

            CVI_S32 ret = CVI_ISP_SetBindAttr(ViPipe, &stBindAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_ISP_SetBindAttr failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            CVI_S32 ret = CVI_ISP_MemInit(ViPipe);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_ISP_MemInit failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            ISP_PUB_ATTR_S stPubAttr;
            stPubAttr.stWndRect.s32X = 0;
            stPubAttr.stWndRect.s32Y = 0;
            stPubAttr.stWndRect.u32Width = cap_width;
            stPubAttr.stWndRect.u32Height = cap_height;
            stPubAttr.stSnsSize.u32Width = cap_width;
            stPubAttr.stSnsSize.u32Height = cap_height;
            stPubAttr.f32FrameRate = cap_fps;
            stPubAttr.enBayer = isp_bayer_format;
            stPubAttr.enWDRMode = cap_wdr_mode;
            stPubAttr.u8SnsMode = 0;

            CVI_S32 ret = CVI_ISP_SetPubAttr(ViPipe, &stPubAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_ISP_SetPubAttr failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            ISP_STATISTICS_CFG_S stsCfg;
            memset(&stsCfg, 0, sizeof(stsCfg));
            CVI_ISP_GetStatisticsConfig(0, &stsCfg);

            stsCfg.stAECfg.stCrop[0].bEnable = 0;
            stsCfg.stAECfg.stCrop[0].u16X = stsCfg.stAECfg.stCrop[0].u16Y = 0;
            stsCfg.stAECfg.stCrop[0].u16W = cap_width;
            stsCfg.stAECfg.stCrop[0].u16H = cap_height;
            memset(stsCfg.stAECfg.au8Weight, 1, AE_WEIGHT_ZONE_ROW * AE_WEIGHT_ZONE_COLUMN * sizeof(CVI_U8));

            stsCfg.stWBCfg.u16ZoneRow = AWB_ZONE_ORIG_ROW;
            stsCfg.stWBCfg.u16ZoneCol = AWB_ZONE_ORIG_COLUMN;
            stsCfg.stWBCfg.stCrop.bEnable = 0;
            stsCfg.stWBCfg.stCrop.u16X = stsCfg.stWBCfg.stCrop.u16Y = 0;
            stsCfg.stWBCfg.stCrop.u16W = cap_width;
            stsCfg.stWBCfg.stCrop.u16H = cap_height;
            stsCfg.stWBCfg.u16BlackLevel = 0;
            stsCfg.stWBCfg.u16WhiteLevel = 4095;
            stsCfg.stFocusCfg.stConfig.bEnable = 1;
            stsCfg.stFocusCfg.stConfig.u8HFltShift = 1;
            stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[0] = 1;
            stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[1] = 2;
            stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[2] = 3;
            stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[3] = 5;
            stsCfg.stFocusCfg.stConfig.s8HVFltLpCoeff[4] = 10;
            stsCfg.stFocusCfg.stConfig.stRawCfg.PreGammaEn = 0;
            stsCfg.stFocusCfg.stConfig.stPreFltCfg.PreFltEn = 1;
            stsCfg.stFocusCfg.stConfig.u16Hwnd = 17;
            stsCfg.stFocusCfg.stConfig.u16Vwnd = 15;
            stsCfg.stFocusCfg.stConfig.stCrop.bEnable = 0;
            // AF offset and size has some limitation.
            stsCfg.stFocusCfg.stConfig.stCrop.u16X = AF_XOFFSET_MIN;
            stsCfg.stFocusCfg.stConfig.stCrop.u16Y = AF_YOFFSET_MIN;
            stsCfg.stFocusCfg.stConfig.stCrop.u16W = cap_width - AF_XOFFSET_MIN * 2;
            stsCfg.stFocusCfg.stConfig.stCrop.u16H = cap_height - AF_YOFFSET_MIN * 2;
            //Horizontal HP0
            stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[0] = 0;
            stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[1] = 0;
            stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[2] = 13;
            stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[3] = 24;
            stsCfg.stFocusCfg.stHParam_FIR0.s8HFltHpCoeff[4] = 0;
            //Horizontal HP1
            stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[0] = 1;
            stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[1] = 2;
            stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[2] = 4;
            stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[3] = 8;
            stsCfg.stFocusCfg.stHParam_FIR1.s8HFltHpCoeff[4] = 0;
            //Vertical HP
            stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[0] = 13;
            stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[1] = 24;
            stsCfg.stFocusCfg.stVParam_FIR.s8VFltHpCoeff[2] = 0;
            stsCfg.unKey.bit1FEAeGloStat = 1;
            stsCfg.unKey.bit1FEAeLocStat = 1;
            stsCfg.unKey.bit1AwbStat1 = 1;
            stsCfg.unKey.bit1AwbStat2 = 1;
            stsCfg.unKey.bit1FEAfStat = 1;

            CVI_S32 ret = CVI_ISP_SetStatisticsConfig(ViPipe, &stsCfg);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_ISP_SetStatisticsConfig failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            CVI_S32 ret = CVI_ISP_Init(ViPipe);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_ISP_Init failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        b_isp_inited = 1;
    }

    // load bin
    {
        CVI_CHAR binName[BIN_FILE_LENGTH] = { 0 };
        {
            CVI_S32 ret = CVI_BIN_GetBinName(binName);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_BIN_GetBinName failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        fprintf(stderr, "binName = %s\n", (const char*)binName);

        FILE* fp = fopen((const char*)binName, "rb");
        if (!fp)
        {
            fprintf(stderr, "fopen %s failed\n", (const char*)binName);
            ret_val = -1;
        }

        fseek(fp, 0, SEEK_END);
        int len = ftell(fp);
        rewind(fp);

        std::vector<unsigned char> buf;
        buf.resize(len);
        fread((char*)buf.data(), 1, len, fp);

        fclose(fp);

        {
            CVI_S32 ret = CVI_BIN_ImportBinData(buf.data(), buf.size());
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_BIN_ImportBinData failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }
    }

    {
        VI_CHN_ATTR_S stChnAttr;
        stChnAttr.stSize.u32Width = cap_width;
        stChnAttr.stSize.u32Height = cap_height;
        stChnAttr.enPixelFormat = PIXEL_FORMAT_YUV_PLANAR_420;
        stChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
        stChnAttr.enVideoFormat = VIDEO_FORMAT_LINEAR;
        stChnAttr.enCompressMode = COMPRESS_MODE_NONE;
        stChnAttr.bMirror = CVI_FALSE;
        stChnAttr.bFlip = CVI_FALSE;
        stChnAttr.u32Depth = 1;
        stChnAttr.stFrameRate.s32SrcFrameRate = -1;
        stChnAttr.stFrameRate.s32DstFrameRate = -1;
        stChnAttr.u32BindVbPool = -1;// FIXME

        CVI_S32 ret = CVI_VI_SetChnAttr(ViPipe, ViChn, &stChnAttr);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_SetChnAttr failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    // prepare vpss
    {
        // vpss create grp
        {
            VPSS_GRP_ATTR_S stGrpAttr;
            stGrpAttr.u32MaxW = cap_width;
            stGrpAttr.u32MaxH = cap_height;
            stGrpAttr.enPixelFormat = PIXEL_FORMAT_NV21;
            stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
            stGrpAttr.stFrameRate.s32DstFrameRate = -1;
            stGrpAttr.u8VpssDev = 0;

            CVI_S32 ret = CVI_VPSS_CreateGrp(VpssGrp, &stGrpAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_CreateGrp failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vpss_grp_created = 1;
        }

        if (crop_width != cap_width || crop_height != cap_height)
        {
            VPSS_CROP_INFO_S stCropInfo;
            stCropInfo.bEnable = CVI_TRUE;
            stCropInfo.enCropCoordinate = VPSS_CROP_ABS_COOR;
            stCropInfo.stCropRect.s32X = (cap_width - crop_width) / 2;
            stCropInfo.stCropRect.s32Y = (cap_height - crop_height) / 2;
            stCropInfo.stCropRect.u32Width = crop_width;
            stCropInfo.stCropRect.u32Height = crop_height;

            CVI_S32 ret = CVI_VPSS_SetChnCrop(VpssGrp, VpssChn, &stCropInfo);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_SetChnCrop failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        // vpss set chn attr
        {
            VPSS_CHN_ATTR_S stChnAttr;
            stChnAttr.u32Width = output_width;
            stChnAttr.u32Height = output_height;
            stChnAttr.enVideoFormat = VIDEO_FORMAT_LINEAR;
            stChnAttr.enPixelFormat = PIXEL_FORMAT_BGR_888;
            stChnAttr.stFrameRate.s32SrcFrameRate = -1;
            stChnAttr.stFrameRate.s32DstFrameRate = -1;
            stChnAttr.bMirror = CVI_FALSE;
            stChnAttr.bFlip = CVI_FALSE;
            stChnAttr.u32Depth = 1;
            stChnAttr.stAspectRatio.enMode = ASPECT_RATIO_NONE;
            stChnAttr.stAspectRatio.bEnableBgColor = CVI_FALSE;
            stChnAttr.stAspectRatio.u32BgColor = 0;
            stChnAttr.stAspectRatio.stVideoRect.s32X = 0;
            stChnAttr.stAspectRatio.stVideoRect.s32Y = 0;
            stChnAttr.stAspectRatio.stVideoRect.u32Width = output_width;
            stChnAttr.stAspectRatio.stVideoRect.u32Height = output_height;
            stChnAttr.stNormalize.bEnable = CVI_FALSE;
            stChnAttr.stNormalize.factor[0] = 0.f;
            stChnAttr.stNormalize.factor[1] = 0.f;
            stChnAttr.stNormalize.factor[2] = 0.f;
            stChnAttr.stNormalize.mean[0] = 0.f;
            stChnAttr.stNormalize.mean[1] = 0.f;
            stChnAttr.stNormalize.mean[2] = 0.f;
            stChnAttr.stNormalize.rounding = VPSS_ROUNDING_TO_EVEN;

            CVI_S32 ret = CVI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stChnAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_SetChnAttr failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        // vpss enable chn
        {
            CVI_S32 ret = CVI_VPSS_EnableChn(VpssGrp, VpssChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_EnableChn failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vpss_chn_enabled = 1;
        }

        // vpss attach vb pool
        {
            CVI_S32 ret = CVI_VPSS_AttachVbPool(VpssGrp, VpssChn, VbPool1);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_AttachVbPool failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vpss_vbpool_attached = 1;
        }
    }

    return 0;

OUT:
    close();
    return ret_val;
}

int capture_cvi_impl::start_streaming()
{
    int ret_val = 0;

    {
        CVI_S32 ret = CVI_VI_StartPipe(ViPipe);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_StartPipe failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vi_pipe_started = 1;
    }

    // spawn isp thread
    {
        void* arg = &ViPipe;
        int ret = pthread_create(&isp_thread, 0, isp_main, arg);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_create failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_isp_thread_created = 1;
    }

    {
        CVI_S32 ret = CVI_VI_EnableChn(ViPipe, ViChn);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_EnableChn failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vi_chn_enabled = 1;
    }

    // vpss start grp
    {
        CVI_S32 ret = CVI_VPSS_StartGrp(VpssGrp);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_StartGrp failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vpss_grp_started = 1;
    }

    return 0;

OUT:
    close();
    return ret_val;
}

int capture_cvi_impl::read_frame(unsigned char* bgrdata)
{
    int ret_val = 0;

    // vi get frame
    {
        CVI_S32 ret = CVI_VI_GetChnFrame(ViPipe, ViChn, &stFrameInfo, 2000);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_GetChnFrame failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vi_frame_got = 1;
    }

    if (0)
    {
        // dump
        VIDEO_FRAME_S& vf = stFrameInfo.stVFrame;
        fprintf(stderr, "vf u32Width = %u\n", vf.u32Width);
        fprintf(stderr, "vf u32Height = %u\n", vf.u32Height);
        fprintf(stderr, "vf enPixelFormat = %d\n", vf.enPixelFormat);
        fprintf(stderr, "vf enBayerFormat = %d\n", vf.enBayerFormat);
        fprintf(stderr, "vf enVideoFormat = %d\n", vf.enVideoFormat);
        fprintf(stderr, "vf enColorGamut = %d\n", vf.enColorGamut);
        fprintf(stderr, "vf u32Stride = %u %u %u\n", vf.u32Stride[0], vf.u32Stride[1], vf.u32Stride[2]);
        fprintf(stderr, "vf u64PhyAddr = %lu %lu %lu\n", vf.u64PhyAddr[0], vf.u64PhyAddr[1], vf.u64PhyAddr[2]);
        fprintf(stderr, "vf pu8VirAddr = %p %p %p\n", vf.pu8VirAddr[0], vf.pu8VirAddr[1], vf.pu8VirAddr[2]);
        fprintf(stderr, "vf u32Length = %u %u %u\n", vf.u32Length[0], vf.u32Length[1], vf.u32Length[2]);
        fprintf(stderr, "vf s16OffsetTop = %d\n", vf.s16OffsetTop);
        fprintf(stderr, "vf s16OffsetBottom = %d\n", vf.s16OffsetBottom);
        fprintf(stderr, "vf s16OffsetLeft = %d\n", vf.s16OffsetLeft);
        fprintf(stderr, "vf s16OffsetRight = %d\n", vf.s16OffsetRight);
        fprintf(stderr, "vf u32TimeRef = %u\n", vf.u32TimeRef);
        fprintf(stderr, "vf u64PTS = %lu\n", vf.u64PTS);
        fprintf(stderr, "vf pPrivateData = %p\n", vf.pPrivateData);
        fprintf(stderr, "vf u32FrameFlag = %u\n", vf.u32FrameFlag);
    }

    // vpss send frame
    {
        CVI_S32 ret = CVI_VPSS_SendFrame(VpssGrp, &stFrameInfo, -1);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_SendFrame failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    // vpss get frame
    {
        CVI_S32 ret = CVI_VPSS_GetChnFrame(VpssGrp, VpssChn, &stFrameInfo_bgr, 2000);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_GetChnFrame failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vpss_frame_got = 1;
    }

    if (0)
    {
        // dump
        VIDEO_FRAME_S& vf = stFrameInfo_bgr.stVFrame;
        fprintf(stderr, "vf u32Width = %u\n", vf.u32Width);
        fprintf(stderr, "vf u32Height = %u\n", vf.u32Height);
        fprintf(stderr, "vf enPixelFormat = %d\n", vf.enPixelFormat);
        fprintf(stderr, "vf enBayerFormat = %d\n", vf.enBayerFormat);
        fprintf(stderr, "vf enVideoFormat = %d\n", vf.enVideoFormat);
        fprintf(stderr, "vf enColorGamut = %d\n", vf.enColorGamut);
        fprintf(stderr, "vf u32Stride = %u %u %u\n", vf.u32Stride[0], vf.u32Stride[1], vf.u32Stride[2]);
        fprintf(stderr, "vf u64PhyAddr = %lu %lu %lu\n", vf.u64PhyAddr[0], vf.u64PhyAddr[1], vf.u64PhyAddr[2]);
        fprintf(stderr, "vf pu8VirAddr = %p %p %p\n", vf.pu8VirAddr[0], vf.pu8VirAddr[1], vf.pu8VirAddr[2]);
        fprintf(stderr, "vf u32Length = %u %u %u\n", vf.u32Length[0], vf.u32Length[1], vf.u32Length[2]);
        fprintf(stderr, "vf s16OffsetTop = %d\n", vf.s16OffsetTop);
        fprintf(stderr, "vf s16OffsetBottom = %d\n", vf.s16OffsetBottom);
        fprintf(stderr, "vf s16OffsetLeft = %d\n", vf.s16OffsetLeft);
        fprintf(stderr, "vf s16OffsetRight = %d\n", vf.s16OffsetRight);
        fprintf(stderr, "vf u32TimeRef = %u\n", vf.u32TimeRef);
        fprintf(stderr, "vf u64PTS = %lu\n", vf.u64PTS);
        fprintf(stderr, "vf pPrivateData = %p\n", vf.pPrivateData);
        fprintf(stderr, "vf u32FrameFlag = %u\n", vf.u32FrameFlag);
    }

    // copy bgr
    {
        const VIDEO_FRAME_S& vf = stFrameInfo_bgr.stVFrame;

        // memcpy Y/BGR
        CVI_U64 phyaddr = vf.u64PhyAddr[0];
        // const unsigned char* ptr = vf.pu8VirAddr[0];
        const int stride = vf.u32Stride[0];
        const int length = vf.u32Length[0];

        const int border_top = vf.s16OffsetTop;
        // const int border_bottom = vf.s16OffsetBottom;
        const int border_left = vf.s16OffsetLeft;
        // const int border_right = vf.s16OffsetRight;

        // fprintf(stderr, "border %d %d %d %d\n", border_top, border_bottom, border_left, border_right);

        void* mapped_ptr = CVI_SYS_MmapCache(phyaddr, length);
        //CVI_SYS_IonInvalidateCache(phyaddr, mapped_ptr, length);

        const unsigned char* ptr = (const unsigned char*)mapped_ptr + border_top * stride + border_left;

        // copy to bgrdata
        int h2 = output_height;
        int w2 = output_width * 3;
        if (stride == output_width * 3)
        {
            h2 = 1;
            w2 = output_height * output_width * 3;
        }

        for (int i = 0; i < h2; i++)
        {
#if __riscv_vector
            int j = 0;
            int n = w2;
            while (n > 0) {
                size_t vl = vsetvl_e8m8(n);
                vuint8m8_t bgr = vle8_v_u8m8(ptr + j, vl);
                vse8_v_u8m8(bgrdata, bgr, vl);
                bgrdata += vl;
                j += vl;
                n -= vl;
            }
#else
            memcpy(bgrdata, ptr, w2);
            bgrdata += w2;
#endif

            ptr += stride;
        }

        CVI_SYS_Munmap(mapped_ptr, length);
    }

OUT:

    if (b_vpss_frame_got)
    {
        CVI_S32 ret = CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrameInfo_bgr);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_ReleaseChnFrame failed %x\n", ret);
            ret_val = -1;
        }

        b_vpss_frame_got = 0;
    }

    if (b_vi_frame_got)
    {
        CVI_S32 ret = CVI_VI_ReleaseChnFrame(ViPipe, ViChn, &stFrameInfo);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_ReleaseChnFrame failed %x\n", ret);
            ret_val = -1;
        }

        b_vi_frame_got = 0;
    }

    return ret_val;
}

int capture_cvi_impl::stop_streaming()
{
    int ret_val = 0;

    if (b_vpss_grp_started)
    {
        CVI_S32 ret = CVI_VPSS_StopGrp(VpssGrp);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_StopGrp failed %x\n", ret);
            ret_val = -1;
        }

        b_vpss_grp_started = 0;
    }

    if (b_vi_chn_enabled)
    {
        CVI_S32 ret = CVI_VI_DisableChn(ViPipe, ViChn);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_DisableChn failed %x\n", ret);
            ret_val = -1;
        }

        b_vi_chn_enabled = 0;
    }

    if (b_isp_inited)
    {
        CVI_S32 ret = CVI_ISP_Exit(ViPipe);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_ISP_Exit failed %x\n", ret);
            ret_val = -1;
        }

        b_isp_inited = 0;
    }

    if (b_isp_thread_created)
    {
        int ret = pthread_join(isp_thread, 0);
        if (ret != 0)
        {
            fprintf(stderr, "pthread_join failed %x\n", ret);
            ret_val = -1;
        }

        b_isp_thread_created = 0;
    }

    if (b_vi_pipe_started)
    {
        CVI_S32 ret = CVI_VI_StopPipe(ViPipe);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_StopPipe failed %x\n", ret);
            ret_val = -1;
        }

        b_vi_pipe_started = 0;
    }

    return ret_val;
}

int capture_cvi_impl::close()
{
    int ret_val = 0;

    ret_val = stop_streaming();

    if (b_vpss_frame_got)
    {
        CVI_S32 ret = CVI_VPSS_ReleaseChnFrame(VpssGrp, VpssChn, &stFrameInfo_bgr);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_ReleaseChnFrame failed %x\n", ret);
            ret_val = -1;
        }

        b_vpss_frame_got = 0;
    }

    if (b_vi_frame_got)
    {
        CVI_S32 ret = CVI_VI_ReleaseChnFrame(ViPipe, ViChn, &stFrameInfo);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_ReleaseChnFrame failed %x\n", ret);
            ret_val = -1;
        }

        b_vi_frame_got = 0;
    }

    // vpss exit
    {
        if (b_vpss_vbpool_attached)
        {
            CVI_S32 ret = CVI_VPSS_DetachVbPool(VpssGrp, VpssChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_DetachVbPool failed %x\n", ret);
                ret_val = -1;
            }

            b_vpss_vbpool_attached = 0;
        }

        if (b_vpss_chn_enabled)
        {
            CVI_S32 ret = CVI_VPSS_DisableChn(VpssGrp, VpssChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_DisableChn failed %x\n", ret);
                ret_val = -1;
            }

            b_vpss_chn_enabled = 0;
        }

        if (b_vpss_grp_created)
        {
            CVI_S32 ret = CVI_VPSS_DestroyGrp(VpssGrp);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_DestroyGrp failed %x\n", ret);
                ret_val = -1;
            }

            b_vpss_grp_created = 0;
        }
    }

    // isp exit
    {
        if (b_isp_ae_registered)
        {
            ALG_LIB_S stAeLib;
            stAeLib.s32Id = ViPipe;
            strncpy(stAeLib.acLibName, CVI_AE_LIB_NAME, sizeof(stAeLib.acLibName));

            CVI_S32 ret = CVI_AE_UnRegister(ViPipe, &stAeLib);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_AE_UnRegister failed %x\n", ret);
                ret_val = -1;
            }

            b_isp_ae_registered = 0;
        }

        if (b_isp_awb_registered)
        {
            ALG_LIB_S stAwbLib;
            stAwbLib.s32Id = ViPipe;
            strncpy(stAwbLib.acLibName, CVI_AWB_LIB_NAME, sizeof(stAwbLib.acLibName));

            CVI_S32 ret = CVI_AWB_UnRegister(ViPipe, &stAwbLib);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_AWB_UnRegister failed %x\n", ret);
                ret_val = -1;
            }

            b_isp_awb_registered = 0;
        }
    }

    if (b_vi_pipe_created)
    {
        CVI_S32 ret = CVI_VI_DestroyPipe(ViPipe);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_DestroyPipe failed %x\n", ret);
            ret_val = -1;
        }

        b_vi_pipe_created = 0;
    }

    if (b_mipi_sensor_set)
    {
        CVI_S32 ret = CVI_MIPI_SetSensorClock(get_sns_ini_cfg()->mipi_dev, 0);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_DisableDev failed %x\n", ret);
            ret_val = -1;
        }

        b_mipi_sensor_set = 0;
    }

    if (b_vi_dev_enabled)
    {
        CVI_S32 ret = CVI_VI_DisableDev(ViDev);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VI_DisableDev failed %x\n", ret);
            ret_val = -1;
        }

        b_vi_dev_enabled = 0;
    }

    if (b_sensor_set)
    {
        if (pstSnsObj->pfnStandby)
        {
            pstSnsObj->pfnStandby(ViPipe);
        }

        if (pstSnsObj->pfnExpSensorCb)
        {
            ISP_SENSOR_EXP_FUNC_S stSnsrSensorFunc;
            pstSnsObj->pfnExpSensorCb(&stSnsrSensorFunc);

            if (stSnsrSensorFunc.pfn_cmos_sensor_exit)
            {
                stSnsrSensorFunc.pfn_cmos_sensor_exit(ViPipe);
            }
        }

        if (pstSnsObj->pfnUnRegisterCallback)
        {
            ALG_LIB_S stAeLib;
            ALG_LIB_S stAwbLib;
            stAeLib.s32Id = ViPipe;
            stAwbLib.s32Id = ViPipe;
            strncpy(stAeLib.acLibName, CVI_AE_LIB_NAME, sizeof(stAeLib.acLibName));
            strncpy(stAwbLib.acLibName, CVI_AWB_LIB_NAME, sizeof(stAwbLib.acLibName));

            CVI_S32 ret = pstSnsObj->pfnUnRegisterCallback(ViPipe, &stAeLib, &stAwbLib);
            if (ret < 0)
            {
                fprintf(stderr, "pfnUnRegisterCallback failed %x\n", ret);
                ret_val = -1;
            }
        }

        b_sensor_set = 0;
    }

    // vb pool exit
    {
        if (b_vb_pool0_created)
        {
            CVI_S32 ret = CVI_VB_DestroyPool(VbPool0);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VB_DestroyPool failed %x\n", ret);
                ret_val = -1;
            }

            b_vb_pool0_created = 0;
        }

        if (b_vb_pool1_created)
        {
            CVI_S32 ret = CVI_VB_DestroyPool(VbPool1);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VB_DestroyPool failed %x\n", ret);
                ret_val = -1;
            }

            b_vb_pool1_created = 0;
        }
    }

    if (b_sys_vi_opened)
    {
        CVI_S32 ret = CVI_SYS_VI_Close();
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_SYS_VI_Close failed %x\n", ret);
            ret_val = -1;
        }

        b_sys_vi_opened = 0;
    }

    if (b_sys_inited)
    {
        CVI_S32 ret = CVI_SYS_Exit();
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_SYS_Exit failed %x\n", ret);
            ret_val = -1;
        }

        b_sys_inited = 0;
    }

    if (b_vb_inited)
    {
        CVI_S32 ret = CVI_VB_Exit();
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VB_Exit failed %x\n", ret);
            ret_val = -1;
        }

        b_vb_inited = 0;
    }

    crop_width = 0;
    crop_height = 0;

    output_width = 0;
    output_height = 0;

    b_vb_inited = 0;
    b_sys_inited = 0;
    b_sys_vi_opened = 0;

    b_vb_pool0_created = 0;
    b_vb_pool1_created = 0;
    b_vb_pool1_created = 0;
    VbPool0 = VB_INVALID_POOLID;
    VbPool1 = VB_INVALID_POOLID;
    VbPool1 = VB_INVALID_POOLID;

    b_sensor_set = 0;

    b_mipi_sensor_set = 0;

    b_vi_dev_enabled = 0;
    b_vi_pipe_created = 0;
    b_vi_pipe_started = 0;
    b_vi_chn_enabled = 0;
    b_vi_frame_got = 0;

    ViDev = 0;
    ViPipe = 0;
    ViChn = 0;

    b_isp_ae_registered = 0;
    b_isp_awb_registered = 0;
    b_isp_inited = 0;
    b_isp_thread_created = 0;

    isp_thread = 0;

    b_vpss_grp_created = 0;
    b_vpss_chn_enabled = 0;
    b_vpss_vbpool_attached = 0;
    b_vpss_grp_started = 0;
    b_vpss_frame_got = 0;

    VpssGrp = 0;
    // VpssGrp = CVI_VPSS_GetAvailableGrp();
    VpssChn = VPSS_CHN0;

    return ret_val;
}

bool capture_cvi::supported()
{
    if (!sys.ready)
        return false;

    if (!vpu.ready)
        return false;

    if (!sns_gc2083.ready)
        return false;

    if (!ae.ready)
        return false;

    if (!awb.ready)
        return false;

    if (!isp.ready)
        return false;

    if (!cvi_bin.ready)
        return false;

    return true;
}

capture_cvi::capture_cvi() : d(new capture_cvi_impl)
{
}

capture_cvi::~capture_cvi()
{
    delete d;
}

int capture_cvi::open(int width, int height, float fps)
{
    return d->open(width, height, fps);
}

int capture_cvi::get_width() const
{
    return d->output_width;
}

int capture_cvi::get_height() const
{
    return d->output_height;
}

float capture_cvi::get_fps() const
{
    return cap_fps;
}

int capture_cvi::start_streaming()
{
    return d->start_streaming();
}

int capture_cvi::read_frame(unsigned char* bgrdata)
{
    return d->read_frame(bgrdata);
}

int capture_cvi::stop_streaming()
{
    return d->stop_streaming();
}

int capture_cvi::close()
{
    return d->close();
}
#else // defined __linux__
bool capture_cvi::supported()
{
    return false;
}

capture_cvi::capture_cvi() : d(0)
{
}

capture_cvi::~capture_cvi()
{
}

int capture_cvi::open(int /*width*/, int /*height*/, float /*fps*/)
{
    return -1;
}

int capture_cvi::get_width() const
{
    return -1;
}

int capture_cvi::get_height() const
{
    return -1;
}

float capture_cvi::get_fps() const
{
    return 0.f;
}

int capture_cvi::start_streaming()
{
    return -1;
}

int capture_cvi::read_frame(unsigned char* /*bgrdata*/)
{
    return -1;
}

int capture_cvi::stop_streaming()
{
    return -1;
}

int capture_cvi::close()
{
    return -1;
}
#endif // defined __linux__
