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

#include "jpeg_encoder_aw.h"

#if defined __linux__
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#if __ARM_NEON
#include <arm_neon.h>
#endif

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

namespace cv {

// 0 = unknown
// 1 = t113-i
// 2 = tinyvision
// 3 = yuzuki-chameleon
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

        if (strncmp(buf, "sun8iw20", 8) == 0)
        {
            // t113-i
            device_model = 1;
        }
        if (strncmp(buf, "sun8iw21", 8) == 0)
        {
            // tinyvision
            device_model = 2;
        }
        if (strncmp(buf, "sun50iw9", 8) == 0)
        {
            // yuzuki-chameleon
            device_model = 3;
        }
    }

    if (device_model > 0)
    {
        fprintf(stderr, "opencv-mobile HW JPG encoder with aw cedarc\n");
    }

    return device_model;
}

static bool is_device_whitelisted()
{
    return get_device_model() > 0;
}


extern "C" {

typedef enum VENC_CODEC_TYPE {
    VENC_CODEC_H264,
    VENC_CODEC_JPEG,
    VENC_CODEC_H264_VER2,
    VENC_CODEC_H265,
    VENC_CODEC_VP8,
} VENC_CODEC_TYPE;

typedef enum VENC_PIXEL_FMT {
    VENC_PIXEL_YUV420SP,
    VENC_PIXEL_YVU420SP,
    VENC_PIXEL_YUV420P,
    VENC_PIXEL_YVU420P,
    VENC_PIXEL_YUV422SP,
    VENC_PIXEL_YVU422SP,
    VENC_PIXEL_YUV422P,
    VENC_PIXEL_YVU422P,
    VENC_PIXEL_YUYV422,
    VENC_PIXEL_UYVY422,
    VENC_PIXEL_YVYU422,
    VENC_PIXEL_VYUY422,
    VENC_PIXEL_ARGB,
    VENC_PIXEL_RGBA,
    VENC_PIXEL_ABGR,
    VENC_PIXEL_BGRA,
    VENC_PIXEL_TILE_32X32,
    VENC_PIXEL_TILE_128X32,
    VENC_PIXEL_AFBC_AW,
    VENC_PIXEL_LBC_AW, //* for v5v200 and newer ic
} VENC_PIXEL_FMT;

typedef enum VENC_INDEXTYPE {
    VENC_IndexParamBitrate                = 0x0,
    /**< reference type: int */
    VENC_IndexParamFramerate,
    /**< reference type: int */
    VENC_IndexParamMaxKeyInterval,
    /**< reference type: int */
    VENC_IndexParamIfilter,
    /**< reference type: int */
    VENC_IndexParamRotation,
    /**< reference type: int */
    VENC_IndexParamSliceHeight,
    /**< reference type: int */
    VENC_IndexParamForceKeyFrame,
    /**< reference type: int (write only)*/
    VENC_IndexParamMotionDetectEnable,
    /**< reference type: MotionParam(write only) */
    VENC_IndexParamMotionDetectStatus,
    /**< reference type: int(read only) */
    VENC_IndexParamRgb2Yuv,
    /**< reference type: VENC_COLOR_SPACE */
    VENC_IndexParamYuv2Yuv,
    /**< reference type: VENC_YUV2YUV */
    VENC_IndexParamROIConfig,
    /**< reference type: VencROIConfig */
    VENC_IndexParamStride,
    /**< reference type: int */
    VENC_IndexParamColorFormat,
    /**< reference type: VENC_PIXEL_FMT */
    VENC_IndexParamSize,
    /**< reference type: VencSize(read only) */
    VENC_IndexParamSetVbvSize,
    /**< reference type: setVbvSize(write only) */
    VENC_IndexParamVbvInfo,
    /**< reference type: getVbvInfo(read only) */
    VENC_IndexParamSuperFrameConfig,
    /**< reference type: VencSuperFrameConfig */
    VENC_IndexParamSetPSkip,
    /**< reference type: unsigned int */
    VENC_IndexParamResetEnc,
    /**< reference type: */
	VENC_IndexParamSaveBSFile,
	/**< reference type: VencSaveBSFile */
	VENC_IndexParamHorizonFlip,
	/**< reference type: unsigned int */

    /* check capabiliy */
    VENC_IndexParamMAXSupportSize,
    /**< reference type: VencSize(read only) */
    VENC_IndexParamCheckColorFormat,
    /**< reference type: VencCheckFormat(read only) */

    /* H264 param */
    VENC_IndexParamH264Param  = 0x100,
    /**< reference type: VencH264Param */
    VENC_IndexParamH264SPSPPS,
    /**< reference type: VencHeaderData (read only)*/
    VENC_IndexParamH264QPRange,
    /**< reference type: VencQPRange */
    VENC_IndexParamH264ProfileLevel,
    /**< reference type: VencProfileLevel */
    VENC_IndexParamH264EntropyCodingCABAC,
    /**< reference type: int(0:CAVLC 1:CABAC) */
    VENC_IndexParamH264CyclicIntraRefresh,
    /**< reference type: VencCyclicIntraRefresh */
    VENC_IndexParamH264FixQP,
    /**< reference type: VencFixQP */
    VENC_IndexParamH264SVCSkip,
    /**< reference type: VencH264SVCSkip */
    VENC_IndexParamH264AspectRatio,
    /**< reference type: VencH264AspectRatio */
    VENC_IndexParamFastEnc,
    /**< reference type: int */
    VENC_IndexParamH264VideoSignal,
    /**< reference type: VencH264VideoSignal */
    VENC_IndexParamH264VideoTiming,
    /**< reference type: VencH264VideoTiming */
    VENC_IndexParamChmoraGray,
    /**< reference type: unsigned char */
    VENC_IndexParamIQpOffset,
    /**< reference type: constant QP */
    VENC_IndexParamH264ConstantQP,
    /**< reference type: int */
    /* jpeg param */
    VENC_IndexParamJpegQuality            = 0x200,
    /**< reference type: int (1~100) */
    VENC_IndexParamJpegExifInfo,
    /**< reference type: EXIFInfo */
    VENC_IndexParamJpegEncMode,
    /**< reference type: 0:jpeg; 1:motion_jepg */
    VENC_IndexParamJpegVideoSignal,
    /**< reference type: VencJpegVideoSignal */

    /* VP8 param */
    VENC_IndexParamVP8Param,
    /* max one frame length */
    VENC_IndexParamSetFrameLenThreshold,
    /**< reference type: int */
    /* decrease the a20 dram bands */
    VENC_IndexParamSetA20LowBands,
    /**< reference type: 0:disable; 1:enable */
    VENC_IndexParamSetBitRateRange,
    /**< reference type: VencBitRateRange */
    VENC_IndexParamLongTermReference,
    /**< reference type: 0:disable; 1:enable, default:enable */

    /* h265 param */
    VENC_IndexParamH265Param = 0x300,
    VENC_IndexParamH265Gop,
    VENC_IndexParamH265ToalFramesNum,
    VENC_IndexParamH26xUpdateLTRef,
    VENC_IndexParamH265Header,
    VENC_IndexParamH265TendRatioCoef,
    VENC_IndexParamH265Trans,
    /**< reference type: VencH265TranS */
    VENC_IndexParamH265Sao,
    /**< reference type: VencH265SaoS */
    VENC_IndexParamH265Dblk,
    /**< reference type: VencH265DblkS */
    VENC_IndexParamH265Timing,
    /**< reference type: VencH265TimingS */
    VENC_IndexParamIntraPeriod,
    VENC_IndexParamMBModeCtrl,
    VENC_IndexParamMBSumInfoOutput,
    VENC_IndexParamMBInfoOutput,
    VENC_IndexParamVUIAspectRatio,
    VENC_IndexParamVUIVideoSignal,
    VENC_IndexParamVUIChromaLoc,
    VENC_IndexParamVUIDisplayWindow,
    VENC_IndexParamVUIBitstreamRestriction,

    VENC_IndexParamAlterFrame = 0x400,
    /**< reference type: unsigned int */
    VENC_IndexParamVirtualIFrame,
    VENC_IndexParamChannelNum,
    VENC_IndexParamProcSet,
    /**< reference type: VencOverlayInfoS */
    VENC_IndexParamSetOverlay,
    /**< reference type: unsigned char */
    VENC_IndexParamAllParams,
    /**< reference type:VencBrightnessS */
    VENC_IndexParamBright,
    /**< reference type:VencSmartFun */
    VENC_IndexParamSmartFuntion,
    /**< reference type: VencHVS */
    VENC_IndexParamHVS,
    /**< reference type: unsigned char */
    VENC_IndexParamSkipTend,
    /**< reference type: unsigned char */
    VENC_IndexParamHighPassFilter,
    /**< reference type: unsigned char */
    VENC_IndexParamPFrameIntraEn,
    /**< reference type: unsigned char */
    VENC_IndexParamEncodeTimeEn,
    /**< reference type: VencEncodeTimeS */
    VENC_IndexParamGetEncodeTime,
    /**< reference type: unsigned char */
    VENC_IndexParam3DFilter,
    /**< reference type: unsigned char */
    VENC_IndexParamIntra4x4En,

    /**< reference type: unsigned int */
    VENC_IndexParamSetNullFrame = 0x500,
    /**< reference type: VencThumbInfo */
    VENC_IndexParamGetThumbYUV,
    /**< reference type: E_ISP_SCALER_RATIO */
    VENC_IndexParamSetThumbScaler,
    /**< reference type: unsigned char */
    VENC_IndexParamAdaptiveIntraInP,
    /**< reference type: VencBaseConfig */
    VENC_IndexParamUpdateBaseInfo,

    /**< reference type: unsigned char */
    VENC_IndexParamFillingCbr,

    /**< reference type: unsigned char */
    VENC_IndexParamRoi,

    /**< reference type: unsigned int */
    /* drop the frame that bitstreamLen exceed vbv-valid-size */
    VENC_IndexParamDropOverflowFrame,

    /**< reference type: unsigned int; 0: day, 1: night*/
    VENC_IndexParamIsNightCaseFlag,

    /**< reference type: unsigned int; 0: normal case, 1: ipc case*/
    VENC_IndexParamProductCase,

    /**< reference type: VencWatermarkInfoS */
    VENC_IndexParamSetOverlayByWatermark,
} VENC_INDEXTYPE;

struct ScMemOpsS;
struct VeOpsS;

typedef enum eVeLbcMode
{
    LBC_MODE_DISABLE  = 0,
    LBC_MODE_1_5X     = 1,
    LBC_MODE_2_0X     = 2,
    LBC_MODE_2_5X     = 3,
    LBC_MODE_NO_LOSSY = 4,
}eVeLbcMode;

typedef struct VencBaseConfig {
    unsigned char       bEncH264Nalu;
    unsigned int        nInputWidth;
    unsigned int        nInputHeight;
    unsigned int        nDstWidth;
    unsigned int        nDstHeight;
    unsigned int        nStride;
    VENC_PIXEL_FMT      eInputFormat;
    struct ScMemOpsS *memops;
    VeOpsS*           veOpsS;
    void*             pVeOpsSelf;

    unsigned char     bOnlyWbFlag;

    //* for v5v200 and newer ic
    unsigned char     bLbcLossyComEnFlag2x;
    unsigned char     bLbcLossyComEnFlag2_5x;
    unsigned char     bIsVbvNoCache;
    //* end
} VencBaseConfig;

typedef struct VencBaseConfig_v85x {
    unsigned char       bEncH264Nalu;
    unsigned int        nInputWidth;
    unsigned int        nInputHeight;
    unsigned int        nDstWidth;
    unsigned int        nDstHeight;
    unsigned int        nStride;
    VENC_PIXEL_FMT      eInputFormat;
    struct ScMemOpsS *memops;
    VeOpsS*           veOpsS;
    void*             pVeOpsSelf;

    unsigned char     bOnlyWbFlag;

    //* for v5v200 and newer ic
    unsigned char     bLbcLossyComEnFlag1_5x;
    unsigned char     bLbcLossyComEnFlag2x;
    unsigned char     bLbcLossyComEnFlag2_5x;
    unsigned char     bIsVbvNoCache;
	//* end

    unsigned int      bOnlineMode;    //* 1: online mode,    0: offline mode;
    unsigned int      bOnlineChannel; //* 1: online channel, 0: offline channel;
    unsigned int      nOnlineShareBufNum; //* share buffer num

	//*for debug
    unsigned int extend_flag;  //* flag&0x1: printf reg before interrupt
                               //* flag&0x2: printf reg after  interrupt
    eVeLbcMode   rec_lbc_mode; //*0: disable, 1:1.5x , 2: 2.0x, 3: 2.5x, 4: no_lossy
	//*for debug(end)
} VencBaseConfig_v85x;

typedef struct VencAllocateBufferParam {
    unsigned int   nBufferNum;
    unsigned int   nSizeY;
    unsigned int   nSizeC;
} VencAllocateBufferParam;

typedef struct VencRect {
    int nLeft;
    int nTop;
    int nWidth;
    int nHeight;
} VencRect;

/* support 4 ROI region */
typedef struct VencROIConfig {
    int                     bEnable;
    int                        index; /* (0~3) */
    int                     nQPoffset;
    unsigned char           roi_abs_flag;
    VencRect                sRect;
} VencROIConfig;

typedef struct VencInputBuffer {
    unsigned long  nID;
    long long         nPts;
    unsigned int   nFlag;
    unsigned char* pAddrPhyY;
    unsigned char* pAddrPhyC;
    unsigned char* pAddrVirY;
    unsigned char* pAddrVirC;
    int            nWidth;
    int            nHeight;
    int            nAlign;
    int            bEnableCorp;
    VencRect       sCropInfo;

    int            ispPicVar;
    int            ispPicVarChroma;     //chroma  filter  coef[0-63],  from isp
    int			   bUseInputBufferRoi;
    VencROIConfig  roi_param[8];
    int            bAllocMemSelf;
    int            nShareBufFd;
    unsigned char  bUseCsiColorFormat;
    VENC_PIXEL_FMT eCsiColorFormat;

    int             envLV;
} VencInputBuffer;

typedef struct VencInputBuffer_v85x {
    unsigned long  nID;
    long long         nPts;
    unsigned int   nFlag;
    unsigned char* pAddrPhyY;
    unsigned char* pAddrPhyC;
    unsigned char* pAddrVirY;
    unsigned char* pAddrVirC;
    int            bEnableCorp;
    VencRect       sCropInfo;

    int            ispPicVar;
    int            ispPicVarChroma;     //chroma  filter  coef[0-63],  from isp
    int			   bUseInputBufferRoi;
    VencROIConfig  roi_param[8];
    int            bAllocMemSelf;
    int            nShareBufFd;
    unsigned char  bUseCsiColorFormat;
    VENC_PIXEL_FMT eCsiColorFormat;

    int             envLV;
    int             bNeedFlushCache;
}VencInputBuffer_v85x;

typedef struct FrameInfo {
    int             CurrQp;
    int             avQp;
    int             nGopIndex;
    int             nFrameIndex;
    int             nTotalIndex;
} FrameInfo;

typedef struct VencOutputBuffer {
    int               nID;
    long long         nPts;
    unsigned int   nFlag;
    unsigned int   nSize0;
    unsigned int   nSize1;
    unsigned char* pData0;
    unsigned char* pData1;

    FrameInfo       frame_info;
    unsigned int   nSize2;
    unsigned char* pData2;
} VencOutputBuffer;

typedef void* VideoEncoder;

typedef VideoEncoder* (*PFN_VideoEncCreate)(VENC_CODEC_TYPE eCodecType);
typedef void (*PFN_VideoEncDestroy)(VideoEncoder* pEncoder);
typedef int (*PFN_VideoEncInit)(VideoEncoder* pEncoder, VencBaseConfig* pConfig);
typedef int (*PFN_VideoEncUnInit)(VideoEncoder* pEncoder);
typedef int (*PFN_AllocInputBuffer)(VideoEncoder* pEncoder, VencAllocateBufferParam* pBufferParam);
typedef int (*PFN_GetOneAllocInputBuffer)(VideoEncoder* pEncoder, VencInputBuffer* pInputbuffer);
typedef int (*PFN_FlushCacheAllocInputBuffer)(VideoEncoder* pEncoder,  VencInputBuffer* pInputbuffer);
typedef int (*PFN_ReturnOneAllocInputBuffer)(VideoEncoder* pEncoder,  VencInputBuffer* pInputbuffer);
typedef int (*PFN_ReleaseAllocInputBuffer)(VideoEncoder* pEncoder);
typedef int (*PFN_AddOneInputBuffer)(VideoEncoder* pEncoder, VencInputBuffer* pInputbuffer);
typedef int (*PFN_VideoEncodeOneFrame)(VideoEncoder* pEncoder);
typedef int (*PFN_AlreadyUsedInputBuffer)(VideoEncoder* pEncoder, VencInputBuffer* pBuffer);
typedef int (*PFN_ValidBitstreamFrameNum)(VideoEncoder* pEncoder);
typedef int (*PFN_GetOneBitstreamFrame)(VideoEncoder* pEncoder, VencOutputBuffer* pBuffer);
typedef int (*PFN_FreeOneBitStreamFrame)(VideoEncoder* pEncoder, VencOutputBuffer* pBuffer);
typedef int (*PFN_VideoEncGetParameter)(VideoEncoder* pEncoder, VENC_INDEXTYPE indexType, void* paramData);
typedef int (*PFN_VideoEncSetParameter)(VideoEncoder* pEncoder, VENC_INDEXTYPE indexType, void* paramData);

// v85x
typedef VideoEncoder* (*PFN_VencCreate)(VENC_CODEC_TYPE eCodecType);
typedef void (*PFN_VencDestroy)(VideoEncoder* pEncoder);
typedef int (*PFN_VencInit)(VideoEncoder* pEncoder, VencBaseConfig_v85x* pConfig);
typedef int (*PFN_VencStart)(VideoEncoder* pEncoder);
typedef int (*PFN_VencPause)(VideoEncoder* pEncoder);
typedef int (*PFN_VencReset)(VideoEncoder* pEncoder);
typedef int (*PFN_VencAllocateInputBuf)(VideoEncoder* pEncoder, VencAllocateBufferParam *pBufferParam, VencInputBuffer_v85x* dst_inputBuf);
typedef int (*PFN_VencGetValidInputBufNum)(VideoEncoder* pEncoder);
typedef int (*PFN_VencQueueInputBuf)(VideoEncoder* pEncoder, VencInputBuffer_v85x* inputbuffer);
typedef int (*PFN_VencGetValidOutputBufNum)(VideoEncoder* pEncoder);
typedef int (*PFN_VencDequeueOutputBuf)(VideoEncoder* pEncoder, VencOutputBuffer* pBuffer);
typedef int (*PFN_VencQueueOutputBuf)(VideoEncoder* pEncoder, VencOutputBuffer* pBuffer);
typedef int (*PFN_VencGetParameter)(VideoEncoder* pEncoder, VENC_INDEXTYPE indexType, void* paramData);
typedef int (*PFN_VencSetParameter)(VideoEncoder* pEncoder, VENC_INDEXTYPE indexType, void* paramData);

typedef enum
{
    VencEvent_FrameFormatNotMatch  = 0,  // frame format is not match to initial setting.
    VencEvent_UpdateMbModeInfo     = 1,
    VencEvent_UpdateMbStatInfo     = 2,
    VencEvent_UpdateSharpParam     = 3,
    VencEvent_UpdateIspMotionParam = 4,
    VencEvent_UpdateVeToIspParam   = 5,
    VencEvent_Max = 0x7FFFFFFF
} VencEventType;

typedef struct
{
    int nResult;
    VencInputBuffer *pInputBuffer;
    //other informations about this frame encoding can be added below.

} VencCbInputBufferDoneInfo;

typedef struct
{
   int (*EventHandler)(
        VideoEncoder* pEncoder,
        void* pAppData,
        VencEventType eEvent,
        unsigned int nData1,
        unsigned int nData2,
        void* pEventData);

    int (*InputBufferDone)(
        VideoEncoder* pEncoder,
        void* pAppData,
        VencCbInputBufferDoneInfo* pBufferDoneInfo);
} VencCbType;

typedef int (*PFN_VencSetCallbacks)(VideoEncoder* pEncoder, VencCbType* pCallbacks, void* pAppData);

}

