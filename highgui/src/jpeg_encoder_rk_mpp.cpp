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

#include "jpeg_encoder_rk_mpp.h"

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

extern "C" {

#define MPP_SUCCESS 0

#define MPP_CTX_ENC 1
#define MPP_VIDEO_CodingMJPEG 8

#define MPP_FMT_YUV420SP 0x0
#define MPP_FMT_BGR888   0x10007

#define MPP_ENC_SET_CFG 0x320001
#define MPP_ENC_GET_CFG 0x320002

#define VALLOC_IOCTL_MB_GET_FD 0xc01c6101

#define MPP_ALIGN(x, a) (((x)+(a)-1)&~((a)-1))

typedef uint32_t RK_U32;
typedef uint64_t RK_U64;
typedef int32_t RK_S32;
typedef int64_t RK_S64;

typedef int MPP_RET;
typedef int MpiCmd;
typedef int MppCodingType;
typedef int MppCtxType;
typedef int MppFrameFormat;
typedef int MppPollType;
typedef int MppPortType;

typedef void* MppBuffer;
typedef void* MppBufferGroup;
typedef void* MppCtx;
typedef void* MppEncCfg;
typedef void* MppFrame;
typedef void* MppPacket;
typedef void* MppParam;
typedef void* MppTask;

typedef struct MppApi_t
{
    RK_U32 size;
    RK_U32 version;
    MPP_RET (*decode)(MppCtx ctx, MppPacket packet, MppFrame *frame);
    MPP_RET (*decode_put_packet)(MppCtx ctx, MppPacket packet);
    MPP_RET (*decode_get_frame)(MppCtx ctx, MppFrame *frame);
    MPP_RET (*encode)(MppCtx ctx, MppFrame frame, MppPacket *packet);
    MPP_RET (*encode_put_frame)(MppCtx ctx, MppFrame frame);
    MPP_RET (*encode_get_packet)(MppCtx ctx, MppPacket *packet);
    MPP_RET (*isp)(MppCtx ctx, MppFrame dst, MppFrame src);
    MPP_RET (*isp_put_frame)(MppCtx ctx, MppFrame frame);
    MPP_RET (*isp_get_frame)(MppCtx ctx, MppFrame *frame);
    MPP_RET (*poll)(MppCtx ctx, MppPortType type, MppPollType timeout);
    MPP_RET (*dequeue)(MppCtx ctx, MppPortType type, MppTask *task);
    MPP_RET (*enqueue)(MppCtx ctx, MppPortType type, MppTask task);
    MPP_RET (*reset)(MppCtx ctx);
    MPP_RET (*control)(MppCtx ctx, MpiCmd cmd, MppParam param);
    MPP_RET (*encode_release_packet)(MppCtx ctx, MppPacket *packet);
    RK_U32 reserv[16];
} MppApi;

struct venc_pack_info
{
    RK_U32 flag;
    RK_U32 temporal_id;
    RK_U32 packet_offset;
    RK_U32 packet_len;
};

struct venc_packet
{
    RK_U64 u64priv_data;
    RK_U64 u64packet_addr;
    RK_U32 len;
    RK_U32 buf_size;

    RK_U64 u64pts;
    RK_U64 u64dts;
    RK_U32 flag;
    RK_U32 temporal_id;
    RK_U32 offset;
    RK_U32 data_num;
    struct venc_pack_info packet[8];
};

struct valloc_mb
{
    int pool_id;
    int mpi_buf_id;
    int dma_buf_fd;
    int struct_size;
    int size;
    int flags;
    int paddr;
};

#define MODULE_TAG NULL
#define mpp_buffer_get(group, buffer, size) mpp_buffer_get_with_tag(group, buffer, size, MODULE_TAG, __FUNCTION__)
#define mpp_buffer_put(buffer) mpp_buffer_put_with_caller(buffer, __FUNCTION__)
#define mpp_buffer_get_ptr(buffer) mpp_buffer_get_ptr_with_caller(buffer, __FUNCTION__)
typedef MPP_RET (*PFN_mpp_buffer_get_with_tag)(MppBufferGroup group, MppBuffer *buffer, size_t size, const char *tag, const char *caller);
typedef MPP_RET (*PFN_mpp_buffer_put_with_caller)(MppBuffer buffer, const char *caller);
typedef void* (*PFN_mpp_buffer_get_ptr_with_caller)(MppBuffer buffer, const char *caller);

typedef MPP_RET (*PFN_mpp_frame_init)(MppFrame *frame);
typedef MPP_RET (*PFN_mpp_frame_deinit)(MppFrame *frame);
typedef void (*PFN_mpp_frame_set_width)(MppFrame frame, RK_U32 width);
typedef void (*PFN_mpp_frame_set_height)(MppFrame frame, RK_U32 height);
typedef void (*PFN_mpp_frame_set_hor_stride)(MppFrame frame, RK_U32 hor_stride);
typedef void (*PFN_mpp_frame_set_ver_stride)(MppFrame frame, RK_U32 ver_stride);
typedef void (*PFN_mpp_frame_set_pts)(MppFrame frame, RK_S64 pts);
typedef void (*PFN_mpp_frame_set_eos)(MppFrame frame, RK_U32 eos);
typedef void (*PFN_mpp_frame_set_jpege_chan_id)(MppFrame frame, RK_S32 chan_id);
typedef void (*PFN_mpp_frame_set_buffer)(MppFrame frame, MppBuffer buffer);
typedef void (*PFN_mpp_frame_set_fmt)(MppFrame frame, MppFrameFormat fmt);

typedef MPP_RET (*PFN_mpp_create_ext)(MppCtx *ctx, MppApi **mpi, int flags);
typedef MPP_RET (*PFN_mpp_init)(MppCtx ctx, MppCtxType type, MppCodingType coding);
typedef MPP_RET (*PFN_mpp_destroy)(MppCtx ctx);

typedef MPP_RET (*PFN_mpp_enc_cfg_init)(MppEncCfg *cfg);
typedef MPP_RET (*PFN_mpp_enc_cfg_deinit)(MppEncCfg cfg);
typedef MPP_RET (*PFN_mpp_enc_cfg_set_s32)(MppEncCfg cfg, const char *name, RK_S32 val);
typedef MPP_RET (*PFN_mpp_enc_cfg_set_u32)(MppEncCfg cfg, const char *name, RK_U32 val);

} // extern "C"

