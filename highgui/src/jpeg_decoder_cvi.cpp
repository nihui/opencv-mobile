//
// Copyright (C) 2023 nihui
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

#include "jpeg_decoder_cvi.h"

#if defined __linux__
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <sstream>
#include <string>

#if __riscv_vector
#include <riscv_vector.h>
#endif

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "exif.hpp"

namespace cv {

// 0 = unknown
// 1 = milkv-duo
// 2 = milkv-duo256m
// 3 = licheerv-nano
// 4 = milkv-duos
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

        if (strncmp(buf, "Cvitek. CV180X ASIC. C906.", 36) == 0 || strncmp(buf, "Milk-V Duo", 10) == 0)
        {
            // milkv duo
            device_model = 1;
        }
        if (strncmp(buf, "Cvitek. CV181X ASIC. C906.", 36) == 0 || strncmp(buf, "Milk-V Duo256M", 14) == 0)
        {
            // milkv duo 256
            device_model = 2;
        }
        if (strncmp(buf, "LicheeRv Nano", 13) == 0)
        {
            // licheerv nano
            device_model = 3;
        }
        if (strncmp(buf, "Milk-V DuoS", 11) == 0)
        {
            // milkv duo s
            device_model = 4;
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
    if (device_model == 3)
    {
        // licheerv nano
        return true;
    }
    if (device_model == 4)
    {
        // milkv duo s
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
        fprintf(stderr, "this device is not whitelisted for jpeg decoder cvi\n");
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

typedef CVI_S32 VDEC_CHN;

typedef struct _VDEC_CHN_POOL_S {
    VB_POOL hPicVbPool; /* RW;  vb pool id for pic buffer */
    VB_POOL hTmvVbPool; /* RW;  vb pool id for tmv buffer */
} VDEC_CHN_POOL_S;

typedef enum {
    PT_JPEG          = 26
} PAYLOAD_TYPE_E;

typedef enum _VIDEO_MODE_E {
    VIDEO_MODE_STREAM = 0, /* send by stream */
    VIDEO_MODE_FRAME, /* send by frame  */
    VIDEO_MODE_COMPAT, /* One frame supports multiple packets sending. */
    /* The current frame is considered to end when bEndOfFrame is equal to HI_TRUE */
    VIDEO_MODE_BUTT
} VIDEO_MODE_E;

typedef struct _VDEC_ATTR_VIDEO_S {
    CVI_U32 u32RefFrameNum; /* RW, Range: [0, 16]; reference frame num. */
    CVI_BOOL bTemporalMvpEnable; /* RW; */
    /* specifies whether temporal motion vector predictors can be used for inter prediction */
    CVI_U32 u32TmvBufSize; /* RW; tmv buffer size(Byte) */
} VDEC_ATTR_VIDEO_S;

typedef struct _VDEC_CHN_ATTR_S {
    PAYLOAD_TYPE_E enType; /* RW; video type to be decoded   */
    VIDEO_MODE_E enMode; /* RW; send by stream or by frame */
    CVI_U32 u32PicWidth; /* RW; max pic width */
    CVI_U32 u32PicHeight; /* RW; max pic height */
    CVI_U32 u32StreamBufSize; /* RW; stream buffer size(Byte) */
    CVI_U32 u32FrameBufSize; /* RW; frame buffer size(Byte) */
    CVI_U32 u32FrameBufCnt;
    union {
        VDEC_ATTR_VIDEO_S stVdecVideoAttr; /* structure with video ( h264/h265) */
    };
} VDEC_CHN_ATTR_S;

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

typedef enum _VIDEO_DEC_MODE_E {
    VIDEO_DEC_MODE_IPB = 0,
    VIDEO_DEC_MODE_IP,
    VIDEO_DEC_MODE_I,
    VIDEO_DEC_MODE_BUTT
} VIDEO_DEC_MODE_E;

typedef enum _VIDEO_OUTPUT_ORDER_E {
    VIDEO_OUTPUT_ORDER_DISP = 0,
    VIDEO_OUTPUT_ORDER_DEC,
    VIDEO_OUTPUT_ORDER_BUTT
} VIDEO_OUTPUT_ORDER_E;

typedef enum _COMPRESS_MODE_E {
    COMPRESS_MODE_NONE = 0,
    COMPRESS_MODE_TILE,
    COMPRESS_MODE_LINE,
    COMPRESS_MODE_FRAME,
    COMPRESS_MODE_BUTT
} COMPRESS_MODE_E;

typedef enum _VIDEO_FORMAT_E {
    VIDEO_FORMAT_LINEAR = 0,
    VIDEO_FORMAT_MAX
} VIDEO_FORMAT_E;

typedef struct _VDEC_PARAM_VIDEO_S {
    CVI_S32 s32ErrThreshold; /* RW, Range: [0, 100]; */
    /* threshold for stream error process, 0: discard with any error, 100 : keep data with any error */
    VIDEO_DEC_MODE_E enDecMode; /* RW; */
    /* decode mode , 0: deocde IPB frames, 1: only decode I frame & P frame , 2: only decode I frame */
    VIDEO_OUTPUT_ORDER_E enOutputOrder; /* RW; */
    /* frames output order ,0: the same with display order , 1: the same width decoder order */
    COMPRESS_MODE_E enCompressMode; /* RW; compress mode */
    VIDEO_FORMAT_E enVideoFormat; /* RW; video format */
} VDEC_PARAM_VIDEO_S;

typedef struct _VDEC_PARAM_PICTURE_S {
    CVI_U32 u32Alpha; /* RW, Range: [0, 255]; value 0 is transparent. */
    /* [0 ,127]   is deemed to transparent when enPixelFormat is ARGB1555 or
    * ABGR1555 [128 ,256] is deemed to non-transparent when enPixelFormat is
    * ARGB1555 or ABGR1555
    */
} VDEC_PARAM_PICTURE_S;

typedef struct _VDEC_CHN_PARAM_S {
    PAYLOAD_TYPE_E enType; /* RW; video type to be decoded   */
    PIXEL_FORMAT_E enPixelFormat; /* RW; out put pixel format */
    CVI_U32 u32DisplayFrameNum; /* RW, Range: [0, 16]; display frame num */
    union {
        VDEC_PARAM_VIDEO_S stVdecVideoParam; /* structure with video ( h265/h264) */
        VDEC_PARAM_PICTURE_S stVdecPictureParam; /* structure with picture (jpeg/mjpeg ) */
    };
} VDEC_CHN_PARAM_S;

typedef enum _BAYER_FORMAT_E {
    BAYER_FORMAT_BG = 0,
    BAYER_FORMAT_GB,
    BAYER_FORMAT_GR,
    BAYER_FORMAT_RG,
    BAYER_FORMAT_MAX
} BAYER_FORMAT_E;

typedef enum _DYNAMIC_RANGE_E {
    DYNAMIC_RANGE_SDR8 = 0,
    DYNAMIC_RANGE_SDR10,
    DYNAMIC_RANGE_HDR10,
    DYNAMIC_RANGE_HLG,
    DYNAMIC_RANGE_SLF,
    DYNAMIC_RANGE_XDR,
    DYNAMIC_RANGE_MAX
} DYNAMIC_RANGE_E;

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

typedef enum _VB_SOURCE_E {
    VB_SOURCE_COMMON = 0,
    VB_SOURCE_MODULE = 1,
    VB_SOURCE_PRIVATE = 2,
    VB_SOURCE_USER = 3,
    VB_SOURCE_BUTT
} VB_SOURCE_E;

typedef struct _VDEC_VIDEO_MOD_PARAM_S {
    CVI_U32 u32MaxPicWidth;
    CVI_U32 u32MaxPicHeight;
    CVI_U32 u32MaxSliceNum;
    CVI_U32 u32VdhMsgNum;
    CVI_U32 u32VdhBinSize;
    CVI_U32 u32VdhExtMemLevel;
} VDEC_VIDEO_MOD_PARAM_S;

typedef enum _VDEC_CAPACITY_STRATEGY_E {
    VDEC_CAPACITY_STRATEGY_BY_MOD = 0,
    VDEC_CAPACITY_STRATEGY_BY_CHN = 1,
    VDEC_CAPACITY_STRATEGY_BUTT
} VDEC_CAPACITY_STRATEGY_E;

typedef struct _VDEC_PICTURE_MOD_PARAM_S {
    CVI_U32 u32MaxPicWidth;
    CVI_U32 u32MaxPicHeight;
    CVI_BOOL bSupportProgressive;
    CVI_BOOL bDynamicAllocate;
    VDEC_CAPACITY_STRATEGY_E enCapStrategy;
} VDEC_PICTURE_MOD_PARAM_S;

typedef struct _VDEC_MOD_PARAM_S {
    VB_SOURCE_E enVdecVBSource; /* RW, Range: [1, 3];  frame buffer mode  */
    CVI_U32 u32MiniBufMode; /* RW, Range: [0, 1];  stream buffer mode */
    CVI_U32 u32ParallelMode; /* RW, Range: [0, 1];  VDH working mode   */
    VDEC_VIDEO_MOD_PARAM_S stVideoModParam;
    VDEC_PICTURE_MOD_PARAM_S stPictureModParam;
} VDEC_MOD_PARAM_S;

typedef struct _VDEC_STREAM_S {
    CVI_U32 u32Len; /* W; stream len */
    CVI_U64 u64PTS; /* W; time stamp */
    CVI_BOOL bEndOfFrame; /* W; is the end of a frame */
    CVI_BOOL bEndOfStream; /* W; is the end of all stream */
    CVI_BOOL bDisplay; /* W; is the current frame displayed. only valid by VIDEO_MODE_FRAME */
    CVI_U8 *pu8Addr; /* W; stream address */
} VDEC_STREAM_S;

typedef CVI_S32 (*PFN_CVI_VDEC_AttachVbPool)(VDEC_CHN VdChn, const VDEC_CHN_POOL_S* pstPool);
typedef CVI_S32 (*PFN_CVI_VDEC_CreateChn)(VDEC_CHN VdChn, const VDEC_CHN_ATTR_S* pstAttr);
typedef CVI_S32 (*PFN_CVI_VDEC_DestroyChn)(VDEC_CHN VdChn);
typedef CVI_S32 (*PFN_CVI_VDEC_DetachVbPool)(VDEC_CHN VdChn);
typedef CVI_S32 (*PFN_CVI_VDEC_GetChnAttr)(VDEC_CHN VdChn, VDEC_CHN_ATTR_S* pstAttr);
typedef CVI_S32 (*PFN_CVI_VDEC_GetChnParam)(VDEC_CHN VdChn, VDEC_CHN_PARAM_S* pstParam);
typedef CVI_S32 (*PFN_CVI_VDEC_GetFrame)(VDEC_CHN VdChn, VIDEO_FRAME_INFO_S* pstFrameInfo, CVI_S32 s32MilliSec);
typedef CVI_S32 (*PFN_CVI_VDEC_GetModParam)(VDEC_MOD_PARAM_S* pstModParam);
typedef CVI_S32 (*PFN_CVI_VDEC_ReleaseFrame)(VDEC_CHN VdChn, const VIDEO_FRAME_INFO_S* pstFrameInfo);
typedef CVI_S32 (*PFN_CVI_VDEC_ResetChn)(VDEC_CHN VdChn);
typedef CVI_S32 (*PFN_CVI_VDEC_SendStream)(VDEC_CHN VdChn, const VDEC_STREAM_S* pstStream, CVI_S32 s32MilliSec);
typedef CVI_S32 (*PFN_CVI_VDEC_SetChnAttr)(VDEC_CHN VdChn, const VDEC_CHN_ATTR_S* pstAttr);
typedef CVI_S32 (*PFN_CVI_VDEC_SetChnParam)(VDEC_CHN VdChn, const VDEC_CHN_PARAM_S* pstParam);
typedef CVI_S32 (*PFN_CVI_VDEC_SetModParam)(const VDEC_MOD_PARAM_S* pstModParam);
typedef CVI_S32 (*PFN_CVI_VDEC_StartRecvStream)(VDEC_CHN VdChn);
typedef CVI_S32 (*PFN_CVI_VDEC_StopRecvStream)(VDEC_CHN VdChn);

}

static void* libvdec_sys = 0;
static void* libvdec = 0;

static PFN_CVI_VDEC_AttachVbPool CVI_VDEC_AttachVbPool = 0;
static PFN_CVI_VDEC_CreateChn CVI_VDEC_CreateChn = 0;
static PFN_CVI_VDEC_DestroyChn CVI_VDEC_DestroyChn = 0;
static PFN_CVI_VDEC_DetachVbPool CVI_VDEC_DetachVbPool = 0;
static PFN_CVI_VDEC_GetChnAttr CVI_VDEC_GetChnAttr = 0;
static PFN_CVI_VDEC_GetChnParam CVI_VDEC_GetChnParam = 0;
static PFN_CVI_VDEC_GetFrame CVI_VDEC_GetFrame = 0;
static PFN_CVI_VDEC_GetModParam CVI_VDEC_GetModParam = 0;
static PFN_CVI_VDEC_ReleaseFrame CVI_VDEC_ReleaseFrame = 0;
static PFN_CVI_VDEC_ResetChn CVI_VDEC_ResetChn = 0;
static PFN_CVI_VDEC_SendStream CVI_VDEC_SendStream = 0;
static PFN_CVI_VDEC_SetChnAttr CVI_VDEC_SetChnAttr = 0;
static PFN_CVI_VDEC_SetChnParam CVI_VDEC_SetChnParam = 0;
static PFN_CVI_VDEC_SetModParam CVI_VDEC_SetModParam = 0;
static PFN_CVI_VDEC_StartRecvStream CVI_VDEC_StartRecvStream = 0;
static PFN_CVI_VDEC_StopRecvStream CVI_VDEC_StopRecvStream = 0;

static int unload_vdec_library()
{
    if (libvdec_sys)
    {
        dlclose(libvdec_sys);
        libvdec_sys = 0;
    }

    if (libvdec)
    {
        dlclose(libvdec);
        libvdec = 0;
    }

    CVI_VDEC_AttachVbPool = 0;
    CVI_VDEC_CreateChn = 0;
    CVI_VDEC_DestroyChn = 0;
    CVI_VDEC_DetachVbPool = 0;
    CVI_VDEC_GetChnAttr = 0;
    CVI_VDEC_GetChnParam = 0;
    CVI_VDEC_GetFrame = 0;
    CVI_VDEC_GetModParam = 0;
    CVI_VDEC_ReleaseFrame = 0;
    CVI_VDEC_ResetChn = 0;
    CVI_VDEC_SendStream = 0;
    CVI_VDEC_SetChnAttr = 0;
    CVI_VDEC_SetChnParam = 0;
    CVI_VDEC_SetModParam = 0;
    CVI_VDEC_StartRecvStream = 0;
    CVI_VDEC_StopRecvStream = 0;

    return 0;
}

static int load_vdec_library()
{
    if (libvdec)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        fprintf(stderr, "this device is not whitelisted for jpeg decoder cvi\n");
        return -1;
    }

    libvdec_sys = dlopen("libsys.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libvdec_sys)
    {
        libvdec_sys = dlopen("/mnt/system/lib/libsys.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libvdec_sys)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    libvdec = dlopen("libvdec.so", RTLD_LOCAL | RTLD_NOW);
    if (!libvdec)
    {
        libvdec = dlopen("/mnt/system/lib/libvdec.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libvdec)
    {
        fprintf(stderr, "%s\n", dlerror());
        goto OUT;
    }

    CVI_VDEC_AttachVbPool = (PFN_CVI_VDEC_AttachVbPool)dlsym(libvdec, "CVI_VDEC_AttachVbPool");
    CVI_VDEC_CreateChn = (PFN_CVI_VDEC_CreateChn)dlsym(libvdec, "CVI_VDEC_CreateChn");
    CVI_VDEC_DestroyChn = (PFN_CVI_VDEC_DestroyChn)dlsym(libvdec, "CVI_VDEC_DestroyChn");
    CVI_VDEC_DetachVbPool = (PFN_CVI_VDEC_DetachVbPool)dlsym(libvdec, "CVI_VDEC_DetachVbPool");
    CVI_VDEC_GetChnAttr = (PFN_CVI_VDEC_GetChnAttr)dlsym(libvdec, "CVI_VDEC_GetChnAttr");
    CVI_VDEC_GetChnParam = (PFN_CVI_VDEC_GetChnParam)dlsym(libvdec, "CVI_VDEC_GetChnParam");
    CVI_VDEC_GetFrame = (PFN_CVI_VDEC_GetFrame)dlsym(libvdec, "CVI_VDEC_GetFrame");
    CVI_VDEC_GetModParam = (PFN_CVI_VDEC_GetModParam)dlsym(libvdec, "CVI_VDEC_GetModParam");
    CVI_VDEC_ReleaseFrame = (PFN_CVI_VDEC_ReleaseFrame)dlsym(libvdec, "CVI_VDEC_ReleaseFrame");
    CVI_VDEC_ResetChn = (PFN_CVI_VDEC_ResetChn)dlsym(libvdec, "CVI_VDEC_ResetChn");
    CVI_VDEC_SendStream = (PFN_CVI_VDEC_SendStream)dlsym(libvdec, "CVI_VDEC_SendStream");
    CVI_VDEC_SetChnAttr = (PFN_CVI_VDEC_SetChnAttr)dlsym(libvdec, "CVI_VDEC_SetChnAttr");
    CVI_VDEC_SetChnParam = (PFN_CVI_VDEC_SetChnParam)dlsym(libvdec, "CVI_VDEC_SetChnParam");
    CVI_VDEC_SetModParam = (PFN_CVI_VDEC_SetModParam)dlsym(libvdec, "CVI_VDEC_SetModParam");
    CVI_VDEC_StartRecvStream = (PFN_CVI_VDEC_StartRecvStream)dlsym(libvdec, "CVI_VDEC_StartRecvStream");
    CVI_VDEC_StopRecvStream = (PFN_CVI_VDEC_StopRecvStream)dlsym(libvdec, "CVI_VDEC_StopRecvStream");

    return 0;

OUT:
    unload_vdec_library();

    return -1;
}

class vdec_library_loader
{
public:
    bool ready;

    vdec_library_loader()
    {
        ready = (load_vdec_library() == 0);
    }

    ~vdec_library_loader()
    {
        unload_vdec_library();
    }
};

static vdec_library_loader vdec;


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

typedef struct _FRAME_RATE_CTRL_S {
    CVI_S32 s32SrcFrameRate; /* RW; source frame rate */
    CVI_S32 s32DstFrameRate; /* RW; dest frame rate */
} FRAME_RATE_CTRL_S;

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

typedef struct _RECT_S {
    CVI_S32 s32X;
    CVI_S32 s32Y;
    CVI_U32 u32Width;
    CVI_U32 u32Height;
} RECT_S;

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

typedef enum _ROTATION_E {
    ROTATION_0 = 0,
    ROTATION_90,
    ROTATION_180,
    ROTATION_270,
    ROTATION_XY_FLIP,
    ROTATION_MAX
} ROTATION_E;

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
typedef CVI_S32 (*PFN_CVI_VPSS_SetChnRotation)(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, ROTATION_E enRotation);
typedef CVI_S32 (*PFN_CVI_VPSS_StartGrp)(VPSS_GRP VpssGrp);
typedef CVI_S32 (*PFN_CVI_VPSS_StopGrp)(VPSS_GRP VpssGrp);

}

static void* libvpu_awb = 0;
static void* libvpu_ae = 0;
static void* libvpu_isp_algo = 0;
static void* libvpu_isp = 0;
static void* libvpu_cvi_bin_isp = 0;
static void* libvpu_cvi_bin = 0;
static void* libvpu = 0;

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
static PFN_CVI_VPSS_SetChnRotation CVI_VPSS_SetChnRotation = 0;
static PFN_CVI_VPSS_StartGrp CVI_VPSS_StartGrp = 0;
static PFN_CVI_VPSS_StopGrp CVI_VPSS_StopGrp = 0;

static int unload_vpu_library()
{
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
    CVI_VPSS_SetChnRotation = 0;
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
        fprintf(stderr, "this device is not whitelisted for jpeg decoder cvi\n");
        return -1;
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
    CVI_VPSS_SetChnRotation = (PFN_CVI_VPSS_SetChnRotation)dlsym(libvpu, "CVI_VPSS_SetChnRotation");
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


class jpeg_decoder_cvi_impl
{
public:
    jpeg_decoder_cvi_impl();
    ~jpeg_decoder_cvi_impl();

    int init(const unsigned char* jpgdata, int size, int* width, int* height, int* ch);

    int decode(const unsigned char* jpgdata, int size, unsigned char* outbgr) const;

    int deinit();

protected:
    int corrupted; // 0=fine
    int width;
    int height;
    int ch;
    int components; // 1=gray 3=yuv
    int sampling_factor; // 0=444 1=422h 2=422v 3=420 4=400
    int progressive;
    int orientation; // exif

    // for CVI_VDEC
    PIXEL_FORMAT_E yuv_pixel_format;
    CVI_U32 yuv_buffer_size;
    PIXEL_FORMAT_E yuv_rotated_pixel_format;
    CVI_U32 yuv_rotated_buffer_size;
    PIXEL_FORMAT_E bgr_pixel_format;
    CVI_U32 bgr_buffer_size;

    // hack for vdec/vpss alignment
    bool need_realign_uv;
};

jpeg_decoder_cvi_impl::jpeg_decoder_cvi_impl()
{
    corrupted = 1;
    width = 0;
    height = 0;
    ch = 0;
    components = 0;
    sampling_factor = -1;
    progressive = 0;
    orientation = -1;

    yuv_pixel_format = PIXEL_FORMAT_YUV_PLANAR_444;
    yuv_buffer_size = 0;
    yuv_rotated_pixel_format = PIXEL_FORMAT_NV12;
    yuv_rotated_buffer_size = 0;
    bgr_pixel_format = PIXEL_FORMAT_BGR_888;
    bgr_buffer_size = 0;

    need_realign_uv = false;
}

jpeg_decoder_cvi_impl::~jpeg_decoder_cvi_impl()
{
    deinit();
}

int jpeg_decoder_cvi_impl::init(const unsigned char* jpgdata, int jpgsize, int* _width, int* _height, int* _ch)
{
    if (!jpgdata || jpgsize < 4)
        return -1;

    // jpg magic
    if (jpgdata[0] != 0xFF || jpgdata[1] != 0xD8)
        return -1;

    // parse jpg for width height components sampling-factor progressive
    const unsigned char* pbuf = jpgdata;
    const unsigned char* pend = pbuf + jpgsize;
    while (pbuf + 1 < pend)
    {
        unsigned char marker0 = pbuf[0];
        unsigned char marker1 = pbuf[1];
        pbuf += 2;

        if (marker0 != 0xFF)
            break;

        // SOI EOI
        if (marker1 == 0xD8 || marker1 == 0xD9)
            continue;

        if (marker1 != 0xC0 && marker1 != 0xC2)
        {
            unsigned int skipsize = (pbuf[0] << 8) + pbuf[1];
            pbuf += skipsize;
            continue;
        }

        // SOF0 SOF2
        unsigned int skipsize = (pbuf[0] << 8) + pbuf[1];
        if (pbuf + skipsize > pend)
            break;

        // only 8bit supported
        if (pbuf[2] != 8)
            break;

        height = (pbuf[3] << 8) + pbuf[4];
        width = (pbuf[5] << 8) + pbuf[6];
        if (height == 0 || width == 0)
            break;

        components = pbuf[7];
        if (components != 1 && components != 3)
            break;

        pbuf += 8;

        unsigned char phv[3][2];
        for (int c = 0; c < components; c++)
        {
            unsigned char q = pbuf[1];
            phv[c][0] = (q >> 4); // 2 1 1   2 1 1   1 1 1   1 1 1
            phv[c][1] = (q & 15); // 2 1 1   1 1 1   2 1 1   1 1 1
            pbuf += 3;
        }

        if (components == 3 && phv[1][0] == 1 && phv[1][1] == 1 && phv[2][0] == 1 && phv[2][1] == 1)
        {
            if (phv[0][0] == 1 && phv[0][1] == 1) sampling_factor = 0;
            if (phv[0][0] == 2 && phv[0][1] == 1) sampling_factor = 1;
            if (phv[0][0] == 1 && phv[0][1] == 2) sampling_factor = 2;
            if (phv[0][0] == 2 && phv[0][1] == 2) sampling_factor = 3;
        }
        if (components == 1 && phv[0][0] == 1 && phv[0][1] == 1)
        {
            sampling_factor = 4;
        }

        // unsupported sampling factor
        if (sampling_factor == -1)
            break;

        // jpg is fine
        corrupted = 0;

        if (marker1 == 0xC2)
            progressive = 1;

        break;
    }

    // resolve exif orientation
    {
        std::string s((const char*)jpgdata, jpgsize);
        std::istringstream iss(s);

        cv::ExifReader exif_reader(iss);
        if (exif_reader.parse())
        {
            cv::ExifEntry_t e = exif_reader.getTag(cv::ORIENTATION);
            orientation = e.field_u16;
            if (orientation < 1 && orientation > 8)
                orientation = 1;
        }
    }
    // orientation = 7;

    if (corrupted)
        return -1;

    // progressive not supported
    if (progressive)
        return -1;

    // yuv422v not supported
    if (sampling_factor == 2)
        return -1;

    if (width % 2 != 0 || height % 2 != 0)
        return -1;

    if (width < 8 && height < 8)
        return -1;

    switch (sampling_factor)
    {
    case 0:
        yuv_pixel_format = PIXEL_FORMAT_YUV_PLANAR_444;
        yuv_buffer_size = ALIGN(width, 64) * ALIGN(height, 64) * 3;
        break;
    case 1:
        yuv_pixel_format = PIXEL_FORMAT_YUV_PLANAR_422;
        yuv_buffer_size = ALIGN(width, 128) * ALIGN(height, 64) * 2;
        break;
    case 2:
        yuv_pixel_format = PIXEL_FORMAT_YUV_PLANAR_422;
        yuv_buffer_size = ALIGN(width, 128) * ALIGN(height, 64) * 2;
        break;
    case 3:
        yuv_pixel_format = PIXEL_FORMAT_YUV_PLANAR_420;
        yuv_buffer_size = ALIGN(width, 128) * ALIGN(height, 64) * 3 / 2;
        break;
    case 4:
        yuv_pixel_format = PIXEL_FORMAT_YUV_400;
        yuv_rotated_pixel_format = PIXEL_FORMAT_YUV_400; // fast path for grayscale
        yuv_buffer_size = ALIGN(width, 64) * ALIGN(height, 64);
        break;
    default:
        // should never reach here
        break;
    }

    ch = *_ch;
    if (ch == 0)
        ch = components;

    if (ch == 1)
    {
        yuv_rotated_pixel_format = PIXEL_FORMAT_YUV_400; // fast path for grayscale
    }

    if (ch == 3 && (yuv_pixel_format == PIXEL_FORMAT_YUV_PLANAR_422 || yuv_pixel_format == PIXEL_FORMAT_YUV_PLANAR_420))
    {
        if (ALIGN(width, 64) != ALIGN(width, 128))
        {
            // vdec produce yuv422/yuv420 with width aligned to 64
            // but vpss need input width aligned to 128
            // we hack the frame metadata and move data, it works  -- nihui
            need_realign_uv = true;
        }
    }

    if (orientation > 4)
    {
        // swap width height
        int tmp = height;
        height = width;
        width = tmp;
    }

    if (orientation > 4)
    {
        // rotated NV12 buffer
        yuv_rotated_buffer_size = ALIGN(width, 64) * ALIGN(height, 64) * 3 / 2;
    }

    bgr_buffer_size = ALIGN(width, 64) * ALIGN(height, 64) * 3;

    *_width = width;
    *_height = height;
    *_ch = ch;

    // fprintf(stderr, "width = %d\n", width);
    // fprintf(stderr, "height = %d\n", height);
    // fprintf(stderr, "ch = %d\n", ch);
    // fprintf(stderr, "yuv_buffer_size = %d\n", yuv_buffer_size);
    // fprintf(stderr, "yuv_rotated_buffer_size = %d\n", yuv_rotated_buffer_size);
    // fprintf(stderr, "bgr_buffer_size = %d\n", bgr_buffer_size);
    // fprintf(stderr, "orientation = %d\n", orientation);
    // fprintf(stderr, "components = %d\n", components);
    // fprintf(stderr, "need_realign_uv = %d\n", need_realign_uv);

    return 0;
}

int jpeg_decoder_cvi_impl::decode(const unsigned char* jpgdata, int jpgsize, unsigned char* outbgr) const
{
    if (!outbgr)
        return -1;

    // corrupted file
    if (corrupted)
        return -1;

    // progressive not supported
    if (progressive)
        return -1;

    // yuv422v not supported
    if (sampling_factor == 2)
        return -1;

    // yuv444 rotate 90/270 not supported as nv12 color loss
    // if (sampling_factor == 0 && orientation > 4 && approx_yuv444_rotate == 0)
    //     return -1;

    if (width % 2 != 0 || height % 2 != 0)
        return -1;

    if (width < 8 && height < 8)
        return -1;

    // flag
    int ret_val = 0;
    int b_vb_inited = 0;
    int b_sys_inited = 0;

    // vb pool
    int b_vb_pool0_created = 0;
    int b_vb_pool1_created = 0;
    int b_vb_pool2_created = 0;
    VB_POOL VbPool0 = VB_INVALID_POOLID;
    VB_POOL VbPool1 = VB_INVALID_POOLID;
    VB_POOL VbPool2 = VB_INVALID_POOLID;

    // vdec
    int b_vdec_chn_created = 0;
    int b_vdec_vbpool_attached = 0;
    int b_vdec_recvstream_started = 0;
    int b_vdec_frame_got = 0;

    VDEC_CHN VdChn = 0;
    VIDEO_FRAME_INFO_S stFrameInfo;

    CVI_U64 phyaddr0_old = 0;
    CVI_U64 phyaddr1_old = 0;
    CVI_U64 phyaddr2_old = 0;

    // vpss rotate
    int b_vpss_rotate_grp_created = 0;
    int b_vpss_rotate_chn_enabled = 0;
    int b_vpss_rotate_vbpool_attached = 0;
    int b_vpss_rotate_grp_started = 0;
    int b_vpss_rotate_frame_got = 0;

    VPSS_GRP VpssRotateGrp = 1;
    // VPSS_GRP VpssGrp = CVI_VPSS_GetAvailableGrp();
    VPSS_CHN VpssRotateChn = VPSS_CHN0;
    VIDEO_FRAME_INFO_S stFrameInfo_rotated;

    // vpss bgr
    int b_vpss_bgr_grp_created = 0;
    int b_vpss_bgr_chn_enabled = 0;
    int b_vpss_bgr_vbpool_attached = 0;
    int b_vpss_bgr_grp_started = 0;
    int b_vpss_bgr_frame_got = 0;

    VPSS_GRP VpssRgbGrp = 0;
    // VPSS_GRP VpssGrp = CVI_VPSS_GetAvailableGrp();
    VPSS_CHN VpssRgbChn = VPSS_CHN0;
    VIDEO_FRAME_INFO_S stFrameInfo_bgr;


    // init vb pool
    {
        VB_CONFIG_S stVbConfig;
        stVbConfig.u32MaxPoolCnt = 1;
        stVbConfig.astCommPool[0].u32BlkSize = orientation > 4 ? yuv_rotated_buffer_size : 4096;
        stVbConfig.astCommPool[0].u32BlkCnt = 1;
        stVbConfig.astCommPool[0].enRemapMode = VB_REMAP_MODE_NONE;
        snprintf(stVbConfig.astCommPool[0].acName, MAX_VB_POOL_NAME_LEN, "cv-imread-jpg-comm0");

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

    // prepare vb pool
    {
        // create vb pool0
        {
            VB_POOL_CONFIG_S stVbPoolCfg;
            stVbPoolCfg.u32BlkSize = yuv_buffer_size;
            stVbPoolCfg.u32BlkCnt = 1;
            stVbPoolCfg.enRemapMode = VB_REMAP_MODE_NONE;
            snprintf(stVbPoolCfg.acName, MAX_VB_POOL_NAME_LEN, "cv-imread-jpg-%d-0", orientation);

            VbPool0 = CVI_VB_CreatePool(&stVbPoolCfg);
            if (VbPool0 == VB_INVALID_POOLID)
            {
                fprintf(stderr, "CVI_VB_CreatePool VbPool0 failed %d\n", VbPool0);
                ret_val = -1;
                goto OUT;
            }

            b_vb_pool0_created = 1;
        }

        // create vb pool1
        {
            VB_POOL_CONFIG_S stVbPoolCfg;
            stVbPoolCfg.u32BlkSize = orientation > 4 ? yuv_rotated_buffer_size : bgr_buffer_size;
            stVbPoolCfg.u32BlkCnt = 1;
            stVbPoolCfg.enRemapMode = VB_REMAP_MODE_NONE;
            snprintf(stVbPoolCfg.acName, MAX_VB_POOL_NAME_LEN, "cv-imread-jpg-%d-1", orientation);

            VbPool1 = CVI_VB_CreatePool(&stVbPoolCfg);
            if (VbPool1 == VB_INVALID_POOLID)
            {
                fprintf(stderr, "CVI_VB_CreatePool VbPool1 failed %d\n", VbPool1);
                ret_val = -1;
                goto OUT;
            }

            b_vb_pool1_created = 1;
        }

        // create vb pool2
        if (orientation > 4)
        {
            VB_POOL_CONFIG_S stVbPoolCfg;
            stVbPoolCfg.u32BlkSize = bgr_buffer_size;
            stVbPoolCfg.u32BlkCnt = 1;
            stVbPoolCfg.enRemapMode = VB_REMAP_MODE_NONE;
            snprintf(stVbPoolCfg.acName, MAX_VB_POOL_NAME_LEN, "cv-imread-jpg-%d-1", orientation);

            VbPool2 = CVI_VB_CreatePool(&stVbPoolCfg);
            if (VbPool2 == VB_INVALID_POOLID)
            {
                fprintf(stderr, "CVI_VB_CreatePool VbPool2 failed %d\n", VbPool2);
                ret_val = -1;
                goto OUT;
            }

            b_vb_pool2_created = 1;
        }
    }

    // prepare vdec
    {
        // vdec create chn
        {
            VDEC_CHN_ATTR_S stAttr;
            stAttr.enType = PT_JPEG;
            stAttr.enMode = VIDEO_MODE_FRAME;
            stAttr.u32PicWidth = orientation > 4 ? height : width;
            stAttr.u32PicHeight = orientation > 4 ? width : height;
            stAttr.u32StreamBufSize = ALIGN(width * height, 0x4000);
            stAttr.u32FrameBufSize = yuv_buffer_size;
            stAttr.u32FrameBufCnt = 1;
            stAttr.stVdecVideoAttr.u32RefFrameNum = 0;
            stAttr.stVdecVideoAttr.bTemporalMvpEnable = CVI_FALSE;
            stAttr.stVdecVideoAttr.u32TmvBufSize = 0;

            CVI_S32 ret = CVI_VDEC_CreateChn(VdChn, &stAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VDEC_CreateChn failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vdec_chn_created = 1;
        }

        // vdec set mod param
        {
            VDEC_MOD_PARAM_S stModParam;
            CVI_S32 ret = CVI_VDEC_GetModParam(&stModParam);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VDEC_GetModParam failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            stModParam.enVdecVBSource = VB_SOURCE_USER;

            ret = CVI_VDEC_SetModParam(&stModParam);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VDEC_SetModParam failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        // vdec set chn param
        {
            VDEC_CHN_PARAM_S stParam;
            CVI_S32 ret = CVI_VDEC_GetChnParam(VdChn, &stParam);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VDEC_GetChnParam failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            stParam.stVdecPictureParam.u32Alpha = 255;
            stParam.enPixelFormat = yuv_pixel_format;
            stParam.u32DisplayFrameNum = 0;

            ret = CVI_VDEC_SetChnParam(VdChn, &stParam);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VDEC_SetChnParam failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        // vdec attach vbpool
        {
            VDEC_CHN_POOL_S stPool;
            stPool.hPicVbPool = VbPool0;
            stPool.hTmvVbPool = VB_INVALID_POOLID;

            CVI_S32 ret = CVI_VDEC_AttachVbPool(VdChn, &stPool);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VDEC_AttachVbPool failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vdec_vbpool_attached = 1;
        }
    }

    // prepare vpss rotate
    if (orientation > 4)
    {
        // vpss create grp
        {
            VPSS_GRP_ATTR_S stGrpAttr;
            stGrpAttr.u32MaxW = height;
            stGrpAttr.u32MaxH = width;
            // stGrpAttr.enPixelFormat = yuv_pixel_format;
            stGrpAttr.enPixelFormat = ch == 1 ? PIXEL_FORMAT_YUV_400 : yuv_pixel_format;
            stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
            stGrpAttr.stFrameRate.s32DstFrameRate = -1;
            stGrpAttr.u8VpssDev = 0;

            CVI_S32 ret = CVI_VPSS_CreateGrp(VpssRotateGrp, &stGrpAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_CreateGrp failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vpss_rotate_grp_created = 1;
        }

        // vpss set chn attr
        {
            CVI_BOOL need_mirror = CVI_FALSE;
            CVI_BOOL need_flip = CVI_FALSE;
            if (orientation == 5 || orientation == 7)
            {
                need_mirror = CVI_TRUE;
            }

            VPSS_CHN_ATTR_S stChnAttr;
            stChnAttr.u32Width = height;
            stChnAttr.u32Height = width;
            stChnAttr.enVideoFormat = VIDEO_FORMAT_LINEAR;
            stChnAttr.enPixelFormat = yuv_rotated_pixel_format;
            stChnAttr.stFrameRate.s32SrcFrameRate = -1;
            stChnAttr.stFrameRate.s32DstFrameRate = -1;
            stChnAttr.bMirror = need_mirror;
            stChnAttr.bFlip = need_flip;
            stChnAttr.u32Depth = 1;
            stChnAttr.stAspectRatio.enMode = ASPECT_RATIO_NONE;
            stChnAttr.stAspectRatio.bEnableBgColor = CVI_FALSE;
            stChnAttr.stAspectRatio.u32BgColor = 0;
            stChnAttr.stAspectRatio.stVideoRect.s32X = 0;
            stChnAttr.stAspectRatio.stVideoRect.s32Y = 0;
            stChnAttr.stAspectRatio.stVideoRect.u32Width = height;
            stChnAttr.stAspectRatio.stVideoRect.u32Height = width;
            stChnAttr.stNormalize.bEnable = CVI_FALSE;
            stChnAttr.stNormalize.factor[0] = 0.f;
            stChnAttr.stNormalize.factor[1] = 0.f;
            stChnAttr.stNormalize.factor[2] = 0.f;
            stChnAttr.stNormalize.mean[0] = 0.f;
            stChnAttr.stNormalize.mean[1] = 0.f;
            stChnAttr.stNormalize.mean[2] = 0.f;
            stChnAttr.stNormalize.rounding = VPSS_ROUNDING_TO_EVEN;

            CVI_S32 ret = CVI_VPSS_SetChnAttr(VpssRotateGrp, VpssRotateChn, &stChnAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_SetChnAttr failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        // vpss rotate
        {
            ROTATION_E rotation = ROTATION_0;
            if (orientation == 5 || orientation == 6)
            {
                rotation = ROTATION_270;
            }
            if (orientation == 7 || orientation == 8)
            {
                rotation = ROTATION_90;
            }

            CVI_S32 ret = CVI_VPSS_SetChnRotation(VpssRotateGrp, VpssRotateChn, rotation);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_SetChnRotation %d failed %x\n", rotation, ret);
                ret_val = -1;
                goto OUT;
            }
        }

        // vpss enable chn
        {
            CVI_S32 ret = CVI_VPSS_EnableChn(VpssRotateGrp, VpssRotateChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_EnableChn failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vpss_rotate_chn_enabled = 1;
        }

        {
            CVI_S32 ret = CVI_VPSS_DetachVbPool(VpssRotateGrp, VpssRotateChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_DetachVbPool failed %x\n", ret);
                ret_val = -1;
            }
        }

        // vpss attach vb pool
        {
            CVI_S32 ret = CVI_VPSS_AttachVbPool(VpssRotateGrp, VpssRotateChn, VbPool1);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_AttachVbPool failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vpss_rotate_vbpool_attached = 1;
        }
    }

    // prepare vpss bgr
    if (components == 3 && ch == 3)
    {
        // vpss create grp
        {
            VPSS_GRP_ATTR_S stGrpAttr;
            stGrpAttr.u32MaxW = width;
            stGrpAttr.u32MaxH = height;
            stGrpAttr.enPixelFormat = orientation > 4 ? yuv_rotated_pixel_format : yuv_pixel_format;
            stGrpAttr.stFrameRate.s32SrcFrameRate = -1;
            stGrpAttr.stFrameRate.s32DstFrameRate = -1;
            stGrpAttr.u8VpssDev = 0;

            CVI_S32 ret = CVI_VPSS_CreateGrp(VpssRgbGrp, &stGrpAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_CreateGrp failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vpss_bgr_grp_created = 1;
        }

        // vpss set chn attr
        {
            CVI_BOOL need_mirror = CVI_FALSE;
            CVI_BOOL need_flip = CVI_FALSE;
            if (orientation == 2 || orientation == 3)
            {
                need_mirror = CVI_TRUE;
            }
            if (orientation == 3 || orientation == 4)
            {
                need_flip = CVI_TRUE;
            }

            VPSS_CHN_ATTR_S stChnAttr;
            stChnAttr.u32Width = width;
            stChnAttr.u32Height = height;
            stChnAttr.enVideoFormat = VIDEO_FORMAT_LINEAR;
            stChnAttr.enPixelFormat = bgr_pixel_format;
            stChnAttr.stFrameRate.s32SrcFrameRate = -1;
            stChnAttr.stFrameRate.s32DstFrameRate = -1;
            stChnAttr.bMirror = need_mirror;
            stChnAttr.bFlip = need_flip;
            stChnAttr.u32Depth = 1;
            stChnAttr.stAspectRatio.enMode = ASPECT_RATIO_NONE;
            stChnAttr.stAspectRatio.bEnableBgColor = CVI_FALSE;
            stChnAttr.stAspectRatio.u32BgColor = 0;
            stChnAttr.stAspectRatio.stVideoRect.s32X = 0;
            stChnAttr.stAspectRatio.stVideoRect.s32Y = 0;
            stChnAttr.stAspectRatio.stVideoRect.u32Width = width;
            stChnAttr.stAspectRatio.stVideoRect.u32Height = height;
            stChnAttr.stNormalize.bEnable = CVI_FALSE;
            stChnAttr.stNormalize.factor[0] = 0.f;
            stChnAttr.stNormalize.factor[1] = 0.f;
            stChnAttr.stNormalize.factor[2] = 0.f;
            stChnAttr.stNormalize.mean[0] = 0.f;
            stChnAttr.stNormalize.mean[1] = 0.f;
            stChnAttr.stNormalize.mean[2] = 0.f;
            stChnAttr.stNormalize.rounding = VPSS_ROUNDING_TO_EVEN;

            CVI_S32 ret = CVI_VPSS_SetChnAttr(VpssRgbGrp, VpssRgbChn, &stChnAttr);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_SetChnAttr failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        // vpss enable chn
        {
            CVI_S32 ret = CVI_VPSS_EnableChn(VpssRgbGrp, VpssRgbChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_EnableChn failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vpss_bgr_chn_enabled = 1;
        }

        // vpss attach vb pool
        {
            CVI_S32 ret = CVI_VPSS_AttachVbPool(VpssRgbGrp, VpssRgbChn, orientation > 4 ? VbPool2 : VbPool1);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_AttachVbPool failed %x\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_vpss_bgr_vbpool_attached = 1;
        }
    }

    // vdec start recv stream
    {
        CVI_S32 ret = CVI_VDEC_StartRecvStream(VdChn);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VDEC_StartRecvStream failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vdec_recvstream_started = 1;
    }

    // vpss rotate start grp
    if (orientation > 4)
    {
        CVI_S32 ret = CVI_VPSS_StartGrp(VpssRotateGrp);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_StartGrp failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vpss_rotate_grp_started = 1;
    }

    // vpss bgr start grp
    if (components == 3 && ch == 3)
    {
        CVI_S32 ret = CVI_VPSS_StartGrp(VpssRgbGrp);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_StartGrp failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vpss_bgr_grp_started = 1;
    }

    // vdec send stream
    {
        VDEC_STREAM_S stStream;
        stStream.u32Len = jpgsize;
        stStream.u64PTS = 0;
        stStream.bEndOfFrame = CVI_TRUE;
        stStream.bEndOfStream = CVI_TRUE;
        stStream.bDisplay = 1;
        stStream.pu8Addr = (unsigned char*)jpgdata;

        CVI_S32 ret = CVI_VDEC_SendStream(VdChn, &stStream, -1);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VDEC_SendStream failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    // vdec get frame
    {
        CVI_S32 ret = CVI_VDEC_GetFrame(VdChn, &stFrameInfo, -1);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VDEC_GetFrame failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vdec_frame_got = 1;

        // fprintf(stderr, "stFrameInfo %d   %d x %d  %d\n", stFrameInfo.u32PoolId, stFrameInfo.stVFrame.u32Width, stFrameInfo.stVFrame.u32Height, stFrameInfo.stVFrame.enPixelFormat);

        if (stFrameInfo.stVFrame.enPixelFormat != yuv_pixel_format)
        {
            fprintf(stderr, "unsupported stFrameInfo pixel format %d\n", stFrameInfo.stVFrame.enPixelFormat);
            ret_val = -1;
            goto OUT;
        }
    }

    if (need_realign_uv)
    {
        // save phyaddr metadata
        VIDEO_FRAME_S& vf = stFrameInfo.stVFrame;
        phyaddr0_old = vf.u64PhyAddr[0];
        phyaddr1_old = vf.u64PhyAddr[1];
        phyaddr2_old = vf.u64PhyAddr[2];

        // make width to be multiple of 128
        if (yuv_pixel_format == PIXEL_FORMAT_YUV_PLANAR_422)
        {
            int aligned_width = orientation > 4 ? ALIGN(height, 64) : ALIGN(width, 64);
            int aligned_height = orientation > 4 ? ALIGN(width, 64) : ALIGN(height, 64);
            int new_aligned_width = orientation > 4 ? ALIGN(height, 128) : ALIGN(width, 128);

            CVI_U64 phyaddr0 = vf.u64PhyAddr[0];
            CVI_U64 phyaddr1 = vf.u64PhyAddr[1];
            CVI_U64 phyaddr2 = vf.u64PhyAddr[2];
            void* mapped_ptr0 = CVI_SYS_MmapCache(phyaddr0, yuv_buffer_size);
            void* mapped_ptr1 = CVI_SYS_MmapCache(phyaddr1, new_aligned_width * aligned_height / 2);
            void* mapped_ptr2 = CVI_SYS_MmapCache(phyaddr2, new_aligned_width * aligned_height / 2);
            // CVI_SYS_IonInvalidateCache(phyaddr0, mapped_ptr0, yuv_buffer_size);
            // CVI_SYS_IonInvalidateCache(phyaddr1, mapped_ptr1, new_aligned_width * aligned_height / 2);
            // CVI_SYS_IonInvalidateCache(phyaddr2, mapped_ptr2, new_aligned_width * aligned_height / 2);

            const unsigned char* y = (unsigned char*)mapped_ptr0;
            const unsigned char* u = (unsigned char*)mapped_ptr1;
            const unsigned char* v = (unsigned char*)mapped_ptr2;

            unsigned char* y2 = (unsigned char*)mapped_ptr0;
            unsigned char* u2 = y2 + new_aligned_width * aligned_height;
            unsigned char* v2 = u2 + new_aligned_width * aligned_height / 2;

            if ((int)(phyaddr2 - phyaddr0) <= new_aligned_width * aligned_height + new_aligned_width * aligned_height / 2)
            {
                // move v to later
                for (int i = aligned_height - 1; i >= 0; i--)
                {
                    memmove(v2 + i * new_aligned_width / 2, v + i * aligned_width / 2, aligned_width / 2);
                }

                if ((int)(phyaddr1 - phyaddr0) <= new_aligned_width * aligned_height)
                {
                    // move u to later
                    for (int i = aligned_height - 1; i >= 0; i--)
                    {
                        memmove(u2 + i * new_aligned_width / 2, u + i * aligned_width / 2, aligned_width / 2);
                    }
                }
                else
                {
                    // move u to advance
                    for (int i = 0; i < aligned_height; i++)
                    {
                        memmove(u2 + i * new_aligned_width / 2, u + i * aligned_width / 2, aligned_width / 2);
                    }
                }
            }
            else
            {
                if ((int)(phyaddr1 - phyaddr0) <= new_aligned_width * aligned_height)
                {
                    // move u to later
                    for (int i = aligned_height - 1; i >= 0; i--)
                    {
                        memmove(u2 + i * new_aligned_width / 2, u + i * aligned_width / 2, aligned_width / 2);
                    }
                }
                else
                {
                    // move u to advance
                    for (int i = 0; i < aligned_height; i++)
                    {
                        memmove(u2 + i * new_aligned_width / 2, u + i * aligned_width / 2, aligned_width / 2);
                    }
                }

                // move v to advance
                for (int i = 0; i < aligned_height; i++)
                {
                    memmove(v2 + i * new_aligned_width / 2, v + i * aligned_width / 2, aligned_width / 2);
                }
            }

            // move y
            for (int i = aligned_height - 1; i >= 0; i--)
            {
                memmove(y2 + i * new_aligned_width, y + i * aligned_width, aligned_width);
            }

            CVI_SYS_IonFlushCache(phyaddr0, mapped_ptr0, yuv_buffer_size);
            CVI_SYS_Munmap(mapped_ptr0, yuv_buffer_size);
            CVI_SYS_Munmap(mapped_ptr1, new_aligned_width * aligned_height / 2);
            CVI_SYS_Munmap(mapped_ptr2, new_aligned_width * aligned_height / 2);

            vf.u64PhyAddr[1] = vf.u64PhyAddr[0] + new_aligned_width * aligned_height;
            vf.u64PhyAddr[2] = vf.u64PhyAddr[1] + new_aligned_width * aligned_height / 2;

            vf.u32Stride[0] = new_aligned_width;
            vf.u32Stride[1] = new_aligned_width / 2;
            vf.u32Stride[2] = new_aligned_width / 2;

            vf.u32Length[0] = new_aligned_width * aligned_height;
            vf.u32Length[1] = new_aligned_width * aligned_height / 2;
            vf.u32Length[2] = new_aligned_width * aligned_height / 2;
        }
        if (yuv_pixel_format == PIXEL_FORMAT_YUV_PLANAR_420)
        {
            int aligned_width = orientation > 4 ? ALIGN(height, 64) : ALIGN(width, 64);
            int aligned_height = orientation > 4 ? ALIGN(width, 64) : ALIGN(height, 64);
            int new_aligned_width = orientation > 4 ? ALIGN(height, 128) : ALIGN(width, 128);

            CVI_U64 phyaddr0 = vf.u64PhyAddr[0];
            CVI_U64 phyaddr1 = vf.u64PhyAddr[1];
            CVI_U64 phyaddr2 = vf.u64PhyAddr[2];
            void* mapped_ptr0 = CVI_SYS_MmapCache(phyaddr0, yuv_buffer_size);
            void* mapped_ptr1 = CVI_SYS_MmapCache(phyaddr1, new_aligned_width * aligned_height / 4);
            void* mapped_ptr2 = CVI_SYS_MmapCache(phyaddr2, new_aligned_width * aligned_height / 4);
            // CVI_SYS_IonInvalidateCache(phyaddr0, mapped_ptr0, yuv_buffer_size);
            // CVI_SYS_IonInvalidateCache(phyaddr1, mapped_ptr1, new_aligned_width * aligned_height / 4);
            // CVI_SYS_IonInvalidateCache(phyaddr2, mapped_ptr2, new_aligned_width * aligned_height / 4);

            const unsigned char* y = (unsigned char*)mapped_ptr0;
            const unsigned char* u = (unsigned char*)mapped_ptr1;
            const unsigned char* v = (unsigned char*)mapped_ptr2;

            unsigned char* y2 = (unsigned char*)mapped_ptr0;
            unsigned char* u2 = y2 + new_aligned_width * aligned_height;
            unsigned char* v2 = u2 + new_aligned_width * aligned_height / 4;

            if ((int)(phyaddr2 - phyaddr0) <= new_aligned_width * aligned_height + new_aligned_width * aligned_height / 4)
            {
                // move v to later
                for (int i = aligned_height / 2 - 1; i >= 0; i--)
                {
                    memmove(v2 + i * new_aligned_width / 2, v + i * aligned_width / 2, aligned_width / 2);
                }

                if ((int)(phyaddr1 - phyaddr0) <= new_aligned_width * aligned_height)
                {
                    // move u to later
                    for (int i = aligned_height / 2 - 1; i >= 0; i--)
                    {
                        memmove(u2 + i * new_aligned_width / 2, u + i * aligned_width / 2, aligned_width / 2);
                    }
                }
                else
                {
                    // move u to advance
                    for (int i = 0; i < aligned_height / 2; i++)
                    {
                        memmove(u2 + i * new_aligned_width / 2, u + i * aligned_width / 2, aligned_width / 2);
                    }
                }
            }
            else
            {
                if ((int)(phyaddr1 - phyaddr0) <= new_aligned_width * aligned_height)
                {
                    // move u to later
                    for (int i = aligned_height / 2 - 1; i >= 0; i--)
                    {
                        memmove(u2 + i * new_aligned_width / 2, u + i * aligned_width / 2, aligned_width / 2);
                    }
                }
                else
                {
                    // move u to advance
                    for (int i = 0; i < aligned_height / 2; i++)
                    {
                        memmove(u2 + i * new_aligned_width / 2, u + i * aligned_width / 2, aligned_width / 2);
                    }
                }

                // move v to advance
                for (int i = 0; i < aligned_height / 2; i++)
                {
                    memmove(v2 + i * new_aligned_width / 2, v + i * aligned_width / 2, aligned_width / 2);
                }
            }

            // move y
            for (int i = aligned_height - 1; i >= 0; i--)
            {
                memmove(y2 + i * new_aligned_width, y + i * aligned_width, aligned_width);
            }

            CVI_SYS_IonFlushCache(phyaddr0, mapped_ptr0, yuv_buffer_size);
            CVI_SYS_Munmap(mapped_ptr0, yuv_buffer_size);
            CVI_SYS_Munmap(mapped_ptr1, new_aligned_width * aligned_height / 4);
            CVI_SYS_Munmap(mapped_ptr2, new_aligned_width * aligned_height / 4);

            vf.u64PhyAddr[1] = vf.u64PhyAddr[0] + new_aligned_width * aligned_height;
            vf.u64PhyAddr[2] = vf.u64PhyAddr[1] + new_aligned_width * aligned_height / 4;

            vf.u32Stride[0] = new_aligned_width;
            vf.u32Stride[1] = new_aligned_width / 2;
            vf.u32Stride[2] = new_aligned_width / 2;

            vf.u32Length[0] = new_aligned_width * aligned_height;
            vf.u32Length[1] = new_aligned_width * aligned_height / 4;
            vf.u32Length[2] = new_aligned_width * aligned_height / 4;
        }
    }

    // vpss rotate send frame
    if (orientation > 4)
    {
        if (ch == 1)
        {
            VIDEO_FRAME_S& vf = stFrameInfo.stVFrame;
            vf.enPixelFormat = PIXEL_FORMAT_YUV_400;
        }

        CVI_S32 ret = CVI_VPSS_SendFrame(VpssRotateGrp, &stFrameInfo, -1);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_SendFrame rotate failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    // vpss rotate get frame
    if (orientation > 4)
    {
        CVI_S32 ret = CVI_VPSS_GetChnFrame(VpssRotateGrp, VpssRotateChn, &stFrameInfo_rotated, 2000);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_GetChnFrame rotate failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vpss_rotate_frame_got = 1;

        // fprintf(stderr, "stFrameInfo_rotated %d   %d x %d  %d\n", stFrameInfo_rotated.u32PoolId, stFrameInfo_rotated.stVFrame.u32Width, stFrameInfo_rotated.stVFrame.u32Height, stFrameInfo_rotated.stVFrame.enPixelFormat);

        if (stFrameInfo_rotated.stVFrame.enPixelFormat != yuv_rotated_pixel_format)
        {
            fprintf(stderr, "unsupported stFrameInfo_rotated pixel format %d\n", stFrameInfo_rotated.stVFrame.enPixelFormat);
            ret_val = -1;
            goto OUT;
        }
    }

    if (orientation > 4)
    {
        // because vpss rotate pad garbage
        VIDEO_FRAME_S& vf = stFrameInfo_rotated.stVFrame;

        vf.u32Width = width;
        vf.u32Height = height;
    }

    // vpss bgr send frame
    if (components == 3 && ch == 3)
    {
        CVI_S32 ret = CVI_VPSS_SendFrame(VpssRgbGrp, orientation > 4 ? &stFrameInfo_rotated : &stFrameInfo, -1);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_SendFrame bgr failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    // vpss bgr get frame
    if (components == 3 && ch == 3)
    {
        CVI_S32 ret = CVI_VPSS_GetChnFrame(VpssRgbGrp, VpssRgbChn, &stFrameInfo_bgr, 2000);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_GetChnFrame bgr failed %x\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_vpss_bgr_frame_got = 1;

        // fprintf(stderr, "stFrameInfo_bgr %d   %d x %d  %d\n", stFrameInfo_bgr.u32PoolId, stFrameInfo_bgr.stVFrame.u32Width, stFrameInfo_bgr.stVFrame.u32Height, stFrameInfo_bgr.stVFrame.enPixelFormat);

        if (stFrameInfo_bgr.stVFrame.enPixelFormat != bgr_pixel_format)
        {
            fprintf(stderr, "unsupported stFrameInfo_bgr pixel format %d\n", stFrameInfo_bgr.stVFrame.enPixelFormat);
            ret_val = -1;
            goto OUT;
        }
    }

    if (components == 3 || (components == 1 && ch == 1))
    {
        const VIDEO_FRAME_S& vf = (ch == 1) ? (orientation > 4 ? stFrameInfo_rotated.stVFrame : stFrameInfo.stVFrame) : stFrameInfo_bgr.stVFrame;

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

        int h2 = height;
        int w2 = width * ch;
        if (stride == width * ch)
        {
            h2 = 1;
            w2 = height * width * ch;
        }

        for (int i = 0; i < h2; i++)
        {
#if __riscv_vector
            int j = 0;
            int n = w2;
            while (n > 0) {
                size_t vl = vsetvl_e8m8(n);
                vuint8m8_t bgr = vle8_v_u8m8(ptr + j, vl);
                vse8_v_u8m8(outbgr, bgr, vl);
                outbgr += vl;
                j += vl;
                n -= vl;
            }
#else
            memcpy(outbgr, ptr, w2);
            outbgr += w2;
#endif

            ptr += stride;
        }

        CVI_SYS_Munmap(mapped_ptr, length);
    }
    else // if (components == 1 && ch == 3)
    {
        const VIDEO_FRAME_S& vf = (orientation > 4) ? stFrameInfo_rotated.stVFrame : stFrameInfo.stVFrame;

        // repeat Y for BGR
        CVI_U64 phyaddr = vf.u64PhyAddr[0];
        // const unsigned char* bgr_ptr = vf.pu8VirAddr[0];
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

        for (int i = 0; i < height; i++)
        {
#if __riscv_vector
            int j = 0;
            int n = width;
            while (n > 0) {
                size_t vl = vsetvl_e8m2(n);
                vuint8m2_t g = vle8_v_u8m2(ptr + j, vl);
                vuint8m2x3_t o = vcreate_u8m2x3(g, g, g);
                vsseg3e8_v_u8m2x3(outbgr, o, vl);
                outbgr += vl * 3;
                j += vl;
                n -= vl;
            }
#else
            for (int j = 0; j < width; j++)
            {
                outbgr[0] = ptr[j];
                outbgr[1] = ptr[j];
                outbgr[2] = ptr[j];
                outbgr += 3;
            }
#endif

            ptr += stride;
        }

        CVI_SYS_Munmap(mapped_ptr, length);
    }

    if (need_realign_uv)
    {
        // restore phyaddr metadata
        VIDEO_FRAME_S& vf = stFrameInfo.stVFrame;
        vf.u64PhyAddr[0] = phyaddr0_old;
        vf.u64PhyAddr[1] = phyaddr1_old;
        vf.u64PhyAddr[2] = phyaddr2_old;
    }

    if (orientation > 4)
    {
        if (ch == 1)
        {
            VIDEO_FRAME_S& vf = stFrameInfo.stVFrame;
            vf.enPixelFormat = yuv_pixel_format;
        }
    }

OUT:

    if (b_vpss_bgr_frame_got)
    {
        CVI_S32 ret = CVI_VPSS_ReleaseChnFrame(VpssRgbGrp, VpssRgbChn, &stFrameInfo_bgr);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_ReleaseChnFrame failed %x\n", ret);
            ret_val = -1;
        }
    }

    if (b_vpss_rotate_frame_got)
    {
        CVI_S32 ret = CVI_VPSS_ReleaseChnFrame(VpssRotateGrp, VpssRotateChn, &stFrameInfo_rotated);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_ReleaseChnFrame failed %x\n", ret);
            ret_val = -1;
        }
    }

    if (b_vdec_frame_got)
    {
        CVI_S32 ret = CVI_VDEC_ReleaseFrame(VdChn, &stFrameInfo);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VDEC_ReleaseFrame failed %x\n", ret);
            ret_val = -1;
        }
    }

    if (b_vpss_bgr_grp_started)
    {
        CVI_S32 ret = CVI_VPSS_StopGrp(VpssRgbGrp);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_StopGrp failed %x\n", ret);
            ret_val = -1;
        }
    }

    if (b_vpss_rotate_grp_started)
    {
        CVI_S32 ret = CVI_VPSS_StopGrp(VpssRotateGrp);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VPSS_StopGrp failed %x\n", ret);
            ret_val = -1;
        }
    }

    if (b_vdec_recvstream_started)
    {
        CVI_S32 ret = CVI_VDEC_StopRecvStream(VdChn);
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VDEC_StopRecvStream failed %x\n", ret);
            ret_val = -1;
        }
    }

    // vpss bgr exit
    {
        if (b_vpss_bgr_vbpool_attached)
        {
            CVI_S32 ret = CVI_VPSS_DetachVbPool(VpssRgbGrp, VpssRgbChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_DetachVbPool failed %x\n", ret);
                ret_val = -1;
            }
        }

        if (b_vpss_bgr_chn_enabled)
        {
            CVI_S32 ret = CVI_VPSS_DisableChn(VpssRgbGrp, VpssRgbChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_DisableChn failed %x\n", ret);
                ret_val = -1;
            }
        }

        if (b_vpss_bgr_grp_created)
        {
            CVI_S32 ret = CVI_VPSS_DestroyGrp(VpssRgbGrp);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_DestroyGrp failed %x\n", ret);
                ret_val = -1;
            }
        }
    }

    // vpss rotate exit
    {
        if (b_vpss_rotate_vbpool_attached)
        {
            CVI_S32 ret = CVI_VPSS_DetachVbPool(VpssRotateGrp, VpssRotateChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_DetachVbPool failed %x\n", ret);
                ret_val = -1;
            }
        }

        if (b_vpss_rotate_chn_enabled)
        {
            CVI_S32 ret = CVI_VPSS_DisableChn(VpssRotateGrp, VpssRotateChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_DisableChn failed %x\n", ret);
                ret_val = -1;
            }
        }

        if (b_vpss_rotate_grp_created)
        {
            CVI_S32 ret = CVI_VPSS_DestroyGrp(VpssRotateGrp);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VPSS_DestroyGrp failed %x\n", ret);
                ret_val = -1;
            }
        }
    }

    // vdec exit
    {
        if (b_vdec_vbpool_attached)
        {
            CVI_S32 ret = CVI_VDEC_DetachVbPool(VdChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VDEC_DetachVbPool failed %x\n", ret);
                ret_val = -1;
            }
        }

        if (b_vdec_chn_created)
        {
            CVI_S32 ret = CVI_VDEC_ResetChn(VdChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VDEC_DestroyChn failed %x\n", ret);
                ret_val = -1;
            }
        }

        if (b_vdec_chn_created)
        {
            CVI_S32 ret = CVI_VDEC_DestroyChn(VdChn);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VDEC_DestroyChn failed %x\n", ret);
                ret_val = -1;
            }
        }
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
        }

        if (b_vb_pool1_created)
        {
            CVI_S32 ret = CVI_VB_DestroyPool(VbPool1);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VB_DestroyPool failed %x\n", ret);
                ret_val = -1;
            }
        }

        if (b_vb_pool2_created)
        {
            CVI_S32 ret = CVI_VB_DestroyPool(VbPool2);
            if (ret != CVI_SUCCESS)
            {
                fprintf(stderr, "CVI_VB_DestroyPool failed %x\n", ret);
                ret_val = -1;
            }
        }
    }

    if (b_sys_inited)
    {
        CVI_S32 ret = CVI_SYS_Exit();
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_SYS_Exit failed %x\n", ret);
            ret_val = -1;
        }
    }

    if (b_vb_inited)
    {
        CVI_S32 ret = CVI_VB_Exit();
        if (ret != CVI_SUCCESS)
        {
            fprintf(stderr, "CVI_VB_Exit failed %x\n", ret);
            ret_val = -1;
        }
    }

    return ret_val;
}

int jpeg_decoder_cvi_impl::deinit()
{
    corrupted = 1;
    width = 0;
    height = 0;
    ch = 0;
    components = 0;
    sampling_factor = -1;
    progressive = 0;
    orientation = -1;

    yuv_pixel_format = PIXEL_FORMAT_YUV_PLANAR_444;
    yuv_buffer_size = 0;
    yuv_rotated_pixel_format = PIXEL_FORMAT_NV12;
    yuv_rotated_buffer_size = 0;
    bgr_pixel_format = PIXEL_FORMAT_BGR_888;
    bgr_buffer_size = 0;

    need_realign_uv = false;

    return 0;
}

bool jpeg_decoder_cvi::supported(const unsigned char* jpgdata, int jpgsize)
{
    if (!jpgdata || jpgsize < 4)
        return false;

    // jpg magic
    if (jpgdata[0] != 0xFF || jpgdata[1] != 0xD8)
        return false;

    if (!sys.ready)
        return false;

    if (!vdec.ready)
        return false;

    if (!vpu.ready)
        return false;

    return true;
}

jpeg_decoder_cvi::jpeg_decoder_cvi() : d(new jpeg_decoder_cvi_impl)
{
}

jpeg_decoder_cvi::~jpeg_decoder_cvi()
{
    delete d;
}

int jpeg_decoder_cvi::init(const unsigned char* jpgdata, int jpgsize, int* width, int* height, int* ch)
{
    return d->init(jpgdata, jpgsize, width, height, ch);
}

int jpeg_decoder_cvi::decode(const unsigned char* jpgdata, int jpgsize, unsigned char* outbgr) const
{
    return d->decode(jpgdata, jpgsize, outbgr);
}

int jpeg_decoder_cvi::deinit()
{
    return d->deinit();
}

} // namespace cv

#else // defined __linux__

namespace cv {

bool jpeg_decoder_cvi::supported(const unsigned char* /*jpgdata*/, int /*jpgsize*/)
{
    return false;
}

jpeg_decoder_cvi::jpeg_decoder_cvi() : d(0)
{
}

jpeg_decoder_cvi::~jpeg_decoder_cvi()
{
}

int jpeg_decoder_cvi::init(const unsigned char* /*jpgdata*/, int /*jpgsize*/, int* /*width*/, int* /*height*/, int* /*ch*/)
{
    return -1;
}

int jpeg_decoder_cvi::decode(const unsigned char* /*jpgdata*/, int /*jpgsize*/, unsigned char* /*outbgr*/) const
{
    return -1;
}

int jpeg_decoder_cvi::deinit()
{
    return -1;
}

} // namespace cv

#endif // defined __linux__