static void* libvencoder = 0;

static PFN_VideoEncCreate VideoEncCreate = 0;
static PFN_VideoEncDestroy VideoEncDestroy = 0;
static PFN_VideoEncInit VideoEncInit = 0;
static PFN_VideoEncUnInit VideoEncUnInit = 0;
static PFN_AllocInputBuffer AllocInputBuffer = 0;
static PFN_GetOneAllocInputBuffer GetOneAllocInputBuffer = 0;
static PFN_FlushCacheAllocInputBuffer FlushCacheAllocInputBuffer = 0;
static PFN_ReturnOneAllocInputBuffer ReturnOneAllocInputBuffer = 0;
static PFN_ReleaseAllocInputBuffer ReleaseAllocInputBuffer = 0;
static PFN_AddOneInputBuffer AddOneInputBuffer = 0;
static PFN_VideoEncodeOneFrame VideoEncodeOneFrame = 0;
static PFN_AlreadyUsedInputBuffer AlreadyUsedInputBuffer = 0;
static PFN_ValidBitstreamFrameNum ValidBitstreamFrameNum = 0;
static PFN_GetOneBitstreamFrame GetOneBitstreamFrame = 0;
static PFN_FreeOneBitStreamFrame FreeOneBitStreamFrame = 0;
static PFN_VideoEncGetParameter VideoEncGetParameter = 0;
static PFN_VideoEncSetParameter VideoEncSetParameter = 0;