static void* librockchip_mpp = 0;

static PFN_mpp_buffer_get_with_tag mpp_buffer_get_with_tag = 0;
static PFN_mpp_buffer_put_with_caller mpp_buffer_put_with_caller = 0;
static PFN_mpp_buffer_get_ptr_with_caller mpp_buffer_get_ptr_with_caller = 0;

static PFN_mpp_frame_init mpp_frame_init = 0;
static PFN_mpp_frame_deinit mpp_frame_deinit = 0;
static PFN_mpp_frame_set_width mpp_frame_set_width = 0;
static PFN_mpp_frame_set_height mpp_frame_set_height = 0;
static PFN_mpp_frame_set_hor_stride mpp_frame_set_hor_stride    = 0;
static PFN_mpp_frame_set_ver_stride mpp_frame_set_ver_stride    = 0;
static PFN_mpp_frame_set_pts mpp_frame_set_pts = 0;
static PFN_mpp_frame_set_eos mpp_frame_set_eos = 0;
static PFN_mpp_frame_set_jpege_chan_id mpp_frame_set_jpege_chan_id  = 0;
static PFN_mpp_frame_set_buffer mpp_frame_set_buffer  = 0;
static PFN_mpp_frame_set_fmt mpp_frame_set_fmt = 0;

static PFN_mpp_create_ext mpp_create_ext = 0;
static PFN_mpp_init mpp_init = 0;
static PFN_mpp_destroy mpp_destroy = 0;

static PFN_mpp_enc_cfg_init mpp_enc_cfg_init = 0;
static PFN_mpp_enc_cfg_deinit mpp_enc_cfg_deinit = 0;
static PFN_mpp_enc_cfg_set_s32 mpp_enc_cfg_set_s32 = 0;
static PFN_mpp_enc_cfg_set_u32 mpp_enc_cfg_set_u32 = 0;

