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

#include "jpeg_decoder_aw.h"

#if defined __linux__
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <sstream>
#include <string>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#if __ARM_NEON
#include <arm_neon.h>
#endif

#include "exif.hpp"
#include "kanna_rotate.h"

namespace cv {

// 0 = unknown
// 1 = t113-i
// 2 = tinyvision
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
    }

    return device_model;
}

static bool is_device_whitelisted()
{
    const int device_model = get_device_model();

    if (device_model == 1)
    {
        // t113-i
        return true;
    }
    if (device_model == 2)
    {
        // tinyvision
        return true;
    }

    return false;
}

extern "C" {

typedef void (*PFN_AddVDPlugin)();
typedef void (*PFN_AddVDPluginSingle)(const char* lib);

}

static void* libcdc_base = 0;
static void* libvideoengine = 0;

static PFN_AddVDPlugin AddVDPlugin = 0;
static PFN_AddVDPluginSingle AddVDPluginSingle = 0;

static int unload_videoengine_library()
{
    if (libcdc_base)
    {
        dlclose(libcdc_base);
        libcdc_base = 0;
    }

    if (libvideoengine)
    {
        dlclose(libvideoengine);
        libvideoengine = 0;
    }

    AddVDPlugin = 0;
    AddVDPluginSingle = 0;

    return 0;
}