// v85x
static PFN_VencCreate VencCreate = 0;
static PFN_VencDestroy VencDestroy = 0;
static PFN_VencInit VencInit = 0;
static PFN_VencStart VencStart = 0;
static PFN_VencPause VencPause = 0;
static PFN_VencReset VencReset = 0;
static PFN_VencAllocateInputBuf VencAllocateInputBuf = 0;
static PFN_VencGetValidInputBufNum VencGetValidInputBufNum = 0;
static PFN_VencQueueInputBuf VencQueueInputBuf = 0;
static PFN_VencGetValidOutputBufNum VencGetValidOutputBufNum = 0;
static PFN_VencDequeueOutputBuf VencDequeueOutputBuf = 0;
static PFN_VencQueueOutputBuf VencQueueOutputBuf = 0;
static PFN_VencGetParameter VencGetParameter = 0;
static PFN_VencSetParameter VencSetParameter = 0;
static PFN_VencSetCallbacks VencSetCallbacks = 0;

static int load_vencoder_library()
{
    if (libvencoder)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        return -1;
    }

    libvencoder = dlopen("libvencoder.so", RTLD_LOCAL | RTLD_NOW);
    if (!libvencoder)
    {
        libvencoder = dlopen("/usr/lib/libvencoder.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libvencoder)
    {
        return -1;
    }

    if (get_device_model() == 2)
    {
        VencCreate = (PFN_VencCreate)dlsym(libvencoder, "VencCreate");
        VencDestroy = (PFN_VencDestroy)dlsym(libvencoder, "VencDestroy");
        VencInit = (PFN_VencInit)dlsym(libvencoder, "VencInit");
        VencStart = (PFN_VencStart)dlsym(libvencoder, "VencStart");
        VencPause = (PFN_VencPause)dlsym(libvencoder, "VencPause");
        VencReset = (PFN_VencReset)dlsym(libvencoder, "VencReset");
        VencAllocateInputBuf = (PFN_VencAllocateInputBuf)dlsym(libvencoder, "VencAllocateInputBuf");
        VencGetValidInputBufNum = (PFN_VencGetValidInputBufNum)dlsym(libvencoder, "VencGetValidInputBufNum");
        VencQueueInputBuf = (PFN_VencQueueInputBuf)dlsym(libvencoder, "VencQueueInputBuf");
        VencGetValidOutputBufNum = (PFN_VencGetValidOutputBufNum)dlsym(libvencoder, "VencGetValidOutputBufNum");
        VencDequeueOutputBuf = (PFN_VencDequeueOutputBuf)dlsym(libvencoder, "VencDequeueOutputBuf");
        VencQueueOutputBuf = (PFN_VencQueueOutputBuf)dlsym(libvencoder, "VencQueueOutputBuf");
        VencGetParameter = (PFN_VencGetParameter)dlsym(libvencoder, "VencGetParameter");
        VencSetParameter = (PFN_VencSetParameter)dlsym(libvencoder, "VencSetParameter");
        VencSetCallbacks = (PFN_VencSetCallbacks)dlsym(libvencoder, "VencSetCallbacks");
    }
    else
    {
        VideoEncCreate = (PFN_VideoEncCreate)dlsym(libvencoder, "VideoEncCreate");
        VideoEncDestroy = (PFN_VideoEncDestroy)dlsym(libvencoder, "VideoEncDestroy");
        VideoEncInit = (PFN_VideoEncInit)dlsym(libvencoder, "VideoEncInit");
        VideoEncUnInit = (PFN_VideoEncUnInit)dlsym(libvencoder, "VideoEncUnInit");
        AllocInputBuffer = (PFN_AllocInputBuffer)dlsym(libvencoder, "AllocInputBuffer");
        GetOneAllocInputBuffer = (PFN_GetOneAllocInputBuffer)dlsym(libvencoder, "GetOneAllocInputBuffer");
        FlushCacheAllocInputBuffer = (PFN_FlushCacheAllocInputBuffer)dlsym(libvencoder, "FlushCacheAllocInputBuffer");
        ReturnOneAllocInputBuffer = (PFN_ReturnOneAllocInputBuffer)dlsym(libvencoder, "ReturnOneAllocInputBuffer");
        ReleaseAllocInputBuffer = (PFN_ReleaseAllocInputBuffer)dlsym(libvencoder, "ReleaseAllocInputBuffer");
        AddOneInputBuffer = (PFN_AddOneInputBuffer)dlsym(libvencoder, "AddOneInputBuffer");
        VideoEncodeOneFrame = (PFN_VideoEncodeOneFrame)dlsym(libvencoder, "VideoEncodeOneFrame");
        AlreadyUsedInputBuffer = (PFN_AlreadyUsedInputBuffer)dlsym(libvencoder, "AlreadyUsedInputBuffer");
        ValidBitstreamFrameNum = (PFN_ValidBitstreamFrameNum)dlsym(libvencoder, "ValidBitstreamFrameNum");
        GetOneBitstreamFrame = (PFN_GetOneBitstreamFrame)dlsym(libvencoder, "GetOneBitstreamFrame");
        FreeOneBitStreamFrame = (PFN_FreeOneBitStreamFrame)dlsym(libvencoder, "FreeOneBitStreamFrame");
        VideoEncGetParameter = (PFN_VideoEncGetParameter)dlsym(libvencoder, "VideoEncGetParameter");
        VideoEncSetParameter = (PFN_VideoEncSetParameter)dlsym(libvencoder, "VideoEncSetParameter");
    }

    return 0;
}

static int unload_vencoder_library()
{
    if (!libvencoder)
        return 0;

    dlclose(libvencoder);
    libvencoder = 0;

    VideoEncCreate = 0;
    VideoEncDestroy = 0;
    VideoEncInit = 0;
    VideoEncUnInit = 0;
    AllocInputBuffer = 0;
    GetOneAllocInputBuffer = 0;
    FlushCacheAllocInputBuffer = 0;
    ReturnOneAllocInputBuffer = 0;
    ReleaseAllocInputBuffer = 0;
    AddOneInputBuffer = 0;
    VideoEncodeOneFrame = 0;
    AlreadyUsedInputBuffer = 0;
    ValidBitstreamFrameNum = 0;
    GetOneBitstreamFrame = 0;
    FreeOneBitStreamFrame = 0;
    VideoEncGetParameter = 0;
    VideoEncSetParameter = 0;

    VencCreate = 0;
    VencDestroy = 0;
    VencInit = 0;
    VencStart = 0;
    VencPause = 0;
    VencReset = 0;
    VencAllocateInputBuf = 0;
    VencGetValidInputBufNum = 0;
    VencQueueInputBuf = 0;
    VencGetValidOutputBufNum = 0;
    VencDequeueOutputBuf = 0;
    VencQueueOutputBuf = 0;
    VencGetParameter = 0;
    VencSetParameter = 0;
    VencSetCallbacks = 0;

    return 0;
}

class vencoder_library_loader
{
public:
    bool ready;

    vencoder_library_loader()
    {
        ready = (load_vencoder_library() == 0);
    }

    ~vencoder_library_loader()
    {
        unload_vencoder_library();
    }
};

static vencoder_library_loader vencoder;


static void gray2yuv420sp(const unsigned char* graydata, int width, int height, unsigned char* yptr, unsigned char* uvptr, int stride)
{
    for (int y = 0; y + 1 < height; y += 2)
    {
        const unsigned char* p0 = graydata + y * width;
        const unsigned char* p1 = graydata + (y + 1) * width;
        unsigned char* yptr0 = yptr + y * stride;
        unsigned char* yptr1 = yptr + (y + 1) * stride;
        unsigned char* uvptr0 = uvptr + (y / 2) * stride;

        memcpy(yptr0, p0, width);
        memcpy(yptr1, p1, width);
        memset(uvptr0, 128, width);
    }
}

static void bgr2yuv420sp(const unsigned char* bgrdata, int width, int height, unsigned char* yptr, unsigned char* uvptr, int stride)
{
#if __ARM_NEON
    uint8x8_t _v38 = vdup_n_u8(38);
    uint8x8_t _v75 = vdup_n_u8(75);
    uint8x8_t _v15 = vdup_n_u8(15);

    uint8x8_t _v127 = vdup_n_u8(127);
    uint8x8_t _v84_107 = vzip_u8(vdup_n_u8(84), vdup_n_u8(107)).val[0];
    uint8x8_t _v43_20 = vzip_u8(vdup_n_u8(43), vdup_n_u8(20)).val[0];
    uint16x8_t _v128 = vdupq_n_u16((128 << 8) + 128);
#endif // __ARM_NEON

    for (int y = 0; y + 1 < height; y += 2)
    {
        const unsigned char* p0 = bgrdata + y * width * 3;
        const unsigned char* p1 = bgrdata + (y + 1) * width * 3;
        unsigned char* yptr0 = yptr + y * stride;
        unsigned char* yptr1 = yptr + (y + 1) * stride;
        unsigned char* uvptr0 = uvptr + (y / 2) * stride;

        int x = 0;
#if __ARM_NEON
        for (; x + 7 < width; x += 8)
        {
            uint8x8x3_t _bgr0 = vld3_u8(p0);
            uint8x8x3_t _bgr1 = vld3_u8(p1);

            uint16x8_t _y0 = vmull_u8(_bgr0.val[0], _v15);
            uint16x8_t _y1 = vmull_u8(_bgr1.val[0], _v15);
            _y0 = vmlal_u8(_y0, _bgr0.val[1], _v75);
            _y1 = vmlal_u8(_y1, _bgr1.val[1], _v75);
            _y0 = vmlal_u8(_y0, _bgr0.val[2], _v38);
            _y1 = vmlal_u8(_y1, _bgr1.val[2], _v38);
            uint8x8_t _y0_u8 = vqrshrun_n_s16(vreinterpretq_s16_u16(_y0), 7);
            uint8x8_t _y1_u8 = vqrshrun_n_s16(vreinterpretq_s16_u16(_y1), 7);

            uint16x4_t _b4 = vpaddl_u8(_bgr0.val[0]);
            uint16x4_t _g4 = vpaddl_u8(_bgr0.val[1]);
            uint16x4_t _r4 = vpaddl_u8(_bgr0.val[2]);
            _b4 = vpadal_u8(_b4, _bgr1.val[0]);
            _g4 = vpadal_u8(_g4, _bgr1.val[1]);
            _r4 = vpadal_u8(_r4, _bgr1.val[2]);
            uint16x4x2_t _brbr = vzip_u16(_b4, _r4);
            uint16x4x2_t _gggg = vzip_u16(_g4, _g4);
            uint16x4x2_t _rbrb = vzip_u16(_r4, _b4);
            uint8x8_t _br = vshrn_n_u16(vcombine_u16(_brbr.val[0], _brbr.val[1]), 2);
            uint8x8_t _gg = vshrn_n_u16(vcombine_u16(_gggg.val[0], _gggg.val[1]), 2);
            uint8x8_t _rb = vshrn_n_u16(vcombine_u16(_rbrb.val[0], _rbrb.val[1]), 2);

            // uint8x8_t _br = vtrn_u8(_bgr0.val[0], _bgr0.val[2]).val[0];
            // uint8x8_t _gg = vtrn_u8(_bgr0.val[1], _bgr0.val[1]).val[0];
            // uint8x8_t _rb = vtrn_u8(_bgr0.val[2], _bgr0.val[0]).val[0];

            uint16x8_t _uv = vmlal_u8(_v128, _br, _v127);
            _uv = vmlsl_u8(_uv, _gg, _v84_107);
            _uv = vmlsl_u8(_uv, _rb, _v43_20);
            uint8x8_t _uv_u8 = vqshrn_n_u16(_uv, 8);

            vst1_u8(yptr0, _y0_u8);
            vst1_u8(yptr1, _y1_u8);
            vst1_u8(uvptr0, _uv_u8);

            p0 += 24;
            p1 += 24;
            yptr0 += 8;
            yptr1 += 8;
            uvptr0 += 8;
        }
#endif
        for (; x + 1 < width; x += 2)
        {
            unsigned char b00 = p0[0];
            unsigned char g00 = p0[1];
            unsigned char r00 = p0[2];

            unsigned char b01 = p0[3];
            unsigned char g01 = p0[4];
            unsigned char r01 = p0[5];

            unsigned char b10 = p1[0];
            unsigned char g10 = p1[1];
            unsigned char r10 = p1[2];

            unsigned char b11 = p1[3];
            unsigned char g11 = p1[4];
            unsigned char r11 = p1[5];

            // y =  0.29900 * r + 0.58700 * g + 0.11400 * b
            // u = -0.16874 * r - 0.33126 * g + 0.50000 * b  + 128
            // v =  0.50000 * r - 0.41869 * g - 0.08131 * b  + 128

#define SATURATE_CAST_UCHAR(X) (unsigned char)::std::min(::std::max((int)(X), 0), 255);
            unsigned char y00 = SATURATE_CAST_UCHAR(( 38 * r00 + 75 * g00 +  15 * b00 + 64) >> 7);
            unsigned char y01 = SATURATE_CAST_UCHAR(( 38 * r01 + 75 * g01 +  15 * b01 + 64) >> 7);
            unsigned char y10 = SATURATE_CAST_UCHAR(( 38 * r10 + 75 * g10 +  15 * b10 + 64) >> 7);
            unsigned char y11 = SATURATE_CAST_UCHAR(( 38 * r11 + 75 * g11 +  15 * b11 + 64) >> 7);

            unsigned char b4 = (b00 + b01 + b10 + b11) / 4;
            unsigned char g4 = (g00 + g01 + g10 + g11) / 4;
            unsigned char r4 = (r00 + r01 + r10 + r11) / 4;

            // unsigned char b4 = b00;
            // unsigned char g4 = g00;
            // unsigned char r4 = r00;

            unsigned char u = SATURATE_CAST_UCHAR(((-43 * r4 -  84 * g4 + 127 * b4 + 128) >> 8) + 128);
            unsigned char v = SATURATE_CAST_UCHAR(((127 * r4 - 107 * g4 -  20 * b4 + 128) >> 8) + 128);
#undef SATURATE_CAST_UCHAR

            yptr0[0] = y00;
            yptr0[1] = y01;
            yptr1[0] = y10;
            yptr1[1] = y11;
            uvptr0[0] = u;
            uvptr0[1] = v;

            p0 += 6;
            p1 += 6;
            yptr0 += 2;
            yptr1 += 2;
            uvptr0 += 2;
        }
    }
}

static void bgra2yuv420sp(const unsigned char* bgradata, int width, int height, unsigned char* yptr, unsigned char* uvptr, int stride)
{
#if __ARM_NEON
    uint8x8_t _v38 = vdup_n_u8(38);
    uint8x8_t _v75 = vdup_n_u8(75);
    uint8x8_t _v15 = vdup_n_u8(15);

    uint8x8_t _v127 = vdup_n_u8(127);
    uint8x8_t _v84_107 = vzip_u8(vdup_n_u8(84), vdup_n_u8(107)).val[0];
    uint8x8_t _v43_20 = vzip_u8(vdup_n_u8(43), vdup_n_u8(20)).val[0];
    uint16x8_t _v128 = vdupq_n_u16((128 << 8) + 128);
#endif // __ARM_NEON

    for (int y = 0; y + 1 < height; y += 2)
    {
        const unsigned char* p0 = bgradata + y * width * 4;
        const unsigned char* p1 = bgradata + (y + 1) * width * 4;
        unsigned char* yptr0 = yptr + y * stride;
        unsigned char* yptr1 = yptr + (y + 1) * stride;
        unsigned char* uvptr0 = uvptr + (y / 2) * stride;

        int x = 0;
#if __ARM_NEON
        for (; x + 7 < width; x += 8)
        {
            uint8x8x4_t _bgr0 = vld4_u8(p0);
            uint8x8x4_t _bgr1 = vld4_u8(p1);

            uint16x8_t _y0 = vmull_u8(_bgr0.val[0], _v15);
            uint16x8_t _y1 = vmull_u8(_bgr1.val[0], _v15);
            _y0 = vmlal_u8(_y0, _bgr0.val[1], _v75);
            _y1 = vmlal_u8(_y1, _bgr1.val[1], _v75);
            _y0 = vmlal_u8(_y0, _bgr0.val[2], _v38);
            _y1 = vmlal_u8(_y1, _bgr1.val[2], _v38);
            uint8x8_t _y0_u8 = vqrshrun_n_s16(vreinterpretq_s16_u16(_y0), 7);
            uint8x8_t _y1_u8 = vqrshrun_n_s16(vreinterpretq_s16_u16(_y1), 7);

            uint16x4_t _b4 = vpaddl_u8(_bgr0.val[0]);
            uint16x4_t _g4 = vpaddl_u8(_bgr0.val[1]);
            uint16x4_t _r4 = vpaddl_u8(_bgr0.val[2]);
            _b4 = vpadal_u8(_b4, _bgr1.val[0]);
            _g4 = vpadal_u8(_g4, _bgr1.val[1]);
            _r4 = vpadal_u8(_r4, _bgr1.val[2]);
            uint16x4x2_t _brbr = vzip_u16(_b4, _r4);
            uint16x4x2_t _gggg = vzip_u16(_g4, _g4);
            uint16x4x2_t _rbrb = vzip_u16(_r4, _b4);
            uint8x8_t _br = vshrn_n_u16(vcombine_u16(_brbr.val[0], _brbr.val[1]), 2);
            uint8x8_t _gg = vshrn_n_u16(vcombine_u16(_gggg.val[0], _gggg.val[1]), 2);
            uint8x8_t _rb = vshrn_n_u16(vcombine_u16(_rbrb.val[0], _rbrb.val[1]), 2);

            // uint8x8_t _br = vtrn_u8(_bgr0.val[0], _bgr0.val[2]).val[0];
            // uint8x8_t _gg = vtrn_u8(_bgr0.val[1], _bgr0.val[1]).val[0];
            // uint8x8_t _rb = vtrn_u8(_bgr0.val[2], _bgr0.val[0]).val[0];

            uint16x8_t _uv = vmlal_u8(_v128, _br, _v127);
            _uv = vmlsl_u8(_uv, _gg, _v84_107);
            _uv = vmlsl_u8(_uv, _rb, _v43_20);
            uint8x8_t _uv_u8 = vqshrn_n_u16(_uv, 8);

            vst1_u8(yptr0, _y0_u8);
            vst1_u8(yptr1, _y1_u8);
            vst1_u8(uvptr0, _uv_u8);

            p0 += 32;
            p1 += 32;
            yptr0 += 8;
            yptr1 += 8;
            uvptr0 += 8;
        }
#endif
        for (; x + 1 < width; x += 2)
        {
            unsigned char b00 = p0[0];
            unsigned char g00 = p0[1];
            unsigned char r00 = p0[2];

            unsigned char b01 = p0[4];
            unsigned char g01 = p0[5];
            unsigned char r01 = p0[6];

            unsigned char b10 = p1[0];
            unsigned char g10 = p1[1];
            unsigned char r10 = p1[2];

            unsigned char b11 = p1[4];
            unsigned char g11 = p1[5];
            unsigned char r11 = p1[6];

            // y =  0.29900 * r + 0.58700 * g + 0.11400 * b
            // u = -0.16874 * r - 0.33126 * g + 0.50000 * b  + 128
            // v =  0.50000 * r - 0.41869 * g - 0.08131 * b  + 128

#define SATURATE_CAST_UCHAR(X) (unsigned char)::std::min(::std::max((int)(X), 0), 255);
            unsigned char y00 = SATURATE_CAST_UCHAR(( 38 * r00 + 75 * g00 +  15 * b00 + 64) >> 7);
            unsigned char y01 = SATURATE_CAST_UCHAR(( 38 * r01 + 75 * g01 +  15 * b01 + 64) >> 7);
            unsigned char y10 = SATURATE_CAST_UCHAR(( 38 * r10 + 75 * g10 +  15 * b10 + 64) >> 7);
            unsigned char y11 = SATURATE_CAST_UCHAR(( 38 * r11 + 75 * g11 +  15 * b11 + 64) >> 7);

            unsigned char b4 = (b00 + b01 + b10 + b11) / 4;
            unsigned char g4 = (g00 + g01 + g10 + g11) / 4;
            unsigned char r4 = (r00 + r01 + r10 + r11) / 4;

            // unsigned char b4 = b00;
            // unsigned char g4 = g00;
            // unsigned char r4 = r00;

            unsigned char u = SATURATE_CAST_UCHAR(((-43 * r4 -  84 * g4 + 127 * b4 + 128) >> 8) + 128);
            unsigned char v = SATURATE_CAST_UCHAR(((127 * r4 - 107 * g4 -  20 * b4 + 128) >> 8) + 128);
#undef SATURATE_CAST_UCHAR

            yptr0[0] = y00;
            yptr0[1] = y01;
            yptr1[0] = y10;
            yptr1[1] = y11;
            uvptr0[0] = u;
            uvptr0[1] = v;

            p0 += 8;
            p1 += 8;
            yptr0 += 2;
            yptr1 += 2;
            uvptr0 += 2;
        }
    }
}

class jpeg_encoder_aw_impl
{
public:
    jpeg_encoder_aw_impl();
    ~jpeg_encoder_aw_impl();

    int init(int width, int height, int ch, int quality);

    int encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const;

    int encode(const unsigned char* bgrdata, const char* outfilepath) const;

    int deinit();

public:
    int inited;
    int width;
    int height;
    int ch;

    VideoEncoder* venc;

    mutable VencInputBuffer input_buffer;
    mutable VencInputBuffer_v85x input_buffer_v85x;
    int b_input_buffer_got;
};

jpeg_encoder_aw_impl::jpeg_encoder_aw_impl()
{
    inited = 0;
    width = 0;
    height = 0;
    ch = 0;

    venc = 0;

    b_input_buffer_got = 0;
}

jpeg_encoder_aw_impl::~jpeg_encoder_aw_impl()
{
    deinit();
}

static int EventHandler(VideoEncoder* /*pEncoder*/, void* /*pAppData*/, VencEventType /*eEvent*/, unsigned int /*nData1*/, unsigned int /*nData2*/, void* /*pEventData*/)
{
    // fprintf(stderr, "EventHandler event = %d\n", eEvent);
    return 0;
}

static int InputBufferDone(VideoEncoder* /*pEncoder*/, void* pAppData, VencCbInputBufferDoneInfo* pBufferDoneInfo)
{
    // fprintf(stderr, "InputBufferDone\n");
    jpeg_encoder_aw_impl* pthis = (jpeg_encoder_aw_impl*)pAppData;

    memcpy(&pthis->input_buffer_v85x, pBufferDoneInfo->pInputBuffer, sizeof(VencInputBuffer_v85x));

    return 0;
}

int jpeg_encoder_aw_impl::init(int _width, int _height, int _ch, int quality)
{
    if (!vencoder.ready)
    {
        fprintf(stderr, "vencoder not ready\n");
        return -1;
    }

    if (inited)
    {
        int ret = deinit();
        if (ret != 0)
        {
            fprintf(stderr, "deinit failed before re-init\n");
            return -1;
        }
    }

    width = _width;
    height = _height;
    ch = _ch;

    // fprintf(stderr, "width = %d\n", width);
    // fprintf(stderr, "height = %d\n", height);
    // fprintf(stderr, "ch = %d\n", ch);

    const int aligned_width = (width + 15) / 16 * 16;
    const int aligned_height = (height + 15) / 16 * 16;

    if (get_device_model() == 2)
    {
        venc = VencCreate(VENC_CODEC_JPEG);
        if (!venc)
        {
            fprintf(stderr, "VencCreate failed\n");
            goto OUT;
        }

        // {
        //     int vbv_size = aligned_width * aligned_height * 3 / 2;
        //     int ret = VencSetParameter(venc, VENC_IndexParamSetVbvSize, (void*)&vbv_size);
        //     if (ret)
        //     {
        //         fprintf(stderr, "VencSetParameter VENC_IndexParamSetVbvSize failed %d\n", ret);
        //         goto OUT;
        //     }
        // }

        {
            int ret = VencSetParameter(venc, VENC_IndexParamJpegQuality, (void*)&quality);
            if (ret)
            {
                fprintf(stderr, "VencSetParameter VENC_IndexParamJpegQuality failed %d\n", ret);
                goto OUT;
            }
        }

        {
            int enc_mode = 0;
            int ret = VencSetParameter(venc, VENC_IndexParamJpegEncMode, (void*)&enc_mode);
            if (ret)
            {
                fprintf(stderr, "VencSetParameter VENC_IndexParamJpegEncMode failed %d\n", ret);
                goto OUT;
            }
        }

        {
            VencBaseConfig_v85x config;
            memset(&config, 0, sizeof(config));
            config.nInputWidth = width;
            config.nInputHeight = height;
            config.nDstWidth = width;
            config.nDstHeight = height;
            config.nStride = aligned_width;
            config.eInputFormat = VENC_PIXEL_YUV420SP;

            int ret = VencInit(venc, &config);
            if (ret)
            {
                fprintf(stderr, "VencInit failed %d\n", ret);
                goto OUT;
            }
        }

        {
            VencAllocateBufferParam bufferParam;
            bufferParam.nSizeY = aligned_width * aligned_height;
            bufferParam.nSizeC = aligned_width * aligned_height / 2;
            bufferParam.nBufferNum = 1;

            int ret = VencAllocateInputBuf(venc, &bufferParam, &input_buffer_v85x);
            if (ret)
            {
                fprintf(stderr, "VencAllocateInputBuf failed %d\n", ret);
                goto OUT;
            }

            b_input_buffer_got = 1;
        }

        {
            VencCbType vencCallBack;
            vencCallBack.EventHandler = EventHandler;
            vencCallBack.InputBufferDone = InputBufferDone;

            int ret = VencSetCallbacks(venc, &vencCallBack, this);
            if (ret)
            {
                fprintf(stderr, "VencSetCallbacks failed %d\n", ret);
                goto OUT;
            }
        }

        {
            int ret = VencStart(venc);
            if (ret)
            {
                fprintf(stderr, "VencStart failed %d\n", ret);
                goto OUT;
            }
        }
    }
    else
    {
        venc = VideoEncCreate(VENC_CODEC_JPEG);
        if (!venc)
        {
            fprintf(stderr, "VideoEncCreate failed\n");
            goto OUT;
        }

        {
            int ret = VideoEncSetParameter(venc, VENC_IndexParamJpegQuality, (void*)&quality);
            if (ret)
            {
                fprintf(stderr, "VideoEncSetParameter VENC_IndexParamJpegQuality failed %d\n", ret);
                goto OUT;
            }
        }

        {
            int enc_mode = 0;
            int ret = VideoEncSetParameter(venc, VENC_IndexParamJpegEncMode, (void*)&enc_mode);
            if (ret)
            {
                fprintf(stderr, "VideoEncSetParameter VENC_IndexParamJpegEncMode failed %d\n", ret);
                goto OUT;
            }
        }

        {
            VencBaseConfig config;
            memset(&config, 0, sizeof(config));
            config.nInputWidth = width;
            config.nInputHeight = height;
            config.nDstWidth = width;
            config.nDstHeight = height;
            config.nStride = aligned_width;
            config.eInputFormat = VENC_PIXEL_YUV420SP;

            int ret = VideoEncInit(venc, &config);
            if (ret)
            {
                fprintf(stderr, "VideoEncInit failed %d\n", ret);
                goto OUT;
            }
        }

        {
            VencAllocateBufferParam bufferParam;
            bufferParam.nSizeY = aligned_width * aligned_height;
            bufferParam.nSizeC = aligned_width * aligned_height / 2;
            bufferParam.nBufferNum = 1;

            int ret = AllocInputBuffer(venc, &bufferParam);
            if (ret)
            {
                fprintf(stderr, "AllocInputBuffer failed %d\n", ret);
                goto OUT;
            }
        }

        {
            int ret = GetOneAllocInputBuffer(venc, &input_buffer);
            if (ret)
            {
                fprintf(stderr, "GetOneAllocInputBuffer failed %d\n", ret);
                goto OUT;
            }

            b_input_buffer_got = 1;
        }
    }

    inited = 1;

    return 0;

OUT:
    deinit();

    return -1;
}

int jpeg_encoder_aw_impl::encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const
{
    outdata.clear();

    if (!inited)
    {
        fprintf(stderr, "not inited\n");
        return -1;
    }

    int ret_val = 0;

    VencOutputBuffer output_buffer;
    int b_output_buffer_got = 0;

    const int aligned_width = (width + 15) / 16 * 16;

    if (get_device_model() == 2)
    {
        if (ch == 1)
        {
            gray2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer_v85x.pAddrVirY, (unsigned char*)input_buffer_v85x.pAddrVirC, aligned_width);
        }
        if (ch == 3)
        {
            bgr2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer_v85x.pAddrVirY, (unsigned char*)input_buffer_v85x.pAddrVirC, aligned_width);
        }
        if (ch == 4)
        {
            bgra2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer_v85x.pAddrVirY, (unsigned char*)input_buffer_v85x.pAddrVirC, aligned_width);
        }

        {
            input_buffer_v85x.bNeedFlushCache = 1;

            int ret = VencQueueInputBuf(venc, &input_buffer_v85x);
            if (ret)
            {
                fprintf(stderr, "VencQueueInputBuf failed %d\n", ret);
                goto OUT;
            }
        }

        int retry_count = 0;
        while (1)
        {
            int ret = VencDequeueOutputBuf(venc, &output_buffer);
            if (ret == 5 && retry_count < 20)
            {
                // VENC_RESULT_BITSTREAM_IS_EMPTY
                // wait encoder complete
                usleep(10*1000);
                retry_count++;
                continue;
            }
            if (ret)
            {
                fprintf(stderr, "VencDequeueOutputBuf failed %d\n", ret);
                goto OUT;
            }

            b_output_buffer_got = 1;
            break;
        }
    }
    else
    {
        if (ch == 1)
        {
            gray2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer.pAddrVirY, (unsigned char*)input_buffer.pAddrVirC, aligned_width);
        }
        if (ch == 3)
        {
            bgr2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer.pAddrVirY, (unsigned char*)input_buffer.pAddrVirC, aligned_width);
        }
        if (ch == 4)
        {
            bgra2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer.pAddrVirY, (unsigned char*)input_buffer.pAddrVirC, aligned_width);
        }

        {
            int ret = FlushCacheAllocInputBuffer(venc, &input_buffer);
            if (ret)
            {
                fprintf(stderr, "FlushCacheAllocInputBuffer failed %d\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            int ret = AddOneInputBuffer(venc, &input_buffer);
            if (ret)
            {
                fprintf(stderr, "AddOneInputBuffer failed %d\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            int ret = VideoEncodeOneFrame(venc);
            if (ret)
            {
                fprintf(stderr, "VideoEncodeOneFrame failed %d\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            int ret = AlreadyUsedInputBuffer(venc, &input_buffer);
            if (ret)
            {
                fprintf(stderr, "AlreadyUsedInputBuffer failed %d\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            int ret = GetOneBitstreamFrame(venc, &output_buffer);
            if (ret)
            {
                fprintf(stderr, "GetOneBitstreamFrame failed %d\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_output_buffer_got = 1;
        }
    }

    outdata.resize(output_buffer.nSize0 + output_buffer.nSize1);
    memcpy(outdata.data(), output_buffer.pData0, output_buffer.nSize0);
    if (output_buffer.nSize1)
    {
        memcpy(outdata.data() + output_buffer.nSize0, output_buffer.pData1, output_buffer.nSize1);
    }

OUT:
    if (b_output_buffer_got)
    {
        if (get_device_model() == 2)
        {
            int ret = VencQueueOutputBuf(venc, &output_buffer);
            if (ret)
            {
                fprintf(stderr, "VencQueueOutputBuf failed %d\n", ret);
                ret_val = -1;
            }
        }
        else
        {
            int ret = FreeOneBitStreamFrame(venc, &output_buffer);
            if (ret)
            {
                fprintf(stderr, "FreeOneBitStreamFrame failed %d\n", ret);
                ret_val = -1;
            }
        }
    }

    return ret_val;
}

int jpeg_encoder_aw_impl::encode(const unsigned char* bgrdata, const char* outfilepath) const
{
    if (!inited)
    {
        fprintf(stderr, "not inited\n");
        return -1;
    }

    int ret_val = 0;

    VencOutputBuffer output_buffer;
    int b_output_buffer_got = 0;

    const int aligned_width = (width + 15) / 16 * 16;

    FILE* fp = 0;

    if (get_device_model() == 2)
    {
    // fprintf(stderr, "a\n");
        if (ch == 1)
        {
            gray2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer_v85x.pAddrVirY, (unsigned char*)input_buffer_v85x.pAddrVirC, aligned_width);
        }
        if (ch == 3)
        {
            bgr2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer_v85x.pAddrVirY, (unsigned char*)input_buffer_v85x.pAddrVirC, aligned_width);
        }
        if (ch == 4)
        {
            bgra2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer_v85x.pAddrVirY, (unsigned char*)input_buffer_v85x.pAddrVirC, aligned_width);
        }

        {
            input_buffer_v85x.bNeedFlushCache = 1;

            int ret = VencQueueInputBuf(venc, &input_buffer_v85x);
            if (ret)
            {
                fprintf(stderr, "VencQueueInputBuf failed %d\n", ret);
                goto OUT;
            }
        }

        int retry_count = 0;
        while (1)
        {
            int ret = VencDequeueOutputBuf(venc, &output_buffer);
            if (ret == 5 && retry_count < 20)
            {
                // VENC_RESULT_BITSTREAM_IS_EMPTY
                // wait encoder complete
                usleep(10*1000);
                retry_count++;
                continue;
            }
            if (ret)
            {
                fprintf(stderr, "VencDequeueOutputBuf failed %d\n", ret);
                goto OUT;
            }

            b_output_buffer_got = 1;
            break;
        }
    }
    else
    {
        if (ch == 1)
        {
            gray2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer.pAddrVirY, (unsigned char*)input_buffer.pAddrVirC, aligned_width);
        }
        if (ch == 3)
        {
            bgr2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer.pAddrVirY, (unsigned char*)input_buffer.pAddrVirC, aligned_width);
        }
        if (ch == 4)
        {
            bgra2yuv420sp(bgrdata, width, height, (unsigned char*)input_buffer.pAddrVirY, (unsigned char*)input_buffer.pAddrVirC, aligned_width);
        }

        {
            int ret = FlushCacheAllocInputBuffer(venc, &input_buffer);
            if (ret)
            {
                fprintf(stderr, "FlushCacheAllocInputBuffer failed %d\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            int ret = AddOneInputBuffer(venc, &input_buffer);
            if (ret)
            {
                fprintf(stderr, "AddOneInputBuffer failed %d\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            int ret = VideoEncodeOneFrame(venc);
            if (ret)
            {
                fprintf(stderr, "VideoEncodeOneFrame failed %d\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            int ret = AlreadyUsedInputBuffer(venc, &input_buffer);
            if (ret)
            {
                fprintf(stderr, "AlreadyUsedInputBuffer failed %d\n", ret);
                ret_val = -1;
                goto OUT;
            }
        }

        {
            int ret = GetOneBitstreamFrame(venc, &output_buffer);
            if (ret)
            {
                fprintf(stderr, "GetOneBitstreamFrame failed %d\n", ret);
                ret_val = -1;
                goto OUT;
            }

            b_output_buffer_got = 1;
        }
    }

    fp = fopen(outfilepath, "wb");
    if (!fp)
    {
        fprintf(stderr, "fopen %s failed\n", outfilepath);
        ret_val = -1;
        goto OUT;
    }

    fwrite(output_buffer.pData0, 1, output_buffer.nSize0, fp);
    if (output_buffer.nSize1)
    {
        fwrite(output_buffer.pData1, 1, output_buffer.nSize1, fp);
    }

OUT:
    if (b_output_buffer_got)
    {
        if (get_device_model() == 2)
        {
            int ret = VencQueueOutputBuf(venc, &output_buffer);
            if (ret)
            {
                fprintf(stderr, "VencQueueOutputBuf failed %d\n", ret);
                ret_val = -1;
            }
        }
        else
        {
            int ret = FreeOneBitStreamFrame(venc, &output_buffer);
            if (ret)
            {
                fprintf(stderr, "FreeOneBitStreamFrame failed %d\n", ret);
                ret_val = -1;
            }
        }
    }

    if (fp)
    {
        fclose(fp);
    }

    return ret_val;
}

int jpeg_encoder_aw_impl::deinit()
{
    if (!inited)
        return 0;

    int ret_val = 0;

    if (b_input_buffer_got)
    {
        if (get_device_model() == 2)
        {
            // free input_buffer_v85x ?
        }
        else
        {
            int ret = ReturnOneAllocInputBuffer(venc, &input_buffer);
            if (ret)
            {
                fprintf(stderr, "ReturnOneAllocInputBuffer failed %d\n", ret);
                ret_val = -1;
            }
        }

        b_input_buffer_got = 0;
    }

    if (venc)
    {
        if (get_device_model() == 2)
        {
            VencPause(venc);
            VencDestroy(venc);
        }
        else
        {
            VideoEncDestroy(venc);
        }

        venc = 0;
    }

    width = 0;
    height = 0;
    ch = 0;

    inited = 0;

    return ret_val;
}

bool jpeg_encoder_aw::supported(int width, int height, int ch)
{
    if (!vencoder.ready)
        return false;

    if (ch != 1 && ch != 3 && ch != 4)
        return false;

    if (width % 2 != 0 || height % 2 != 0)
        return false;

    if (width < 8 || height < 8)
        return false;

    if (width * height > 4000 * 4000)
        return false;

    return true;
}

jpeg_encoder_aw::jpeg_encoder_aw() : d(new jpeg_encoder_aw_impl)
{
}

jpeg_encoder_aw::~jpeg_encoder_aw()
{
    delete d;
}

int jpeg_encoder_aw::init(int width, int height, int ch, int quality)
{
    return d->init(width, height, ch, quality);
}

int jpeg_encoder_aw::encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const
{
    return d->encode(bgrdata, outdata);
}

int jpeg_encoder_aw::encode(const unsigned char* bgrdata, const char* outfilepath) const
{
    return d->encode(bgrdata, outfilepath);
}

int jpeg_encoder_aw::deinit()
{
    return d->deinit();
}

} // namespace cv

#else // defined __linux__

namespace cv {

bool jpeg_encoder_aw::supported(int /*width*/, int /*height*/, int /*ch*/)
{
    return false;
}

jpeg_encoder_aw::jpeg_encoder_aw() : d(0)
{
}

jpeg_encoder_aw::~jpeg_encoder_aw()
{
}

int jpeg_encoder_aw::init(int /*width*/, int /*height*/, int /*ch*/, int /*quality*/)
{
    return -1;
}

int jpeg_encoder_aw::encode(const unsigned char* /*bgrdata*/, std::vector<unsigned char>& /*outdata*/) const
{
    return -1;
}

int jpeg_encoder_aw::encode(const unsigned char* /*bgrdata*/, const char* /*outfilepath*/) const
{
    return -1;
}

int jpeg_encoder_aw::deinit()
{
    return -1;
}

} // namespace cv

#endif // defined __linux__