static int load_rkmpp_library()
{
    if (librockchip_mpp)
        return 0;

    // check device whitelist
    bool whitelisted = false;
    {
        FILE* fp = fopen("/proc/device-tree/model", "rb");
        if (!fp)
            return -1;

        char buf[1024];
        fgets(buf, 1024, fp);
        fclose(fp);

        if (strncmp(buf, "Luckfox Pico", 12) == 0)
        {
            // luckfox pico family and plus pro max mini variants
            whitelisted = true;
        }
    }

    if (!whitelisted)
    {
        fprintf(stderr, "this device is not \n");
        return -1;
    }

    librockchip_mpp = dlopen("librockchip_mpp.so", RTLD_LOCAL | RTLD_NOW);
    if (!librockchip_mpp)
    {
        librockchip_mpp = dlopen("librockchip_mpp.so.0", RTLD_LOCAL | RTLD_NOW);
    }
    if (!librockchip_mpp)
    {
        librockchip_mpp = dlopen("librockchip_mpp.so.1", RTLD_LOCAL | RTLD_NOW);
    }
    if (!librockchip_mpp)
    {
        librockchip_mpp = dlopen("/oem/usr/lib/librockchip_mpp.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!librockchip_mpp)
    {
        return -1;
    }

    mpp_buffer_get_with_tag = (PFN_mpp_buffer_get_with_tag)dlsym(librockchip_mpp, "mpp_buffer_get_with_tag");
    mpp_buffer_put_with_caller = (PFN_mpp_buffer_put_with_caller)dlsym(librockchip_mpp, "mpp_buffer_put_with_caller");
    mpp_buffer_get_ptr_with_caller = (PFN_mpp_buffer_get_ptr_with_caller)dlsym(librockchip_mpp, "mpp_buffer_get_ptr_with_caller");

    mpp_frame_init = (PFN_mpp_frame_init)dlsym(librockchip_mpp, "mpp_frame_init");
    mpp_frame_deinit = (PFN_mpp_frame_deinit)dlsym(librockchip_mpp, "mpp_frame_deinit");
    mpp_frame_set_width = (PFN_mpp_frame_set_width)dlsym(librockchip_mpp, "mpp_frame_set_width");
    mpp_frame_set_height = (PFN_mpp_frame_set_height)dlsym(librockchip_mpp, "mpp_frame_set_height");
    mpp_frame_set_hor_stride = (PFN_mpp_frame_set_hor_stride)dlsym(librockchip_mpp,"mpp_frame_set_hor_stride");
    mpp_frame_set_ver_stride = (PFN_mpp_frame_set_ver_stride)dlsym(librockchip_mpp,"mpp_frame_set_ver_stride");
    mpp_frame_set_pts = (PFN_mpp_frame_set_pts)dlsym(librockchip_mpp, "mpp_frame_set_pts");
    mpp_frame_set_eos = (PFN_mpp_frame_set_eos)dlsym(librockchip_mpp, "mpp_frame_set_eos");
    mpp_frame_set_jpege_chan_id = (PFN_mpp_frame_set_jpege_chan_id)dlsym(librockchip_mpp,"mpp_frame_set_jpege_chan_id");
    mpp_frame_set_buffer = (PFN_mpp_frame_set_buffer)dlsym(librockchip_mpp, "mpp_frame_set_buffer");
    mpp_frame_set_fmt = (PFN_mpp_frame_set_fmt)dlsym(librockchip_mpp, "mpp_frame_set_fmt");

    mpp_create_ext = (PFN_mpp_create_ext)dlsym(librockchip_mpp, "mpp_create_ext");
    mpp_init = (PFN_mpp_init)dlsym(librockchip_mpp, "mpp_init");
    mpp_destroy = (PFN_mpp_destroy)dlsym(librockchip_mpp, "mpp_destroy");

    mpp_enc_cfg_init = (PFN_mpp_enc_cfg_init)dlsym(librockchip_mpp, "mpp_enc_cfg_init");
    mpp_enc_cfg_deinit = (PFN_mpp_enc_cfg_deinit)dlsym(librockchip_mpp, "mpp_enc_cfg_deinit");
    mpp_enc_cfg_set_s32 = (PFN_mpp_enc_cfg_set_s32)dlsym(librockchip_mpp, "mpp_enc_cfg_set_s32");
    mpp_enc_cfg_set_u32 = (PFN_mpp_enc_cfg_set_u32)dlsym(librockchip_mpp, "mpp_enc_cfg_set_u32");

    return 0;
}

static int unload_rkmpp_library()
{
    if (!librockchip_mpp)
        return 0;

    dlclose(librockchip_mpp);
    librockchip_mpp = 0;

    mpp_buffer_get_with_tag = 0;
    mpp_buffer_put_with_caller = 0;
    mpp_buffer_get_ptr_with_caller = 0;

    mpp_frame_init = 0;
    mpp_frame_deinit = 0;
    mpp_frame_set_width = 0;
    mpp_frame_set_height = 0;
    mpp_frame_set_hor_stride = 0;
    mpp_frame_set_ver_stride = 0;
    mpp_frame_set_pts = 0;
    mpp_frame_set_eos = 0;
    mpp_frame_set_jpege_chan_id = 0;
    mpp_frame_set_buffer = 0;
    mpp_frame_set_fmt = 0;

    mpp_create_ext = 0;
    mpp_init = 0;
    mpp_destroy = 0;

    mpp_enc_cfg_init = 0;
    mpp_enc_cfg_deinit = 0;
    mpp_enc_cfg_set_s32 = 0;
    mpp_enc_cfg_set_u32 = 0;

    return 0;
}

class rkmpp_library_loader
{
public:
    bool ready;

    rkmpp_library_loader()
    {
        ready = (load_rkmpp_library() == 0);
    }

    ~rkmpp_library_loader()
    {
        unload_rkmpp_library();
    }
};

static rkmpp_library_loader rkmpp;

class jpeg_encoder_rk_mpp_impl
{
public:
    jpeg_encoder_rk_mpp_impl();
    ~jpeg_encoder_rk_mpp_impl();

    int init(int width, int height, int ch, int quality);

    int encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const;

    int encode(const unsigned char* bgrdata, const char* outfilepath) const;

    int deinit();

protected:
    int inited;
    int mem_fd;
    MppBufferGroup buf_grp;
    MppCtx ctx;
    MppApi* mpi;
    MppEncCfg cfg;
    MppFrame frame;
    MppBuffer buffer;
    int width;
    int height;
    int ch;
    MppFrameFormat format;
    int hor_stride;
    int ver_stride;
    int frame_size;
};

jpeg_encoder_rk_mpp_impl::jpeg_encoder_rk_mpp_impl()
{
    inited = 0;
    mem_fd = -1;
    buf_grp = 0;
    ctx = 0;
    mpi = 0;
    cfg = 0;
    frame = 0;
    buffer = 0;
    width = 0;
    height = 0;
    ch = 0;
    format = 0;
    hor_stride = 0;
    ver_stride = 0;
    frame_size = 0;
}

jpeg_encoder_rk_mpp_impl::~jpeg_encoder_rk_mpp_impl()
{
    deinit();
}

int jpeg_encoder_rk_mpp_impl::init(int _width, int _height, int _ch, int quality)
{
    if (!rkmpp.ready)
    {
        fprintf(stderr, "rkmpp not ready\n");
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

    if (ch == 1)
    {
        format = MPP_FMT_YUV420SP;
        hor_stride = MPP_ALIGN(width, 8);
        ver_stride = height;
        frame_size = MPP_ALIGN(hor_stride, 64) * MPP_ALIGN(ver_stride, 64) * 3 / 2;
    }
    if (ch == 3 || ch == 4)
    {
        format = MPP_FMT_BGR888;
        hor_stride = MPP_ALIGN(width, 8) * 3;// for vepu limitation
        ver_stride = height;
        frame_size = MPP_ALIGN(hor_stride, 64) * MPP_ALIGN(ver_stride, 64);
    }

    // fprintf(stderr, "width = %d\n", width);
    // fprintf(stderr, "height = %d\n", height);
    // fprintf(stderr, "ch = %d\n", ch);
    // fprintf(stderr, "hor_stride = %d\n", hor_stride);
    // fprintf(stderr, "ver_stride = %d\n", ver_stride);
    // fprintf(stderr, "frame_size = %d\n", frame_size);

    {
        mem_fd = open("/dev/mpi/valloc", O_RDWR | O_CLOEXEC);
        if (mem_fd == -1)
        {
            fprintf(stderr, "open /dev/mpi/valloc failed\n");
            return -1;
        }
    }

    {
        MPP_RET ret = mpp_buffer_get(buf_grp, &buffer, frame_size);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpp_buffer_get failed %d\n", ret);
            return -1;
        }
    }

    // fprintf(stderr, "buffer got\n");

    {
        MPP_RET ret = mpp_create_ext(&ctx, &mpi, 0);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpp_create_ext failed %d\n", ret);
            return -1;
        }
    }

    {
        MPP_RET ret = mpp_init(ctx, MPP_CTX_ENC, MPP_VIDEO_CodingMJPEG);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpp_init failed %d\n", ret);
            return -1;
        }
    }

    // fprintf(stderr, "mpp_init done\n");

    {
        MPP_RET ret = mpp_enc_cfg_init(&cfg);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpp_enc_cfg_init failed %d\n", ret);
            return -1;
        }
    }

    {
        MPP_RET ret = mpi->control(ctx, MPP_ENC_GET_CFG, cfg);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpi control MPP_ENC_GET_CFG failed %d\n", ret);
            return -1;
        }
    }

    {
        mpp_enc_cfg_set_s32(cfg, "prep:width", width);
        mpp_enc_cfg_set_s32(cfg, "prep:height", height);
        mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", hor_stride);
        mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", ver_stride);
        mpp_enc_cfg_set_s32(cfg, "prep:format", format);
        mpp_enc_cfg_set_s32(cfg, "prep:rotation", 0);
        mpp_enc_cfg_set_s32(cfg, "prep:mirroring", 0);

        mpp_enc_cfg_set_s32(cfg, "rc:mode", 2);// MPP_ENC_RC_MODE_FIXQP

        mpp_enc_cfg_set_s32(cfg, "rc:fps_in_flex", 1);
        mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", 1);
        mpp_enc_cfg_set_s32(cfg, "rc:fps_in_denorm", 1);
        mpp_enc_cfg_set_s32(cfg, "rc:fps_out_flex", 1);
        mpp_enc_cfg_set_s32(cfg, "rc:fps_out_num", 1);
        mpp_enc_cfg_set_s32(cfg, "rc:fps_out_denorm", 1);
        mpp_enc_cfg_set_s32(cfg, "rc:gop", 1);

        mpp_enc_cfg_set_u32(cfg, "rc:drop_mode", 0);
        mpp_enc_cfg_set_u32(cfg, "rc:drop_thd", 20);
        mpp_enc_cfg_set_u32(cfg, "rc:drop_gap", 1);

        mpp_enc_cfg_set_s32(cfg, "jpeg:q_factor", quality);
        mpp_enc_cfg_set_s32(cfg, "jpeg:qf_max", 99);
        mpp_enc_cfg_set_s32(cfg, "jpeg:qf_min", 10);

        mpp_enc_cfg_set_s32(cfg, "codec:type", MPP_VIDEO_CodingMJPEG);
    }

    {
        MPP_RET ret = mpi->control(ctx, MPP_ENC_SET_CFG, cfg);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpi control MPP_ENC_SET_CFG failed %d\n", ret);
            return -1;
        }
    }

    // fprintf(stderr, "control done\n");

    {
        MPP_RET ret = mpp_frame_init(&frame);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpp_frame_init failed %d\n", ret);
            return -1;
        }
    }

    {
        mpp_frame_set_width(frame, width);
        mpp_frame_set_height(frame, height);
        mpp_frame_set_hor_stride(frame, hor_stride);
        mpp_frame_set_ver_stride(frame, ver_stride);
        mpp_frame_set_fmt(frame, format);
        mpp_frame_set_pts(frame, 0);
        mpp_frame_set_eos(frame, 1);
        mpp_frame_set_jpege_chan_id(frame, -1);
    }

    {
        mpp_frame_set_buffer(frame, buffer);
    }

    inited = 1;

    return 0;
}