static int load_videoengine_library()
{
    if (libvideoengine)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        fprintf(stderr, "this device is not whitelisted for jpeg decoder aw cedarc\n");
        return -1;
    }

    libcdc_base = dlopen("libcdc_base.so", RTLD_GLOBAL | RTLD_LAZY);
    if (!libcdc_base)
    {
        libcdc_base = dlopen("/usr/lib/libcdc_base.so", RTLD_GLOBAL | RTLD_LAZY);
    }
    if (!libcdc_base)
    {
        goto OUT;
    }

    libvideoengine = dlopen("libvideoengine.so", RTLD_LOCAL | RTLD_NOW);
    if (!libvideoengine)
    {
        libvideoengine = dlopen("/usr/lib/libvideoengine.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libvideoengine)
    {
        goto OUT;
    }

    AddVDPlugin = (PFN_AddVDPlugin)dlsym(libvideoengine, "AddVDPlugin");
    AddVDPluginSingle = (PFN_AddVDPluginSingle)dlsym(libvideoengine, "AddVDPluginSingle");

    return 0;

OUT:
    unload_videoengine_library();

    return -1;
}

class videoengine_library_loader
{
public:
    bool ready;

    videoengine_library_loader()
    {
        ready = (load_videoengine_library() == 0);

        if (libvideoengine)
        {
            // AddVDPlugin();
            AddVDPluginSingle("/usr/lib/libawmjpeg.so");
        }
    }

    ~videoengine_library_loader()
    {
        unload_videoengine_library();
    }
};

static videoengine_library_loader videoengine;


extern "C" {

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#if (INTPTR_MAX == INT64_MAX)
    typedef unsigned long u64;
#else
    typedef unsigned long long u64;
#endif

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

#if (INTPTR_MAX == INT64_MAX)
    typedef signed long s64;
#else
    typedef signed long long s64;
#endif

typedef uintptr_t size_addr;

struct VeOpsS;
struct ScMemOpsS;

enum EVIDEOCODECFORMAT
{
    VIDEO_CODEC_FORMAT_UNKNOWN          = 0,
    VIDEO_CODEC_FORMAT_MJPEG            = 0x101,
    VIDEO_CODEC_FORMAT_MPEG1            = 0x102,
    VIDEO_CODEC_FORMAT_MPEG2            = 0x103,
    VIDEO_CODEC_FORMAT_MPEG4            = 0x104,
    VIDEO_CODEC_FORMAT_MSMPEG4V1        = 0x105,
    VIDEO_CODEC_FORMAT_MSMPEG4V2        = 0x106,
    VIDEO_CODEC_FORMAT_DIVX3            = 0x107, //* not support
    VIDEO_CODEC_FORMAT_DIVX4            = 0x108, //* not support
    VIDEO_CODEC_FORMAT_DIVX5            = 0x109, //* not support
    VIDEO_CODEC_FORMAT_XVID             = 0x10a,
    VIDEO_CODEC_FORMAT_H263             = 0x10b,
    VIDEO_CODEC_FORMAT_SORENSSON_H263   = 0x10c,
    VIDEO_CODEC_FORMAT_RXG2             = 0x10d,
    VIDEO_CODEC_FORMAT_WMV1             = 0x10e,
    VIDEO_CODEC_FORMAT_WMV2             = 0x10f,
    VIDEO_CODEC_FORMAT_WMV3             = 0x110,
    VIDEO_CODEC_FORMAT_VP6              = 0x111,
    VIDEO_CODEC_FORMAT_VP8              = 0x112,
    VIDEO_CODEC_FORMAT_VP9              = 0x113,
    VIDEO_CODEC_FORMAT_RX               = 0x114,
    VIDEO_CODEC_FORMAT_H264             = 0x115,
    VIDEO_CODEC_FORMAT_H265             = 0x116,
    VIDEO_CODEC_FORMAT_AVS              = 0x117,
    VIDEO_CODEC_FORMAT_AVS2             = 0x118,

    VIDEO_CODEC_FORMAT_MAX              = VIDEO_CODEC_FORMAT_AVS2,
    VIDEO_CODEC_FORMAT_MIN              = VIDEO_CODEC_FORMAT_MJPEG,
};

enum EPIXELFORMAT
{
    PIXEL_FORMAT_DEFAULT            = 0,

    PIXEL_FORMAT_YUV_PLANER_420     = 1,
    PIXEL_FORMAT_YUV_PLANER_422     = 2,
    PIXEL_FORMAT_YUV_PLANER_444     = 3,

    PIXEL_FORMAT_YV12               = 4,
    PIXEL_FORMAT_NV21               = 5,
    PIXEL_FORMAT_NV12               = 6,
    PIXEL_FORMAT_YUV_MB32_420       = 7,
    PIXEL_FORMAT_YUV_MB32_422       = 8,
    PIXEL_FORMAT_YUV_MB32_444       = 9,

    PIXEL_FORMAT_RGBA                = 10,
    PIXEL_FORMAT_ARGB                = 11,
    PIXEL_FORMAT_ABGR                = 12,
    PIXEL_FORMAT_BGRA                = 13,

    PIXEL_FORMAT_YUYV                = 14,
    PIXEL_FORMAT_YVYU                = 15,
    PIXEL_FORMAT_UYVY                = 16,
    PIXEL_FORMAT_VYUY                = 17,

    PIXEL_FORMAT_PLANARUV_422        = 18,
    PIXEL_FORMAT_PLANARVU_422        = 19,
    PIXEL_FORMAT_PLANARUV_444        = 20,
    PIXEL_FORMAT_PLANARVU_444        = 21,
    PIXEL_FORMAT_P010_UV             = 22,
    PIXEL_FORMAT_P010_VU             = 23,

    PIXEL_FORMAT_MIN = PIXEL_FORMAT_DEFAULT,
    PIXEL_FORMAT_MAX = PIXEL_FORMAT_PLANARVU_444,
};

typedef enum CONTROL_AFBC_MODE {
    DISABLE_AFBC_ALL_SIZE     = 0,
    ENABLE_AFBC_JUST_BIG_SIZE = 1, //* >= 4k
    ENABLE_AFBC_ALL_SIZE      = 2,
}eControlAfbcMode;

typedef enum CONTROL_IPTV_MODE {
    DISABLE_IPTV_ALL_SIZE     = 0,
    ENABLE_IPTV_JUST_SMALL_SIZE = 1, //* < 4k
    ENABLE_IPTV_ALL_SIZE      = 2,
}eControlIptvMode;

typedef enum COMMON_CONFIG_FLAG
{
   IS_MIRACAST_STREAM = 1,

}eCommonConfigFlag;

enum EVDECODERESULT
{
    VDECODE_RESULT_UNSUPPORTED       = -1,
    VDECODE_RESULT_OK                = 0,
    VDECODE_RESULT_FRAME_DECODED     = 1,
    VDECODE_RESULT_CONTINUE          = 2,
    VDECODE_RESULT_KEYFRAME_DECODED  = 3,
    VDECODE_RESULT_NO_FRAME_BUFFER   = 4,
    VDECODE_RESULT_NO_BITSTREAM      = 5,
    VDECODE_RESULT_RESOLUTION_CHANGE = 6,

    VDECODE_RESULT_MIN = VDECODE_RESULT_UNSUPPORTED,
    VDECODE_RESULT_MAX = VDECODE_RESULT_RESOLUTION_CHANGE,
};

typedef struct VIDEOSTREAMINFO
{
    int   eCodecFormat;
    int   nWidth;
    int   nHeight;
    int   nFrameRate;
    int   nFrameDuration;
    int   nAspectRatio;
    int   bIs3DStream;
    int   nCodecSpecificDataLen;
    char* pCodecSpecificData;
    int   bSecureStreamFlag;
    int   bSecureStreamFlagLevel1;
    int   bIsFramePackage; /* 1: frame package;  0: stream package */
    int   h265ReferencePictureNum;
    int   bReOpenEngine;
    int   bIsFrameCtsTestFlag;
}VideoStreamInfo;

typedef struct VCONFIG
{
    int bScaleDownEn;
    int bRotationEn;
    int bSecOutputEn;
    int nHorizonScaleDownRatio;
    int nVerticalScaleDownRatio;
    int nSDWidth;
    int nSDHeight;
    int bAnySizeSD;
    int nSecHorizonScaleDownRatio;
    int nSecVerticalScaleDownRatio;
    int nRotateDegree;
    int bThumbnailMode;
    int eOutputPixelFormat;
    int eSecOutputPixelFormat;
    int bNoBFrames;
    int bDisable3D;
    int bSupportMaf;    //not use
    int bDispErrorFrame;
    int nVbvBufferSize;
    int nFrameBufferNum;
    int bSecureosEn;
    int  bGpuBufValid;
    int  nAlignStride;
    int  bIsSoftDecoderFlag;
    int  bVirMallocSbm;
    int  bSupportPallocBufBeforeDecode;
    //only used for xuqi, set this flag to 1 meaning palloc the fbm buffer before
    // decode the sequence, to short the first frame decoing time
    int nDeInterlaceHoldingFrameBufferNum;
    int nDisplayHoldingFrameBufferNum;
    int nRotateHoldingFrameBufferNum;
    int nDecodeSmoothFrameBufferNum;
    int bIsTvStream;
    int nLbcLossyComMod;   //1:1.5x; 2:2x; 3:2.5x;
    unsigned int bIsLossy; //lossy compression or not
    unsigned int bRcEn;    //compact storage or not

    struct ScMemOpsS *memops;
    eControlAfbcMode  eCtlAfbcMode;
    eControlIptvMode  eCtlIptvMode;

    VeOpsS*           veOpsS;
    void*             pVeOpsSelf;
    int               bConvertVp910bitTo8bit;
    unsigned int      nVeFreq;

    int               bCalledByOmxFlag;

    int               bSetProcInfoEnable; //* for check the decoder info by cat devices-note
    int               nSetProcInfoFreq;
    int               nChannelNum;
    int               nSupportMaxWidth;  //the max width of mjpeg continue decode
    int               nSupportMaxHeight; //the max height of mjpeg continue decode
    eCommonConfigFlag  commonConfigFlag;
    int               bATMFlag;
}VConfig;

typedef enum eVeLbcMode
{
    LBC_MODE_DISABLE  = 0,
    LBC_MODE_1_5X     = 1,
    LBC_MODE_2_0X     = 2,
    LBC_MODE_2_5X     = 3,
    LBC_MODE_NO_LOSSY = 4,
}eVeLbcMode;

typedef struct VCONFIG_v85x
{
    int bScaleDownEn;
    int bRotationEn;
    int bSecOutputEn;
    int nHorizonScaleDownRatio;
    int nVerticalScaleDownRatio;
    int nSecHorizonScaleDownRatio;
    int nSecVerticalScaleDownRatio;
    int nRotateDegree;
    int bThumbnailMode;
    int eOutputPixelFormat;
    int eSecOutputPixelFormat;
    int bNoBFrames;
    int bDisable3D;
    int bSupportMaf;    //not use
    int bDispErrorFrame;
    int nVbvBufferSize;
    int nFrameBufferNum;
    int bSecureosEn;
    int  bGpuBufValid;
    int  nAlignStride;
    int  bIsSoftDecoderFlag;
    int  bVirMallocSbm;
    int  bSupportPallocBufBeforeDecode;
    //only used for xuqi, set this flag to 1 meaning palloc the fbm buffer before
    // decode the sequence, to short the first frame decoing time
    int nDeInterlaceHoldingFrameBufferNum;
    int nDisplayHoldingFrameBufferNum;
    int nRotateHoldingFrameBufferNum;
    int nDecodeSmoothFrameBufferNum;
    int bIsTvStream;
    eVeLbcMode nLbcLossyComMod;//1:1.5x; 2:2x; 3:2.5x;

    struct ScMemOpsS *memops;
    eControlAfbcMode  eCtlAfbcMode;
    eControlIptvMode  eCtlIptvMode;

    VeOpsS*           veOpsS;
    void*             pVeOpsSelf;
    int               bConvertVp910bitTo8bit;
    unsigned int      nVeFreq;

    int               bCalledByOmxFlag;

    int               bSetProcInfoEnable; //* for check the decoder info by cat devices-note
    int               nSetProcInfoFreq;
    int               nChannelNum;
    int               nSupportMaxWidth;  //the max width of mjpeg continue decode
    int               nSupportMaxHeight; //the max height of mjpeg continue decode

    unsigned int bIsLossy; //lossy compression or not
    unsigned int bRcEn;    //compact storage or not
}VConfig_v85x;

typedef struct VIDEOSTREAMDATAINFO
{
    char*   pData;
    int     nLength;
    int64_t nPts;
    int64_t nPcr;
    int     bIsFirstPart;
    int     bIsLastPart;
    int     nID;
    int     nStreamIndex;
    int     bValid;
    unsigned int     bVideoInfoFlag;
    void*   pVideoInfo;
}VideoStreamDataInfo;

typedef enum VIDEO_TRANSFER
{
    VIDEO_TRANSFER_RESERVED_0      = 0,
    VIDEO_TRANSFER_BT1361          = 1,
    VIDEO_TRANSFER_UNSPECIFIED     = 2,
    VIDEO_TRANSFER_RESERVED_1      = 3,
    VIDEO_TRANSFER_GAMMA2_2        = 4,
    VIDEO_TRANSFER_GAMMA2_8        = 5,
    VIDEO_TRANSFER_SMPTE_170M      = 6,
    VIDEO_TRANSFER_SMPTE_240M      = 7,
    VIDEO_TRANSFER_LINEAR          = 8,
    VIDEO_TRANSFER_LOGARITHMIC_0   = 9,
    VIDEO_TRANSFER_LOGARITHMIC_1   = 10,
    VIDEO_TRANSFER_IEC61966        = 11,
    VIDEO_TRANSFER_BT1361_EXTENDED = 12,
    VIDEO_TRANSFER_SRGB     = 13,
    VIDEO_TRANSFER_BT2020_0 = 14,
    VIDEO_TRANSFER_BT2020_1 = 15,
    VIDEO_TRANSFER_ST2084   = 16,
    VIDEO_TRANSFER_ST428_1  = 17,
    VIDEO_TRANSFER_HLG      = 18,
    VIDEO_TRANSFER_RESERVED = 19, //* 19~255
}VIDEO_TRANSFER;

typedef enum VIDEO_MATRIX_COEFFS
{
    VIDEO_MATRIX_COEFFS_IDENTITY       = 0,
    VIDEO_MATRIX_COEFFS_BT709          = 1,
    VIDEO_MATRIX_COEFFS_UNSPECIFIED_0  = 2,
    VIDEO_MATRIX_COEFFS_RESERVED_0     = 3,
    VIDEO_MATRIX_COEFFS_BT470M         = 4,
    VIDEO_MATRIX_COEFFS_BT601_625_0    = 5,
    VIDEO_MATRIX_COEFFS_BT601_625_1    = 6,
    VIDEO_MATRIX_COEFFS_SMPTE_240M     = 7,
    VIDEO_MATRIX_COEFFS_YCGCO          = 8,
    VIDEO_MATRIX_COEFFS_BT2020         = 9,
    VIDEO_MATRIX_COEFFS_BT2020_CONSTANT_LUMINANCE = 10,
    VIDEO_MATRIX_COEFFS_SOMPATE                   = 11,
    VIDEO_MATRIX_COEFFS_CD_NON_CONSTANT_LUMINANCE = 12,
    VIDEO_MATRIX_COEFFS_CD_CONSTANT_LUMINANCE     = 13,
    VIDEO_MATRIX_COEFFS_BTICC                     = 14,
    VIDEO_MATRIX_COEFFS_RESERVED                  = 15, //* 15~255
}VIDEO_MATRIX_COEFFS;

typedef enum VIDEO_FULL_RANGE_FLAG
{
    VIDEO_FULL_RANGE_LIMITED = 0,
    VIDEO_FULL_RANGE_FULL    = 1,
}VIDEO_FULL_RANGE_FLAG;

typedef struct VIDEO_FRM_MV_INFO
{
    s16              nMaxMv_x;
    s16              nMinMv_x;
    s16              nAvgMv_x;
    s16              nMaxMv_y;
    s16              nMinMv_y;
    s16              nAvgMv_y;
    s16              nMaxMv;
    s16              nMinMv;
    s16              nAvgMv;
    s16              SkipRatio;
}VIDEO_FRM_MV_INFO;

typedef enum VID_FRAME_TYPE
{
    VIDEO_FORMAT_TYPE_UNKONWN = 0,
    VIDEO_FORMAT_TYPE_I,
    VIDEO_FORMAT_TYPE_P,
    VIDEO_FORMAT_TYPE_B,
    VIDEO_FORMAT_TYPE_IDR,
    VIDEO_FORMAT_TYPE_BUTT,
}VID_FRAME_TYPE;

typedef struct VIDEO_FRM_STATUS_INFO
{
    VID_FRAME_TYPE   enVidFrmType;
    int              nVidFrmSize;
    int              nVidFrmDisW;
    int              nVidFrmDisH;
    int              nVidFrmQP;
    double           nAverBitRate;
    double           nFrameRate;
    int64_t          nVidFrmPTS;
    VIDEO_FRM_MV_INFO nMvInfo;
    int              bDropPreFrame;
}VIDEO_FRM_STATUS_INFO;

typedef struct VIDEOPICTURE
{
    int     nID;
    int     nStreamIndex;
    int     ePixelFormat;
    int     nWidth;
    int     nHeight;
    int     nLineStride;
    int     nTopOffset;
    int     nLeftOffset;
    int     nBottomOffset;
    int     nRightOffset;
    int     nFrameRate;
    int     nAspectRatio;
    int     bIsProgressive;
    int     bTopFieldFirst;
    int     bRepeatTopField;
    int64_t nPts;
    int64_t nPcr;
    char*   pData0;
    char*   pData1;
    char*   pData2;
    char*   pData3;
    int     bMafValid;
    char*   pMafData;
    int     nMafFlagStride;
    int     bPreFrmValid;
    int     nBufId;
    size_addr phyYBufAddr;
    size_addr phyCBufAddr;
    void*   pPrivate;
    int     nBufFd;
    int     nBufStatus;
    int     bTopFieldError;
    int        bBottomFieldError;
    int     nColorPrimary;  // default value is 0xffffffff, valid value id 0x0000xxyy
                            // xx: is video full range code
                            // yy: is matrix coefficient
    int     bFrameErrorFlag;

    //* to save hdr info and afbc header info
    void*   pMetaData;

    //*display related parameter
    VIDEO_FULL_RANGE_FLAG   video_full_range_flag;
    VIDEO_TRANSFER          transfer_characteristics;
    VIDEO_MATRIX_COEFFS     matrix_coeffs;
    u8      colour_primaries;
    //*end of display related parameter defined
    //size_addr    nLower2BitPhyAddr;
    int          nLower2BitBufSize;
    int          nLower2BitBufOffset;
    int          nLower2BitBufStride;
    int          b10BitPicFlag;
    int          bEnableAfbcFlag;
    int          nLbcLossyComMod;//1:1.5x; 2:2x; 3:2.5x;
    unsigned int bIsLossy; //lossy compression or not
    unsigned int bRcEn;    //compact storage or not

    int     nBufSize;
    int     nAfbcSize;
    int     nLbcSize;
    int     nDebugCount;
    VIDEO_FRM_STATUS_INFO nCurFrameInfo;
}VideoPicture;

typedef void* VideoDecoder;

typedef VideoDecoder* (*PFN_CreateVideoDecoder)();
typedef void (*PFN_DestroyVideoDecoder)(VideoDecoder* pDecoder);
typedef int (*PFN_InitializeVideoDecoder)(VideoDecoder* pDecoder, VideoStreamInfo* pVideoInfo, VConfig* pVconfig);
typedef int (*PFN_RequestVideoStreamBuffer)(VideoDecoder* pDecoder, int nRequireSize, char** ppBuf, int* pBufSize, char** ppRingBuf, int* pRingBufSize, int nStreamBufIndex);
typedef int (*PFN_SubmitVideoStreamData)(VideoDecoder* pDecoder, VideoStreamDataInfo* pDataInfo, int nStreamBufIndex);
typedef int (*PFN_DecodeVideoStream)(VideoDecoder* pDecoder, int bEndOfStream, int bDecodeKeyFrameOnly, int bDropBFrameIfDelay, int64_t nCurrentTimeUs);
typedef VideoPicture* (*PFN_RequestPicture)(VideoDecoder* pDecoder, int nStreamIndex);
typedef int (*PFN_ReturnPicture)(VideoDecoder* pDecoder, VideoPicture* pPicture);

}


static void* libvdecoder = 0;

static PFN_CreateVideoDecoder CreateVideoDecoder = 0;
static PFN_DestroyVideoDecoder DestroyVideoDecoder = 0;
static PFN_InitializeVideoDecoder InitializeVideoDecoder = 0;
static PFN_RequestVideoStreamBuffer RequestVideoStreamBuffer = 0;
static PFN_SubmitVideoStreamData SubmitVideoStreamData = 0;
static PFN_DecodeVideoStream DecodeVideoStream = 0;
static PFN_RequestPicture RequestPicture = 0;
static PFN_ReturnPicture ReturnPicture = 0;

static int load_vdecoder_library()
{
    if (libvdecoder)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        fprintf(stderr, "this device is not whitelisted for jpeg decoder aw cedarc\n");
        return -1;
    }

    libvdecoder = dlopen("libvdecoder.so", RTLD_LOCAL | RTLD_NOW);
    if (!libvdecoder)
    {
        libvdecoder = dlopen("/usr/lib/libvdecoder.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libvdecoder)
    {
        return -1;
    }

    CreateVideoDecoder = (PFN_CreateVideoDecoder)dlsym(libvdecoder, "CreateVideoDecoder");
    DestroyVideoDecoder = (PFN_DestroyVideoDecoder)dlsym(libvdecoder, "DestroyVideoDecoder");
    InitializeVideoDecoder = (PFN_InitializeVideoDecoder)dlsym(libvdecoder, "InitializeVideoDecoder");
    RequestVideoStreamBuffer = (PFN_RequestVideoStreamBuffer)dlsym(libvdecoder, "RequestVideoStreamBuffer");
    SubmitVideoStreamData = (PFN_SubmitVideoStreamData)dlsym(libvdecoder, "SubmitVideoStreamData");
    DecodeVideoStream = (PFN_DecodeVideoStream)dlsym(libvdecoder, "DecodeVideoStream");
    RequestPicture = (PFN_RequestPicture)dlsym(libvdecoder, "RequestPicture");
    ReturnPicture = (PFN_ReturnPicture)dlsym(libvdecoder, "ReturnPicture");

    return 0;
}

static int unload_vdecoder_library()
{
    if (!libvdecoder)
        return 0;

    dlclose(libvdecoder);
    libvdecoder = 0;

    CreateVideoDecoder = 0;
    DestroyVideoDecoder = 0;
    InitializeVideoDecoder = 0;
    RequestVideoStreamBuffer = 0;
    SubmitVideoStreamData = 0;
    DecodeVideoStream = 0;
    RequestPicture = 0;
    ReturnPicture = 0;

    return 0;
}

class vdecoder_library_loader
{
public:
    bool ready;

    vdecoder_library_loader()
    {
        ready = (load_vdecoder_library() == 0);
    }

    ~vdecoder_library_loader()
    {
        unload_vdecoder_library();
    }
};

static vdecoder_library_loader vdecoder;


static void yuv420sp2bgr_neon(const unsigned char* yptr, const unsigned char* vuptr, int w, int h, int stride, unsigned char* bgr)
{
#if __ARM_NEON
    uint8x8_t _v128 = vdup_n_u8(128);
    int8x8_t _v90 = vdup_n_s8(90);
    int8x8_t _v46 = vdup_n_s8(46);
    int8x8_t _v22 = vdup_n_s8(22);
    int8x8_t _v113 = vdup_n_s8(113);
#endif // __ARM_NEON

    for (int y = 0; y < h; y += 2)
    {
        const unsigned char* yptr0 = yptr;
        const unsigned char* yptr1 = yptr + stride;
        unsigned char* bgr0 = bgr;
        unsigned char* bgr1 = bgr + w * 3;

#if __ARM_NEON
        int nn = w >> 3;
        int remain = w - (nn << 3);
#else
        int remain = w;
#endif // __ARM_NEON

#if __ARM_NEON
        for (; nn > 0; nn--)
        {
            int16x8_t _yy0 = vreinterpretq_s16_u16(vshll_n_u8(vld1_u8(yptr0), 6));
            int16x8_t _yy1 = vreinterpretq_s16_u16(vshll_n_u8(vld1_u8(yptr1), 6));

            int8x8_t _vvuu = vreinterpret_s8_u8(vsub_u8(vld1_u8(vuptr), _v128));
            int8x8x2_t _vvvvuuuu = vtrn_s8(_vvuu, _vvuu);
            int8x8_t _vv = _vvvvuuuu.val[0];
            int8x8_t _uu = _vvvvuuuu.val[1];

            int16x8_t _r0 = vmlal_s8(_yy0, _vv, _v90);
            int16x8_t _g0 = vmlsl_s8(_yy0, _vv, _v46);
            _g0 = vmlsl_s8(_g0, _uu, _v22);
            int16x8_t _b0 = vmlal_s8(_yy0, _uu, _v113);

            int16x8_t _r1 = vmlal_s8(_yy1, _vv, _v90);
            int16x8_t _g1 = vmlsl_s8(_yy1, _vv, _v46);
            _g1 = vmlsl_s8(_g1, _uu, _v22);
            int16x8_t _b1 = vmlal_s8(_yy1, _uu, _v113);

            uint8x8x3_t _bgr0;
            _bgr0.val[0] = vqshrun_n_s16(_b0, 6);
            _bgr0.val[1] = vqshrun_n_s16(_g0, 6);
            _bgr0.val[2] = vqshrun_n_s16(_r0, 6);

            uint8x8x3_t _bgr1;
            _bgr1.val[0] = vqshrun_n_s16(_b1, 6);
            _bgr1.val[1] = vqshrun_n_s16(_g1, 6);
            _bgr1.val[2] = vqshrun_n_s16(_r1, 6);

            vst3_u8(bgr0, _bgr0);
            vst3_u8(bgr1, _bgr1);

            yptr0 += 8;
            yptr1 += 8;
            vuptr += 8;
            bgr0 += 24;
            bgr1 += 24;
        }
#endif // __ARM_NEON

#define SATURATE_CAST_UCHAR(X) (unsigned char)::std::min(::std::max((int)(X), 0), 255);
        for (; remain > 0; remain -= 2)
        {
            // R = 1.164 * yy + 1.596 * vv
            // G = 1.164 * yy - 0.813 * vv - 0.391 * uu
            // B = 1.164 * yy              + 2.018 * uu

            // R = Y + (1.370705 * (V-128))
            // G = Y - (0.698001 * (V-128)) - (0.337633 * (U-128))
            // B = Y + (1.732446 * (U-128))

            // R = ((Y << 6) + 87.72512 * (V-128)) >> 6
            // G = ((Y << 6) - 44.672064 * (V-128) - 21.608512 * (U-128)) >> 6
            // B = ((Y << 6) + 110.876544 * (U-128)) >> 6

            // R = ((Y << 6) + 90 * (V-128)) >> 6
            // G = ((Y << 6) - 46 * (V-128) - 22 * (U-128)) >> 6
            // B = ((Y << 6) + 113 * (U-128)) >> 6

            // R = (yy + 90 * vv) >> 6
            // G = (yy - 46 * vv - 22 * uu) >> 6
            // B = (yy + 113 * uu) >> 6

            int v = vuptr[0] - 128;
            int u = vuptr[1] - 128;

            int ruv = 90 * v;
            int guv = -46 * v + -22 * u;
            int buv = 113 * u;

            int y00 = yptr0[0] << 6;
            bgr0[0] = SATURATE_CAST_UCHAR((y00 + buv) >> 6);
            bgr0[1] = SATURATE_CAST_UCHAR((y00 + guv) >> 6);
            bgr0[2] = SATURATE_CAST_UCHAR((y00 + ruv) >> 6);

            int y01 = yptr0[1] << 6;
            bgr0[3] = SATURATE_CAST_UCHAR((y01 + buv) >> 6);
            bgr0[4] = SATURATE_CAST_UCHAR((y01 + guv) >> 6);
            bgr0[5] = SATURATE_CAST_UCHAR((y01 + ruv) >> 6);

            int y10 = yptr1[0] << 6;
            bgr1[0] = SATURATE_CAST_UCHAR((y10 + buv) >> 6);
            bgr1[1] = SATURATE_CAST_UCHAR((y10 + guv) >> 6);
            bgr1[2] = SATURATE_CAST_UCHAR((y10 + ruv) >> 6);

            int y11 = yptr1[1] << 6;
            bgr1[3] = SATURATE_CAST_UCHAR((y11 + buv) >> 6);
            bgr1[4] = SATURATE_CAST_UCHAR((y11 + guv) >> 6);
            bgr1[5] = SATURATE_CAST_UCHAR((y11 + ruv) >> 6);

            yptr0 += 2;
            yptr1 += 2;
            vuptr += 2;
            bgr0 += 6;
            bgr1 += 6;
        }
#undef SATURATE_CAST_UCHAR

        yptr += 2 * stride;
        vuptr += stride - w;
        bgr += 2 * 3 * w;
    }
}

class jpeg_decoder_aw_impl
{
public:
    jpeg_decoder_aw_impl();
    ~jpeg_decoder_aw_impl();

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
};

jpeg_decoder_aw_impl::jpeg_decoder_aw_impl()
{
    corrupted = 1;
    width = 0;
    height = 0;
    ch = 0;
    components = 0;
    sampling_factor = -1;
    progressive = 0;
    orientation = 0;
}

jpeg_decoder_aw_impl::~jpeg_decoder_aw_impl()
{
    deinit();
}

int jpeg_decoder_aw_impl::init(const unsigned char* jpgdata, int jpgsize, int* _width, int* _height, int* _ch)
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

    if (corrupted)
        return -1;

    // progressive not supported
    if (progressive)
        return -1;

    // grayscale not supported
    if (sampling_factor == 4)
        return -1;

    if (width % 2 != 0 || height % 2 != 0)
        return -1;

    if (width < 8 && height < 8)
        return -1;

    ch = *_ch;
    if (ch == 0)
        ch = components;

    if (orientation > 4)
    {
        // swap width height
        int tmp = height;
        height = width;
        width = tmp;
    }

    *_width = width;
    *_height = height;
    *_ch = ch;

    return 0;
}

int jpeg_decoder_aw_impl::decode(const unsigned char* jpgdata, int jpgsize, unsigned char* outbgr) const
{
    if (!outbgr)
        return -1;

    // corrupted file
    if (corrupted)
        return -1;

    // progressive not supported
    if (progressive)
        return -1;

    // grayscale not supported
    if (sampling_factor == 4)
        return -1;

    if (width % 2 != 0 || height % 2 != 0)
        return -1;

    if (width < 8 && height < 8)
        return -1;

    const int src_width = orientation > 4 ? height : width;
    const int src_height = orientation > 4 ? width : height;

    // flag
    int ret_val = 0;

    VideoDecoder* vdec = 0;
    VideoPicture* vpic = 0;

    char* pBuf = 0;
    int bufSize = 0;
    char* pRingBuf = 0;
    int ringBufSize = 0;

    {
        vdec = CreateVideoDecoder();
        if (!vdec)
        {
            fprintf(stderr, "CreateVideoDecoder failed\n");
            ret_val = -1;
            goto OUT;
        }
    }

    {
        VideoStreamInfo videoInfo;
        memset(&videoInfo, 0, sizeof(videoInfo));
        videoInfo.eCodecFormat = VIDEO_CODEC_FORMAT_MJPEG;
        videoInfo.nWidth = src_width;
        videoInfo.nHeight = src_height;

        VConfig vconfig;
        memset(&vconfig, 0, sizeof(vconfig));
        vconfig.eOutputPixelFormat = PIXEL_FORMAT_NV21;
        vconfig.eSecOutputPixelFormat = PIXEL_FORMAT_NV21;
        vconfig.bSupportPallocBufBeforeDecode = 1;
        vconfig.nDeInterlaceHoldingFrameBufferNum = 1;
        vconfig.nDisplayHoldingFrameBufferNum = 1;
        vconfig.nRotateHoldingFrameBufferNum = 0;
        vconfig.nDecodeSmoothFrameBufferNum = 1;

        VConfig_v85x vconfig_v85x;
        memset(&vconfig_v85x, 0, sizeof(vconfig_v85x));
        vconfig_v85x.eOutputPixelFormat = PIXEL_FORMAT_NV21;
        vconfig_v85x.eSecOutputPixelFormat = PIXEL_FORMAT_NV21;
        vconfig_v85x.bSupportPallocBufBeforeDecode = 1;
        vconfig_v85x.nDeInterlaceHoldingFrameBufferNum = 1;
        vconfig_v85x.nDisplayHoldingFrameBufferNum = 1;
        vconfig_v85x.nRotateHoldingFrameBufferNum = 0;
        vconfig_v85x.nDecodeSmoothFrameBufferNum = 1;

        VConfig* p_vconfig = get_device_model() == 2 ? (VConfig*)&vconfig_v85x : &vconfig;

        int ret = InitializeVideoDecoder(vdec, &videoInfo, p_vconfig);
        if (ret != 0)
        {
            fprintf(stderr, "InitializeVideoDecoder failed %d\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    {
        int ret = RequestVideoStreamBuffer(vdec, jpgsize, &pBuf, &bufSize, &pRingBuf, &ringBufSize, 0);
        if (ret != 0)
        {
            fprintf(stderr, "RequestVideoStreamBuffer failed %d\n", ret);
            ret_val = -1;
            goto OUT;
        }

        if (bufSize + ringBufSize < jpgsize)
        {
            fprintf(stderr, "RequestVideoStreamBuffer too small %d + %d < %d\n", bufSize, ringBufSize, jpgsize);
            ret_val = -1;
            goto OUT;
        }

        // copy to vdec sbm
        if (bufSize >= jpgsize)
        {
            memcpy(pBuf, jpgdata, jpgsize);
        }
        else
        {
            memcpy(pBuf, jpgdata, bufSize);
            memcpy(pRingBuf, jpgdata + bufSize, jpgsize - bufSize);
        }
    }

    {
        VideoStreamDataInfo dataInfo;
        memset(&dataInfo, 0, sizeof(dataInfo));
        dataInfo.pData = pBuf;
        dataInfo.nLength = jpgsize;
        dataInfo.bIsFirstPart = 1;
        dataInfo.bIsLastPart = 1;

        int ret = SubmitVideoStreamData(vdec, &dataInfo, 0);
        if (ret != 0)
        {
            fprintf(stderr, "SubmitVideoStreamData failed %d\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    {
        int endofstream = 1;
        int ret = DecodeVideoStream(vdec, endofstream, 0, 0, 0);
        if (ret != VDECODE_RESULT_KEYFRAME_DECODED)
        {
            fprintf(stderr, "DecodeVideoStream failed %d\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    {
        vpic = RequestPicture(vdec, 0);
        if (!vpic)
        {
            fprintf(stderr, "RequestPicture failed\n");
            ret_val = -1;
            goto OUT;
        }

        // fprintf(stderr, "nID = %d\n", vpic->nID);
        // fprintf(stderr, "nStreamIndex = %d\n", vpic->nStreamIndex);
        // fprintf(stderr, "ePixelFormat = %d\n", vpic->ePixelFormat);
        // fprintf(stderr, "nWidth = %d\n", vpic->nWidth);
        // fprintf(stderr, "nHeight = %d\n", vpic->nHeight);
        // fprintf(stderr, "nLineStride = %d\n", vpic->nLineStride);
        // fprintf(stderr, "nTopOffset = %d\n", vpic->nTopOffset);
        // fprintf(stderr, "nLeftOffset = %d\n", vpic->nLeftOffset);
        // fprintf(stderr, "nBottomOffset = %d\n", vpic->nBottomOffset);
        // fprintf(stderr, "nRightOffset = %d\n", vpic->nRightOffset);
        // fprintf(stderr, "nFrameRate = %d\n", vpic->nFrameRate);
        // fprintf(stderr, "nAspectRatio = %d\n", vpic->nAspectRatio);
        // fprintf(stderr, "bIsProgressive = %d\n", vpic->bIsProgressive);
        // fprintf(stderr, "bTopFieldFirst = %d\n", vpic->bTopFieldFirst);
        // fprintf(stderr, "bRepeatTopField = %d\n", vpic->bRepeatTopField);
        // fprintf(stderr, "nPts = %d\n", vpic->nPts);
        // fprintf(stderr, "nPcr = %d\n", vpic->nPcr);

        if (vpic->ePixelFormat != PIXEL_FORMAT_NV21)
        {
            fprintf(stderr, "unsupported ePixelFormat %d\n", vpic->ePixelFormat);
            ret_val = -1;
            goto OUT;
        }

        {
            const unsigned char* yptr = (const unsigned char*)vpic->pData0;
            const unsigned char* vuptr = (const unsigned char*)vpic->pData1;

            if (orientation == 0 || orientation == 1)
            {
                // no rotate
                yuv420sp2bgr_neon(yptr, vuptr, width, height, vpic->nLineStride, outbgr);
            }
            else
            {
                // rotate
                std::vector<unsigned char> yuv_rotated;
                yuv_rotated.resize(width * height / 2 * 3);

                unsigned char* dstY = (unsigned char*)yuv_rotated.data();
                unsigned char* dstUV = (unsigned char*)yuv_rotated.data() + width * height;

                kanna_rotate_c1(yptr, src_width, src_height, vpic->nLineStride, dstY, width, height, width, orientation);
                kanna_rotate_c2(vuptr, src_width / 2, src_height / 2, vpic->nLineStride, dstUV, width / 2, height / 2, width, orientation);

                yuv420sp2bgr_neon(dstY, dstUV, width, height, width, outbgr);
            }
        }

    }

OUT:

    if (vpic)
    {
        ReturnPicture(vdec, vpic);
    }

    if (vdec)
    {
        DestroyVideoDecoder(vdec);
    }

    return ret_val;
}

int jpeg_decoder_aw_impl::deinit()
{
    corrupted = 1;
    width = 0;
    height = 0;
    ch = 0;
    components = 0;
    sampling_factor = -1;
    progressive = 0;
    orientation = 0;

    return 0;
}

bool jpeg_decoder_aw::supported(const unsigned char* jpgdata, int jpgsize)
{
    if (!jpgdata || jpgsize < 4)
        return false;

    // jpg magic
    if (jpgdata[0] != 0xFF || jpgdata[1] != 0xD8)
        return false;

    if (!videoengine.ready)
        return false;

    if (!vdecoder.ready)
        return false;

    return true;
}

jpeg_decoder_aw::jpeg_decoder_aw() : d(new jpeg_decoder_aw_impl)
{
}

jpeg_decoder_aw::~jpeg_decoder_aw()
{
    delete d;
}

int jpeg_decoder_aw::init(const unsigned char* jpgdata, int jpgsize, int* width, int* height, int* ch)
{
    return d->init(jpgdata, jpgsize, width, height, ch);
}

int jpeg_decoder_aw::decode(const unsigned char* jpgdata, int jpgsize, unsigned char* outbgr) const
{
    return d->decode(jpgdata, jpgsize, outbgr);
}

int jpeg_decoder_aw::deinit()
{
    return d->deinit();
}

} // namespace cv

#else // defined __linux__

namespace cv {

bool jpeg_decoder_aw::supported(const unsigned char* /*jpgdata*/, int /*jpgsize*/)
{
    return false;
}

jpeg_decoder_aw::jpeg_decoder_aw() : d(0)
{
}

jpeg_decoder_aw::~jpeg_decoder_aw()
{
}

int jpeg_decoder_aw::init(const unsigned char* /*jpgdata*/, int /*jpgsize*/, int* /*width*/, int* /*height*/, int* /*ch*/)
{
    return -1;
}

int jpeg_decoder_aw::decode(const unsigned char* /*jpgdata*/, int /*jpgsize*/, unsigned char* /*outbgr*/) const
{
    return -1;
}

int jpeg_decoder_aw::deinit()
{
    return -1;
}

} // namespace cv

#endif // defined __linux__