static inline void my_memset(unsigned char* dst, unsigned char c, int size)
{
#if __ARM_NEON
    uint8x16_t _p = vdupq_n_u8(c);
    int nn = size / 64;
    size -= nn * 64;
    while (nn--)
    {
        vst1q_u8(dst, _p);
        vst1q_u8(dst + 16, _p);
        vst1q_u8(dst + 32, _p);
        vst1q_u8(dst + 48, _p);
        dst += 64;
    }
    if (size > 16)
    {
        vst1q_u8(dst, _p);
        dst += 16;
        size -= 16;
    }
    if (size > 8)
    {
        vst1_u8(dst, vget_low_u8(_p));
        dst += 8;
        size -= 8;
    }
    while (size--)
    {
        *dst++ = c;
    }
#else
    memset(dst, c, size);
#endif
}

static inline void my_memcpy(unsigned char* dst, const unsigned char* src, int size)
{
#if __ARM_NEON
    int nn = size / 64;
    size -= nn * 64;
    while (nn--)
    {
        __builtin_prefetch(src + 64);
        uint8x16_t _p0 = vld1q_u8(src);
        uint8x16_t _p1 = vld1q_u8(src + 16);
        uint8x16_t _p2 = vld1q_u8(src + 32);
        uint8x16_t _p3 = vld1q_u8(src + 48);
        vst1q_u8(dst, _p0);
        vst1q_u8(dst + 16, _p1);
        vst1q_u8(dst + 32, _p2);
        vst1q_u8(dst + 48, _p3);
        src += 64;
        dst += 64;
    }
    if (size > 16)
    {
        uint8x16_t _p0 = vld1q_u8(src);
        vst1q_u8(dst, _p0);
        src += 16;
        dst += 16;
        size -= 16;
    }
    if (size > 8)
    {
        uint8x8_t _p0 = vld1_u8(src);
        vst1_u8(dst, _p0);
        src += 8;
        dst += 8;
        size -= 8;
    }
    while (size--)
    {
        *dst++ = *src++;
    }
#else
    memcpy(dst, src, size);
#endif
}

static inline void my_memcpy_drop_alpha(unsigned char* dst, const unsigned char* src, int w)
{
#if __ARM_NEON
    int nn = w / 16;
    w -= nn * 16;
    while (nn--)
    {
        __builtin_prefetch(src + 64);
        uint8x16x4_t _p0 = vld4q_u8(src);
        uint8x16x3_t _p1;
        _p1.val[0] = _p0.val[0];
        _p1.val[1] = _p0.val[1];
        _p1.val[2] = _p0.val[2];
        vst3q_u8(dst, _p1);
        src += 64;
        dst += 48;
    }
    if (w > 8)
    {
        uint8x8x4_t _p0 = vld4_u8(src);
        uint8x8x3_t _p1;
        _p1.val[0] = _p0.val[0];
        _p1.val[1] = _p0.val[1];
        _p1.val[2] = _p0.val[2];
        vst3_u8(dst, _p1);
        src += 32;
        dst += 24;
        w -= 8;
    }
#endif
    while (w--)
    {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        src += 4;
        dst += 3;
    }
}

int jpeg_encoder_rk_mpp_impl::encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const
{
    outdata.clear();

    if (!inited)
    {
        fprintf(stderr, "not inited\n");
        return -1;
    }

    int ret_val = 0;

    struct venc_packet enc_packet;
    memset(&enc_packet, 0, sizeof(enc_packet));

    MppPacket packet = &enc_packet;

    int b_packet_got = 0;

    {
        void* mapped_ptr = mpp_buffer_get_ptr(buffer);

        if (format == MPP_FMT_YUV420SP)
        {
            for (int i = 0; i < height; i++)
            {
                my_memcpy((unsigned char*)mapped_ptr + i * hor_stride, bgrdata + i * width, width);
            }
            my_memset((unsigned char*)mapped_ptr + height * hor_stride, 128, height * hor_stride / 2);
        }
        if (format == MPP_FMT_BGR888)
        {
            if (ch == 3)
            {
                for (int i = 0; i < height; i++)
                {
                    my_memcpy((unsigned char*)mapped_ptr + i * hor_stride, bgrdata + i * width * 3, width * 3);
                }
            }
            if (ch == 4)
            {
                for (int i = 0; i < height; i++)
                {
                    my_memcpy_drop_alpha((unsigned char*)mapped_ptr + i * hor_stride, bgrdata + i * width * 4, width);
                }
            }
        }
    }

    // fprintf(stderr, "before encode\n");

    {
        MPP_RET ret = mpi->encode_put_frame(ctx, frame);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpi encode_put_frame failed %d\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    {
        MPP_RET ret = mpi->encode_get_packet(ctx, &packet);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpi encode_get_packet failed %d\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_packet_got = 1;
    }

    if (enc_packet.len <= 0)
    {
        fprintf(stderr, "enc_packet empty\n");
        ret_val = -1;
        goto OUT;
    }

    // consume packet
    {
        struct valloc_mb mb;
        memset(&mb, 0, sizeof(mb));
        mb.struct_size = sizeof(mb);
        mb.mpi_buf_id = enc_packet.u64priv_data;

        int ret = ioctl(mem_fd, VALLOC_IOCTL_MB_GET_FD, &mb);
        if (ret == -1)
        {
            fprintf(stderr, "ioctl VALLOC_IOCTL_MB_GET_FD failed\n");
            ret_val = -1;
            goto OUT;
        }

        void* src_ptr = mmap(NULL, enc_packet.buf_size, PROT_READ, MAP_SHARED, mb.dma_buf_fd, 0);
        if (src_ptr == MAP_FAILED)
        {
            fprintf(stderr, "mmap %d failed\n", enc_packet.buf_size);
            close(mb.dma_buf_fd);
            ret_val = -1;
            goto OUT;
        }

        const unsigned char* p0 = (const unsigned char*)src_ptr + enc_packet.offset;

        outdata.resize(enc_packet.len);
        my_memcpy(outdata.data(), p0, enc_packet.len);

        munmap(src_ptr, enc_packet.buf_size);

        close(mb.dma_buf_fd);
    }

OUT:

    if (b_packet_got)
    {
        MPP_RET ret = mpi->encode_release_packet(ctx, &packet);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpi encode_release_packet failed %d\n", ret);
            return -1;
        }
    }

    return ret_val;
}

int jpeg_encoder_rk_mpp_impl::encode(const unsigned char* bgrdata, const char* outfilepath) const
{
    if (!inited)
    {
        fprintf(stderr, "not inited\n");
        return -1;
    }

    int ret_val = 0;

    struct venc_packet enc_packet;
    memset(&enc_packet, 0, sizeof(enc_packet));

    MppPacket packet = &enc_packet;

    int b_packet_got = 0;

    FILE* outfp = 0;

    {
        void* mapped_ptr = mpp_buffer_get_ptr(buffer);

        if (format == MPP_FMT_YUV420SP)
        {
            for (int i = 0; i < height; i++)
            {
                my_memcpy((unsigned char*)mapped_ptr + i * hor_stride, bgrdata + i * width, width);
            }
            my_memset((unsigned char*)mapped_ptr + height * hor_stride, 128, height * hor_stride / 2);
        }
        if (format == MPP_FMT_BGR888)
        {
            if (ch == 3)
            {
                for (int i = 0; i < height; i++)
                {
                    my_memcpy((unsigned char*)mapped_ptr + i * hor_stride, bgrdata + i * width * 3, width * 3);
                }
            }
            if (ch == 4)
            {
                for (int i = 0; i < height; i++)
                {
                    my_memcpy_drop_alpha((unsigned char*)mapped_ptr + i * hor_stride, bgrdata + i * width * 4, width);
                }
            }
        }
    }

    // fprintf(stderr, "before encode\n");

    {
        MPP_RET ret = mpi->encode_put_frame(ctx, frame);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpi encode_put_frame failed %d\n", ret);
            ret_val = -1;
            goto OUT;
        }
    }

    {
        MPP_RET ret = mpi->encode_get_packet(ctx, &packet);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpi encode_get_packet failed %d\n", ret);
            ret_val = -1;
            goto OUT;
        }

        b_packet_got = 1;
    }

    if (enc_packet.len <= 0)
    {
        fprintf(stderr, "enc_packet empty\n");
        ret_val = -1;
        goto OUT;
    }

    {
        outfp = fopen(outfilepath, "wb");
        if (!outfp)
        {
            fprintf(stderr, "fopen %s failed\n", outfilepath);
            ret_val = -1;
            goto OUT;
        }
    }

    // consume packet
    {
        struct valloc_mb mb;
        memset(&mb, 0, sizeof(mb));
        mb.struct_size = sizeof(mb);
        mb.mpi_buf_id = enc_packet.u64priv_data;

        int ret = ioctl(mem_fd, VALLOC_IOCTL_MB_GET_FD, &mb);
        if (ret == -1)
        {
            fprintf(stderr, "ioctl VALLOC_IOCTL_MB_GET_FD failed\n");
            ret_val = -1;
            goto OUT;
        }

        void* src_ptr = mmap(NULL, enc_packet.buf_size, PROT_READ, MAP_SHARED, mb.dma_buf_fd, 0);
        if (src_ptr == MAP_FAILED)
        {
            fprintf(stderr, "mmap %d failed\n", enc_packet.buf_size);
            close(mb.dma_buf_fd);
            ret_val = -1;
            goto OUT;
        }

        const unsigned char* p0 = (const unsigned char*)src_ptr + enc_packet.offset;

        size_t nwrite = fwrite(p0, 1, enc_packet.len, outfp);
        if (nwrite != enc_packet.len)
        {
            fprintf(stderr, "fwrite %d failed\n", enc_packet.len);
            munmap(src_ptr, enc_packet.buf_size);
            close(mb.dma_buf_fd);
            ret_val = -1;
            goto OUT;
        }

        munmap(src_ptr, enc_packet.buf_size);

        close(mb.dma_buf_fd);
    }

OUT:

    if (b_packet_got)
    {
        MPP_RET ret = mpi->encode_release_packet(ctx, &packet);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpi encode_release_packet failed %d\n", ret);
            return -1;
        }
    }

    if (outfp)
    {
        fclose(outfp);
    }

    return ret_val;
}

int jpeg_encoder_rk_mpp_impl::deinit()
{
    if (!inited)
        return 0;

    int ret_val = 0;

    if (buffer)
    {
        MPP_RET ret = mpp_buffer_put(buffer);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpp_buffer_put failed %d\n", ret);
            ret_val = -1;
        }

        buffer = 0;
    }

    if (frame)
    {
        MPP_RET ret = mpp_frame_deinit(&frame);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpp_frame_deinit failed %d\n", ret);
            ret_val = -1;
        }

        frame = 0;
    }

    if (cfg)
    {
        MPP_RET ret = mpp_enc_cfg_deinit(cfg);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpp_enc_cfg_deinit failed %d\n", ret);
            ret_val = -1;
        }

        cfg = 0;
    }

    if (ctx)
    {
        MPP_RET ret = mpp_destroy(ctx);
        if (ret != MPP_SUCCESS)
        {
            fprintf(stderr, "mpp_destroy failed %d\n", ret);
            ret_val = -1;
        }

        ctx = 0;
    }

    if (mem_fd != -1)
    {
        close(mem_fd);
        mem_fd = -1;
    }

    buf_grp = 0;
    mpi = 0;
    width = 0;
    height = 0;
    ch = 0;
    format = 0;
    hor_stride = 0;
    ver_stride = 0;
    frame_size = 0;

    inited = 0;

    return ret_val;
}

bool jpeg_encoder_rk_mpp::supported(int width, int height, int ch)
{
    if (!rkmpp.ready)
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

jpeg_encoder_rk_mpp::jpeg_encoder_rk_mpp() : d(new jpeg_encoder_rk_mpp_impl)
{
}

jpeg_encoder_rk_mpp::~jpeg_encoder_rk_mpp()
{
    delete d;
}

int jpeg_encoder_rk_mpp::init(int width, int height, int ch, int quality)
{
    return d->init(width, height, ch, quality);
}

int jpeg_encoder_rk_mpp::encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const
{
    return d->encode(bgrdata, outdata);
}

int jpeg_encoder_rk_mpp::encode(const unsigned char* bgrdata, const char* outfilepath) const
{
    return d->encode(bgrdata, outfilepath);
}

int jpeg_encoder_rk_mpp::deinit()
{
    return d->deinit();
}

#else // defined __linux__

bool jpeg_encoder_rk_mpp::supported(int /*width*/, int /*height*/, int /*ch*/)
{
    return false;
}

jpeg_encoder_rk_mpp::jpeg_encoder_rk_mpp()
{
}

jpeg_encoder_rk_mpp::~jpeg_encoder_rk_mpp()
{
}

int jpeg_encoder_rk_mpp::init(int /*width*/, int /*height*/, int /*ch*/, int /*quality*/)
{
    return -1;
}

int jpeg_encoder_rk_mpp::encode(const unsigned char* /*bgrdata*/, std::vector<unsigned char>& /*outdata*/) const
{
    return -1;
}

int jpeg_encoder_rk_mpp::encode(const unsigned char* /*bgrdata*/, const char* /*outfilepath*/) const
{
    return -1;
}

int jpeg_encoder_rk_mpp::deinit()
{
    return -1;
}

#endif // defined __linux__
