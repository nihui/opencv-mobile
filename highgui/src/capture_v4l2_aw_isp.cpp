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

#include "capture_v4l2_aw_isp.h"

#if defined __linux__
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#if __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON


namespace cv {

// 0 = unknown
// 1 = tinyvision
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

        if (strncmp(buf, "sun8iw21", 8) == 0)
        {
            // tinyvision
            device_model = 1;
        }
    }

    if (device_model > 0)
    {
        fprintf(stderr, "opencv-mobile MIPI CSI camera with v4l2 awispapi\n");
    }

    return device_model;
}

static bool is_device_whitelisted()
{
    return get_device_model() > 0;
}

extern "C" {

// #include <linux/g2d_driver.h>

typedef enum {
    G2D_FORMAT_ARGB8888,
    G2D_FORMAT_ABGR8888,
    G2D_FORMAT_RGBA8888,
    G2D_FORMAT_BGRA8888,
    G2D_FORMAT_XRGB8888,
    G2D_FORMAT_XBGR8888,
    G2D_FORMAT_RGBX8888,
    G2D_FORMAT_BGRX8888,
    G2D_FORMAT_RGB888,
    G2D_FORMAT_BGR888,
    G2D_FORMAT_RGB565,
    G2D_FORMAT_BGR565,
    G2D_FORMAT_ARGB4444,
    G2D_FORMAT_ABGR4444,
    G2D_FORMAT_RGBA4444,
    G2D_FORMAT_BGRA4444,
    G2D_FORMAT_ARGB1555,
    G2D_FORMAT_ABGR1555,
    G2D_FORMAT_RGBA5551,
    G2D_FORMAT_BGRA5551,
    G2D_FORMAT_ARGB2101010,
    G2D_FORMAT_ABGR2101010,
    G2D_FORMAT_RGBA1010102,
    G2D_FORMAT_BGRA1010102,

    /* invailed for UI channel */
    G2D_FORMAT_IYUV422_V0Y1U0Y0 = 0x20,
    G2D_FORMAT_IYUV422_Y1V0Y0U0,
    G2D_FORMAT_IYUV422_U0Y1V0Y0,
    G2D_FORMAT_IYUV422_Y1U0Y0V0,

    G2D_FORMAT_YUV422UVC_V1U1V0U0,
    G2D_FORMAT_YUV422UVC_U1V1U0V0,
    G2D_FORMAT_YUV422_PLANAR,

    G2D_FORMAT_YUV420UVC_V1U1V0U0 = 0x28,
    G2D_FORMAT_YUV420UVC_U1V1U0V0,
    G2D_FORMAT_YUV420_PLANAR,

    G2D_FORMAT_YUV411UVC_V1U1V0U0 = 0x2c,
    G2D_FORMAT_YUV411UVC_U1V1U0V0,
    G2D_FORMAT_YUV411_PLANAR,

    G2D_FORMAT_Y8 = 0x30,

    /* YUV 10bit format */
    G2D_FORMAT_YVU10_P010 = 0x34,

    G2D_FORMAT_YVU10_P210 = 0x36,

    G2D_FORMAT_YVU10_444 = 0x38,
    G2D_FORMAT_YUV10_444 = 0x39,
    G2D_FORMAT_MAX,
} g2d_fmt_enh;

typedef enum {
    G2D_BLT_NONE_H = 0x0,
    G2D_BLT_BLACKNESS,
    G2D_BLT_NOTMERGEPEN,
    G2D_BLT_MASKNOTPEN,
    G2D_BLT_NOTCOPYPEN,
    G2D_BLT_MASKPENNOT,
    G2D_BLT_NOT,
    G2D_BLT_XORPEN,
    G2D_BLT_NOTMASKPEN,
    G2D_BLT_MASKPEN,
    G2D_BLT_NOTXORPEN,
    G2D_BLT_NOP,
    G2D_BLT_MERGENOTPEN,
    G2D_BLT_COPYPEN,
    G2D_BLT_MERGEPENNOT,
    G2D_BLT_MERGEPEN,
    G2D_BLT_WHITENESS = 0x000000ff,

    G2D_ROT_90  = 0x00000100,
    G2D_ROT_180 = 0x00000200,
    G2D_ROT_270 = 0x00000300,
    G2D_ROT_0   = 0x00000400,
    G2D_ROT_H = 0x00001000,
    G2D_ROT_V = 0x00002000,

/*	G2D_SM_TDLR_1  =    0x10000000, */
    G2D_SM_DTLR_1 = 0x10000000,
/*	G2D_SM_TDRL_1  =    0x20000000, */
/*	G2D_SM_DTRL_1  =    0x30000000, */
} g2d_blt_flags_h;

typedef struct {
    __s32		x;		/* left top point coordinate x */
    __s32		y;		/* left top point coordinate y */
    __u32		w;		/* rectangle width */
    __u32		h;		/* rectangle height */
} g2d_rect;

typedef enum {
    G2D_BT601,
    G2D_BT709,
    G2D_BT2020,
} g2d_color_gmt;

typedef enum {
    G2D_PIXEL_ALPHA,
    G2D_GLOBAL_ALPHA,
    G2D_MIXER_ALPHA,
} g2d_alpha_mode_enh;

enum color_range {
    COLOR_RANGE_0_255 = 0,
    COLOR_RANGE_16_235 = 1,
};

typedef struct {
    int		 bbuff;
    __u32		 color;
    g2d_fmt_enh	 format;
    __u32		 laddr[3];
    __u32		 haddr[3];
    __u32		 width;
    __u32		 height;
    __u32		 align[3];

    g2d_rect	 clip_rect;

    g2d_color_gmt	 gamut;
    int		 bpremul;
    __u8		 alpha;
    g2d_alpha_mode_enh mode;
    int		 fd;
    __u32 use_phy_addr;
    enum color_range color_range;
} g2d_image_enh;

typedef struct {
    g2d_blt_flags_h flag_h;
    g2d_image_enh src_image_h;
    g2d_image_enh dst_image_h;
} g2d_blt_h;

typedef enum {
    G2D_CMD_BITBLT			=	0x50,
    G2D_CMD_FILLRECT		=	0x51,
    G2D_CMD_STRETCHBLT		=	0x52,
    G2D_CMD_PALETTE_TBL		=	0x53,
    G2D_CMD_QUEUE			=	0x54,
    G2D_CMD_BITBLT_H		=	0x55,
    G2D_CMD_FILLRECT_H		=	0x56,
    G2D_CMD_BLD_H			=	0x57,
    G2D_CMD_MASK_H			=	0x58,
} g2d_cmd;

// #include <linux/ion_uapi.h>

typedef int ion_user_handle_t;

enum ion_heap_type {
    ION_HEAP_TYPE_SYSTEM,
    ION_HEAP_TYPE_SYSTEM_CONTIG,
    ION_HEAP_TYPE_CARVEOUT,
    ION_HEAP_TYPE_CHUNK,
    ION_HEAP_TYPE_DMA,
    ION_HEAP_TYPE_CUSTOM,
    ION_HEAP_TYPE_SECURE, /* allwinner add */
};

#define ION_HEAP_SYSTEM_MASK        (1 << ION_HEAP_TYPE_SYSTEM)
#define ION_HEAP_SYSTEM_CONTIG_MASK (1 << ION_HEAP_TYPE_SYSTEM_CONTIG)
#define ION_HEAP_CARVEOUT_MASK      (1 << ION_HEAP_TYPE_CARVEOUT)
#define ION_HEAP_TYPE_DMA_MASK      (1 << ION_HEAP_TYPE_DMA)
#define ION_NUM_HEAP_IDS            (sizeof(unsigned int) * 8)

#define ION_FLAG_CACHED             1
#define ION_FLAG_CACHED_NEEDS_SYNC  2

struct ion_allocation_data {
    size_t len;
    size_t align;
    unsigned int heap_id_mask;
    unsigned int flags;
    ion_user_handle_t handle;
};

struct ion_fd_data {
    ion_user_handle_t handle;
    int fd;
};

struct ion_handle_data {
    ion_user_handle_t handle;
};

#define ION_IOC_MAGIC   'I'
#define ION_IOC_ALLOC   _IOWR(ION_IOC_MAGIC, 0, struct ion_allocation_data)
#define ION_IOC_FREE    _IOWR(ION_IOC_MAGIC, 1, struct ion_handle_data)
#define ION_IOC_MAP     _IOWR(ION_IOC_MAGIC, 2, struct ion_fd_data)

// #include <linux/sunxi_ion_uapi.h>

struct sunxi_cache_range {
    long start;
    long end;
};

#define ION_IOC_SUNXI_FLUSH_RANGE   5

// #include <linux/cedar_ve_uapi.h>

enum IOCTL_CMD {
    IOCTL_UNKOWN = 0x100,
    IOCTL_GET_ENV_INFO,
    IOCTL_WAIT_VE_DE,
    IOCTL_WAIT_VE_EN,
    IOCTL_RESET_VE,
    IOCTL_ENABLE_VE,
    IOCTL_DISABLE_VE,
    IOCTL_SET_VE_FREQ,

    IOCTL_CONFIG_AVS2 = 0x200,
    IOCTL_GETVALUE_AVS2,
    IOCTL_PAUSE_AVS2,
    IOCTL_START_AVS2,
    IOCTL_RESET_AVS2,
    IOCTL_ADJUST_AVS2,
    IOCTL_ENGINE_REQ,
    IOCTL_ENGINE_REL,
    IOCTL_ENGINE_CHECK_DELAY,
    IOCTL_GET_IC_VER,
    IOCTL_ADJUST_AVS2_ABS,
    IOCTL_FLUSH_CACHE,
    IOCTL_SET_REFCOUNT,
    IOCTL_FLUSH_CACHE_ALL,
    IOCTL_TEST_VERSION,

    IOCTL_GET_LOCK = 0x310,
    IOCTL_RELEASE_LOCK,

    IOCTL_SET_VOL = 0x400,

    IOCTL_WAIT_JPEG_DEC = 0x500,
    /*for get the ve ref_count for ipc to delete the semphore*/
    IOCTL_GET_REFCOUNT,

    /*for iommu*/
    IOCTL_GET_IOMMU_ADDR,
    IOCTL_FREE_IOMMU_ADDR,

    /*for debug*/
    IOCTL_SET_PROC_INFO,
    IOCTL_STOP_PROC_INFO,
    IOCTL_COPY_PROC_INFO,

    IOCTL_SET_DRAM_HIGH_CHANNAL = 0x600,

    /* debug for decoder and encoder*/
    IOCTL_PROC_INFO_COPY = 0x610,
    IOCTL_PROC_INFO_STOP,
    IOCTL_POWER_SETUP = 0x700,
    IOCTL_POWER_SHUTDOWN,
};

struct user_iommu_param {
    int				fd;
    unsigned int	iommu_addr;
};

// #include <media/sunxi_camera_v2.h>

#define V4L2_CID_USER_SUNXI_CAMERA_BASE (V4L2_CID_USER_BASE + 0x1050)

#define V4L2_CID_AUTO_FOCUS_INIT    (V4L2_CID_USER_SUNXI_CAMERA_BASE + 2)
#define V4L2_CID_AUTO_FOCUS_RELEASE (V4L2_CID_USER_SUNXI_CAMERA_BASE + 3)
#define V4L2_CID_GSENSOR_ROTATION   (V4L2_CID_USER_SUNXI_CAMERA_BASE + 4)
#define V4L2_CID_FRAME_RATE         (V4L2_CID_USER_SUNXI_CAMERA_BASE + 5)
#define V4L2_CID_TAKE_PICTURE       (V4L2_CID_USER_SUNXI_CAMERA_BASE + 6)
#define V4L2_CID_HOR_VISUAL_ANGLE   (V4L2_CID_USER_SUNXI_CAMERA_BASE + 7)
#define V4L2_CID_VER_VISUAL_ANGLE   (V4L2_CID_USER_SUNXI_CAMERA_BASE + 8)
#define V4L2_CID_FOCUS_LENGTH       (V4L2_CID_USER_SUNXI_CAMERA_BASE + 9)
#define V4L2_CID_R_GAIN             (V4L2_CID_USER_SUNXI_CAMERA_BASE + 10)
#define V4L2_CID_GR_GAIN            (V4L2_CID_USER_SUNXI_CAMERA_BASE + 11)
#define V4L2_CID_GB_GAIN            (V4L2_CID_USER_SUNXI_CAMERA_BASE + 12)
#define V4L2_CID_B_GAIN             (V4L2_CID_USER_SUNXI_CAMERA_BASE + 13)
#define V4L2_CID_SENSOR_TYPE        (V4L2_CID_USER_SUNXI_CAMERA_BASE + 14)
#define V4L2_CID_AE_WIN_X1          (V4L2_CID_USER_SUNXI_CAMERA_BASE + 15)
#define V4L2_CID_AE_WIN_Y1          (V4L2_CID_USER_SUNXI_CAMERA_BASE + 16)
#define V4L2_CID_AE_WIN_X2          (V4L2_CID_USER_SUNXI_CAMERA_BASE + 17)
#define V4L2_CID_AE_WIN_Y2          (V4L2_CID_USER_SUNXI_CAMERA_BASE + 18)
#define V4L2_CID_AF_WIN_X1          (V4L2_CID_USER_SUNXI_CAMERA_BASE + 19)
#define V4L2_CID_AF_WIN_Y1          (V4L2_CID_USER_SUNXI_CAMERA_BASE + 20)
#define V4L2_CID_AF_WIN_X2          (V4L2_CID_USER_SUNXI_CAMERA_BASE + 21)
#define V4L2_CID_AF_WIN_Y2          (V4L2_CID_USER_SUNXI_CAMERA_BASE + 22)
#define V4L2_CID_FLASH_LED_MODE_V1  (V4L2_CID_USER_SUNXI_CAMERA_BASE + 23)

enum v4l2_sensor_type {
    V4L2_SENSOR_TYPE_YUV = 0,
    V4L2_SENSOR_TYPE_RAW = 1,
};

}


extern "C" {

typedef struct {
    int (*ispApiInit)();
    int (*ispGetIspId)(int video_id);
    int (*ispStart)(int isp_id);
    int (*ispStop)(int isp_id);
    int (*ispWaitToExit)(int isp_id);
    int (*ispApiUnInit)();
} AWIspApi;

typedef AWIspApi* (*PFN_CreateAWIspApi)();
typedef void (*PFN_DestroyAWIspApi)(AWIspApi* hdl);

}

static void* libawispapi = 0;

static PFN_CreateAWIspApi CreateAWIspApi = 0;
static PFN_DestroyAWIspApi DestroyAWIspApi = 0;

static int load_awispapi_library()
{
    if (libawispapi)
        return 0;

    // check device whitelist
    bool whitelisted = is_device_whitelisted();
    if (!whitelisted)
    {
        return -1;
    }

    libawispapi = dlopen("libAWIspApi.so", RTLD_LOCAL | RTLD_NOW);
    if (!libawispapi)
    {
        libawispapi = dlopen("/usr/lib/libAWIspApi.so", RTLD_LOCAL | RTLD_NOW);
    }
    if (!libawispapi)
    {
        return -1;
    }

    CreateAWIspApi = (PFN_CreateAWIspApi)dlsym(libawispapi, "CreateAWIspApi");
    DestroyAWIspApi = (PFN_DestroyAWIspApi)dlsym(libawispapi, "DestroyAWIspApi");

    if (!CreateAWIspApi) fprintf(stderr, "%s\n", dlerror());
    if (!DestroyAWIspApi) fprintf(stderr, "%s\n", dlerror());

    return 0;
}

static int unload_awispapi_library()
{
    if (!libawispapi)
        return 0;

    dlclose(libawispapi);
    libawispapi = 0;

    CreateAWIspApi = 0;
    DestroyAWIspApi = 0;

    return 0;
}

class awispapi_library_loader
{
public:
    bool ready;

    awispapi_library_loader()
    {
        ready = (load_awispapi_library() == 0);
    }

    ~awispapi_library_loader()
    {
        unload_awispapi_library();
    }
};

static awispapi_library_loader awispapi;


struct ion_memory
{
    size_t size;
    ion_user_handle_t handle;
    int fd;
    void* virt_addr;
    unsigned int phy_addr;
};

class ion_allocator
{
public:
    ion_allocator();
    ~ion_allocator();

    int open();
    void close();

    int alloc(size_t size, struct ion_memory* mem);
    int free(struct ion_memory* mem);

    int flush(struct ion_memory* mem, int offset, int size);

public:
    int ion_fd;
    int cedar_fd;
};

ion_allocator::ion_allocator()
{
    ion_fd = -1;
    cedar_fd = -1;
}

ion_allocator::~ion_allocator()
{
    close();
}

int ion_allocator::open()
{
    close();

    ion_fd = ::open("/dev/ion", O_RDWR);
    if (ion_fd < 0)
    {
        fprintf(stderr, "open /dev/ion failed\n");
        return -1;
    }

    cedar_fd = ::open("/dev/cedar_dev", O_RDONLY);
    if (cedar_fd < 0)
    {
        fprintf(stderr, "open /dev/cedar_dev failed\n");
        return -1;
    }

    if (ioctl(cedar_fd, IOCTL_ENGINE_REQ, 0))
    {
        fprintf(stderr, "ioctl IOCTL_ENGINE_REQ failed %d %s\n", errno, strerror(errno));
        return -1;
    }

    return 0;
}

void ion_allocator::close()
{
    if (cedar_fd != -1)
    {
        if (ioctl(cedar_fd, IOCTL_ENGINE_REL, 0))
        {
            fprintf(stderr, "ioctl IOCTL_ENGINE_REL failed %d %s\n", errno, strerror(errno));
        }

        ::close(cedar_fd);
        cedar_fd = -1;
    }

    if (ion_fd != -1)
    {
        ::close(ion_fd);
        ion_fd = -1;
    }
}

int ion_allocator::alloc(size_t size, struct ion_memory* mem)
{
    struct ion_allocation_data alloc_data;
    alloc_data.len = size;
    alloc_data.align = 0;//ION_ALLOC_ALIGN;
    alloc_data.heap_id_mask = ION_HEAP_SYSTEM_MASK;
    alloc_data.flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
    alloc_data.handle = 0;
    if (ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data))
    {
        fprintf(stderr, "ioctl ION_IOC_ALLOC failed %d %s\n", errno, strerror(errno));
        return -1;
    }

    struct ion_fd_data fd_data;
    fd_data.handle = alloc_data.handle;
    fd_data.fd = -1;
    if (ioctl(ion_fd, ION_IOC_MAP, &fd_data))
    {
        fprintf(stderr, "ioctl ION_IOC_MAP failed %d %s\n", errno, strerror(errno));
        return -1;
    }

    void* virt_addr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd_data.fd, 0);
    if (!virt_addr)
    {
        fprintf(stderr, "mmap failed %d %s\n", errno, strerror(errno));
        return -1;
    }

    struct user_iommu_param iommu_param;
    iommu_param.fd = fd_data.fd;
    iommu_param.iommu_addr = 0;
    if (ioctl(cedar_fd, IOCTL_GET_IOMMU_ADDR, &iommu_param))
    {
        fprintf(stderr, "ioctl IOCTL_GET_IOMMU_ADDR failed %d %s\n", errno, strerror(errno));
        return -1;
    }

    mem->size = size;
    mem->handle = alloc_data.handle;
    mem->fd = fd_data.fd;
    mem->virt_addr = virt_addr;
    mem->phy_addr = iommu_param.iommu_addr;

    // fprintf(stderr, "alloc %u  at %u\n", mem->size, mem->phy_addr);

    return 0;
}

int ion_allocator::free(struct ion_memory* mem)
{
    if (!mem->handle)
        return 0;

    // fprintf(stderr, "free  %u  at %u\n", mem->size, mem->phy_addr);

    int ret = 0;

    struct user_iommu_param iommu_param;
    iommu_param.fd = mem->fd;
    if (ioctl(cedar_fd, IOCTL_FREE_IOMMU_ADDR, &iommu_param))
    {
        fprintf(stderr, "ioctl IOCTL_FREE_IOMMU_ADDR failed %d %s\n", errno, strerror(errno));
        ret = -1;
    }

    munmap(mem->virt_addr, mem->size);

    // ::close(mem->fd);

    struct ion_handle_data handle_data;
    handle_data.handle = mem->handle;
    if (ioctl(ion_fd, ION_IOC_FREE, &handle_data))
    {
        fprintf(stderr, "ioctl ION_IOC_FREE failed %d %s\n", errno, strerror(errno));
        ret = -1;
    }

    mem->size = 0;
    mem->handle = 0;
    mem->fd = -1;
    mem->virt_addr = 0;
    mem->phy_addr = 0;

    return ret;
}

int ion_allocator::flush(struct ion_memory* mem, int offset, int size)
{
    struct sunxi_cache_range range;
    range.start = (long)mem->virt_addr + offset;
    range.end = range.start + size;
    if (ioctl(ion_fd, ION_IOC_SUNXI_FLUSH_RANGE, (void*)&range))
    {
        fprintf(stderr, "ioctl ION_IOC_SUNXI_FLUSH_RANGE failed %d %s\n", errno, strerror(errno));
        return -1;
    }

    return 0;
}


class capture_v4l2_aw_isp_impl
{
public:
    capture_v4l2_aw_isp_impl();
    ~capture_v4l2_aw_isp_impl();

    int open(int width, int height, float fps);

    int start_streaming();

    int read_frame(unsigned char* bgrdata);

    int stop_streaming();

    int close();

public:
    AWIspApi* awisp;
    int ispid;

    ion_allocator ion;

    int g2d_fd;

    char devpath[32];
    int fd;
    v4l2_buf_type buf_type;

    __u32 cap_pixelformat;
    __u32 cap_width;
    __u32 cap_height;
    __u32 cap_numerator;
    __u32 cap_denominator;

    int crop_width;
    int crop_height;

    int output_width;
    int output_height;

    void* data[3];
    __u32 data_length[3];
    unsigned int data_phy_addr[3];

    struct ion_memory bgr_ion;
};

capture_v4l2_aw_isp_impl::capture_v4l2_aw_isp_impl()
{
    awisp = 0;
    ispid = -1;

    g2d_fd = -1;

    fd = -1;
    buf_type = (v4l2_buf_type)0;

    cap_pixelformat = 0;
    cap_width = 0;
    cap_height = 0;
    cap_numerator = 0;
    cap_denominator = 0;

    crop_width = 0;
    crop_height = 0;

    output_width = 0;
    output_height = 0;

    for (int i = 0; i < 3; i++)
    {
        data[i] = 0;
        data_length[i] = 0;
        data_phy_addr[i] = 0;
    }

}

capture_v4l2_aw_isp_impl::~capture_v4l2_aw_isp_impl()
{
    close();
}

static inline size_t least_common_multiple(size_t a, size_t b)
{
    if (a == b)
        return a;

    if (a > b)
        return least_common_multiple(b, a);

    size_t lcm = b;
    while (lcm % a != 0)
    {
        lcm += b;
    }

    return lcm;
}

int capture_v4l2_aw_isp_impl::open(int width, int height, float fps)
{
    if (!awispapi.ready)
    {
        fprintf(stderr, "awispapi not ready\n");
        return -1;
    }

    int vin_index = -1;

    // enumerate /sys/class/video4linux/videoN/name and find vin_video0
    for (int i = 0; i < 64; i++)
    {
        char path[256];
        sprintf(path, "/sys/class/video4linux/video%d/name", i);

        FILE* fp = fopen(path, "rb");
        if (!fp)
            continue;

        char line[32];
        fgets(line, 32, fp);

        fclose(fp);

        if (strncmp(line, "vin_video0", 10) == 0)
        {
            vin_index = i;
            break;
        }
    }

    if (vin_index == -1)
    {
        fprintf(stderr, "cannot find v4l device with name vin_video0\n");
        return -1;
    }

    sprintf(devpath, "/dev/video%d", vin_index);

    if (ion.open() != 0)
    {
        fprintf(stderr, "ion_allocator open failed\n");
        goto OUT;
    }

    g2d_fd = ::open("/dev/g2d", O_RDWR);
    if (g2d_fd < 0)
    {
        fprintf(stderr, "open /dev/g2d failed %d %s\n", errno, strerror(errno));
        goto OUT;
    }

    fd = ::open(devpath, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
    {
        fprintf(stderr, "open %s failed %d %s\n", devpath, errno, strerror(errno));
        goto OUT;
    }

    // query cap
    {
        struct v4l2_capability caps;
        memset(&caps, 0, sizeof(caps));

        if (ioctl(fd, VIDIOC_QUERYCAP, &caps))
        {
            fprintf(stderr, "%s ioctl VIDIOC_QUERYCAP failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        fprintf(stderr, "   devpath = %s\n", devpath);
        fprintf(stderr, "   driver = %s\n", caps.driver);
        fprintf(stderr, "   card = %s\n", caps.card);
        fprintf(stderr, "   bus_info = %s\n", caps.bus_info);
        fprintf(stderr, "   version = %x\n", caps.version);
        fprintf(stderr, "   capabilities = %x\n", caps.capabilities);
        fprintf(stderr, "   device_caps = %x\n", caps.device_caps);

        if (caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        {
            buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        }
        else if (caps.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        {
            buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        }
        else
        {
            fprintf(stderr, "%s is not V4L2_CAP_VIDEO_CAPTURE or V4L2_CAP_VIDEO_CAPTURE_MPLANE\n", devpath);
            goto OUT;
        }

        if (!(caps.capabilities & V4L2_CAP_STREAMING))
        {
            fprintf(stderr, "%s is not V4L2_CAP_STREAMING\n", devpath);
            goto OUT;
        }
    }


    // select the current video input
    {
        struct v4l2_input inp;
        inp.index = 0;
        inp.type = V4L2_INPUT_TYPE_CAMERA;
        if (ioctl(fd, VIDIOC_S_INPUT, &inp))
        {
            fprintf(stderr, "%s ioctl VIDIOC_S_INPUT failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }
    }

    fprintf(stderr, "V4L2_CID_SENSOR_TYPE = %x\n", V4L2_CID_SENSOR_TYPE);
    fprintf(stderr, "V4L2_SENSOR_TYPE_RAW = %x\n", V4L2_SENSOR_TYPE_RAW);

    // vin isp
    {
        struct v4l2_queryctrl qc_ctrl;
        memset(&qc_ctrl, 0, sizeof(struct v4l2_queryctrl));
        qc_ctrl.id = V4L2_CID_SENSOR_TYPE;

        if (ioctl(fd, VIDIOC_QUERYCTRL, &qc_ctrl))
        {
            fprintf(stderr, "%s ioctl VIDIOC_QUERYCTRL failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }
    }

    {
        struct v4l2_control ctrl;
        memset(&ctrl, 0, sizeof(struct v4l2_control));
        ctrl.id = V4L2_CID_SENSOR_TYPE;

        if (ioctl(fd, VIDIOC_G_CTRL, &ctrl))
        {
            fprintf(stderr, "%s ioctl VIDIOC_G_CTRL failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        if (ctrl.value == V4L2_SENSOR_TYPE_RAW)
        {
            fprintf(stderr, "raw sensor use vin isp\n");
            awisp = CreateAWIspApi();
            if (!awisp)
            {
                fprintf(stderr, "CreateAWIspApi failed\n");
                goto OUT;
            }
        }
    }


    // isp init
    {
        int ret = awisp->ispApiInit();
        if (ret != 0)
        {
            fprintf(stderr, "ispApiInit failed\n");
            goto OUT;
        }
    }

    {
        ispid = awisp->ispGetIspId(vin_index);

        fprintf(stderr, "ispid = %d\n", ispid);
    }

    // enumerate format
    for (int i = 0; ; i++)
    {
        struct v4l2_fmtdesc fmtdesc;
        memset(&fmtdesc, 0, sizeof(fmtdesc));
        fmtdesc.index = i;
        fmtdesc.type = buf_type;
        if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc))
        {
            if (errno == EINVAL)
                break;

            fprintf(stderr, "%s ioctl VIDIOC_ENUM_FMT failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        fprintf(stderr, "   fmt = %s  %x\n", fmtdesc.description, fmtdesc.pixelformat);

        if (fmtdesc.flags & V4L2_FMT_FLAG_COMPRESSED)
        {
            continue;
        }

        if (fmtdesc.pixelformat != V4L2_PIX_FMT_NV21)
        {
            // we could only handle nv21 atm
            continue;
        }

        if (cap_pixelformat == 0)
        {
            cap_pixelformat = fmtdesc.pixelformat;
        }

        // enumerate size
        for (int j = 0; ; j++)
        {
            struct v4l2_frmsizeenum frmsizeenum;
            memset(&frmsizeenum, 0, sizeof(frmsizeenum));
            frmsizeenum.index = j;
            frmsizeenum.pixel_format = fmtdesc.pixelformat;
            if (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum))
            {
                if (errno == EINVAL)
                    break;

                fprintf(stderr, "%s ioctl VIDIOC_ENUM_FRAMESIZES failed %d %s\n", devpath, errno, strerror(errno));
                goto OUT;
            }

            // NOTE
            // cap_width must be a multiple of 16
            // cap_height must be a multiple of 2
            if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                __u32 w = frmsizeenum.discrete.width;
                __u32 h = frmsizeenum.discrete.height;
                fprintf(stderr, "       size = %d x %d\n", w, h);

                if (cap_width == 0 || cap_height == 0)
                {
                    cap_width = w;
                    cap_height = h;
                }
            }
            if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
            {
                __u32 minw = frmsizeenum.stepwise.min_width;
                __u32 maxw = frmsizeenum.stepwise.max_width;
                __u32 minh = frmsizeenum.stepwise.min_height;
                __u32 maxh = frmsizeenum.stepwise.max_height;
                fprintf(stderr, "       size = %d x %d  ~  %d x %d\n", minw, minh, maxw, maxh);

                if (cap_width == 0 || cap_height == 0)
                {
                    if (width / (float)height > maxw / (float)maxh)
                    {
                        // fatter
                        cap_height = (width * maxh / maxw + 1) / 2 * 2;
                        cap_width = (width + 15) / 16 * 16;
                    }
                    else
                    {
                        // thinner
                        cap_width = (height * maxw / maxh + 15) / 16 * 16;
                        cap_height = (height + 1) / 2 * 2;
                    }

                    if (cap_width < minw || cap_height < minh)
                    {
                        cap_width = minw;
                        cap_height = minh;
                    }
                }
            }
            if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_STEPWISE)
            {
                __u32 minw = frmsizeenum.stepwise.min_width;
                __u32 maxw = frmsizeenum.stepwise.max_width;
                __u32 sw = frmsizeenum.stepwise.step_width;
                __u32 minh = frmsizeenum.stepwise.min_height;
                __u32 maxh = frmsizeenum.stepwise.max_height;
                __u32 sh = frmsizeenum.stepwise.step_height;
                fprintf(stderr, "       size = %d x %d  ~  %d x %d  (+%d +%d)\n", minw, minh, maxw, maxh, sw, sh);

                sw = least_common_multiple(sw, 16);
                sh = least_common_multiple(sh, 2);

                if (cap_width == 0 || cap_height == 0)
                {
                    if (width / (float)height > maxw / (float)maxh)
                    {
                        // fatter
                        cap_height = (width * maxh / maxw + sh - 1) / sh * sh;
                        cap_width = (width + sw - 1) / sw * sw;
                    }
                    else
                    {
                        // thinner
                        cap_width = (height * maxw / maxh + sw - 1) / sw * sw;
                        cap_height = (height + sh - 1) / sh * sh;
                    }

                    if (cap_width < minw || cap_height < minh)
                    {
                        cap_width = minw;
                        cap_height = minh;
                    }
                }
            }

            // FIXME hardcode fps for gc2053
            cap_numerator = 1;
            cap_denominator = 20;

            if (frmsizeenum.type != V4L2_FRMSIZE_TYPE_DISCRETE)
            {
                break;
            }
        }
    }

    // {
    //     const char* pp = (const char*)&cap_pixelformat;
    //     fprintf(stderr, "cap_pixelformat = %x  %c%c%c%c\n", cap_pixelformat, pp[0], pp[1], pp[2], pp[3]);
    //     fprintf(stderr, "cap_width = %d\n", cap_width);
    //     fprintf(stderr, "cap_height = %d\n", cap_height);
    //     fprintf(stderr, "cap_numerator = %d\n", cap_numerator);
    //     fprintf(stderr, "cap_denominator = %d\n", cap_denominator);
    // }

    if (cap_pixelformat == 0 || cap_width == 0 || cap_height == 0)
    {
        fprintf(stderr, "%s no supported pixel format or size\n", devpath);
        goto OUT;
    }

    // control format and size
    {
        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            fmt.type = buf_type;
            fmt.fmt.pix_mp.width = cap_width;
            fmt.fmt.pix_mp.height = cap_height;
            fmt.fmt.pix_mp.pixelformat = cap_pixelformat;
            fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
        }
        else
        {
            fmt.type = buf_type;
            fmt.fmt.pix.width = cap_width;
            fmt.fmt.pix.height = cap_height;
            fmt.fmt.pix.pixelformat = cap_pixelformat;
            fmt.fmt.pix.field = V4L2_FIELD_NONE;
        }

        if (ioctl(fd, VIDIOC_S_FMT, &fmt))
        {
            fprintf(stderr, "%s ioctl VIDIOC_S_FMT failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        if (ioctl(fd, VIDIOC_G_FMT, &fmt))
        {
            fprintf(stderr, "%s ioctl VIDIOC_G_FMT failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            cap_pixelformat = fmt.fmt.pix_mp.pixelformat;
            cap_width = fmt.fmt.pix_mp.width;
            cap_height = fmt.fmt.pix_mp.height;
        }
        else
        {
            cap_pixelformat = fmt.fmt.pix.pixelformat;
            cap_width = fmt.fmt.pix.width;
            cap_height = fmt.fmt.pix.height;
        }

        const char* pp = (const char*)&cap_pixelformat;
        fprintf(stderr, "cap_pixelformat = %x  %c%c%c%c\n", cap_pixelformat, pp[0], pp[1], pp[2], pp[3]);
        fprintf(stderr, "cap_width = %d\n", cap_width);
        fprintf(stderr, "cap_height = %d\n", cap_height);
        fprintf(stderr, "bytesperline: %d\n", fmt.fmt.pix.bytesperline);
    }

    // resolve output size
    {
        if (width > (int)cap_width && height > (int)cap_height)
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
        else if (width > (int)cap_width)
        {
            output_width = cap_width;
            output_height = height * cap_width / width;
        }
        else if (height > (int)cap_height)
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

    if (output_width < std::max(16, (int)cap_width / 16) || output_height < std::max(16, (int)cap_height / 16))
    {
        fprintf(stderr, "invalid size %d x %d -> %d x %d\n", width, height, output_width, output_height);
        goto OUT;
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

    // control fps
    {
        struct v4l2_streamparm streamparm;
        memset(&streamparm, 0, sizeof(streamparm));
        streamparm.type = buf_type;

        if (ioctl(fd, VIDIOC_G_PARM, &streamparm))
        {
            if (errno == ENOTTY)
            {
                // VIDIOC_G_PARM not implemented
            }
            else
            {
                fprintf(stderr, "%s ioctl VIDIOC_G_PARM failed %d %s\n", devpath, errno, strerror(errno));
                goto OUT;
            }
        }

        if (streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
        {
            streamparm.parm.capture.timeperframe.numerator = cap_numerator;
            streamparm.parm.capture.timeperframe.denominator = cap_denominator;
            if (ioctl(fd, VIDIOC_S_PARM, &streamparm))
            {
                fprintf(stderr, "%s ioctl VIDIOC_S_PARM failed %d %s\n", devpath, errno, strerror(errno));
                goto OUT;
            }

            cap_numerator = streamparm.parm.capture.timeperframe.numerator;
            cap_denominator = streamparm.parm.capture.timeperframe.denominator;
        }
        else
        {
            fprintf(stderr, "%s does not support changing fps\n", devpath);
        }

        fprintf(stderr, "cap_numerator = %d\n", cap_numerator);
        fprintf(stderr, "cap_denominator = %d\n", cap_denominator);
    }

    // mmap
    {
        struct v4l2_requestbuffers requestbuffers;
        memset(&requestbuffers, 0, sizeof(requestbuffers));
        requestbuffers.count = 3;// FIXME hardcode
        requestbuffers.type = buf_type;
        requestbuffers.memory = V4L2_MEMORY_MMAP;
        if (ioctl(fd, VIDIOC_REQBUFS, &requestbuffers))
        {
            fprintf(stderr, "%s ioctl VIDIOC_REQBUFS failed %d %s\n", devpath, errno, strerror(errno));
            goto OUT;
        }

        fprintf(stderr, "requestbuffers.count = %d\n", requestbuffers.count);

        for (int i = 0; i < 3/*requestbuffers.count*/; i++)
        {
            struct v4l2_plane planes[2];
            memset(&planes[0], 0, sizeof(planes[0]));
            memset(&planes[1], 0, sizeof(planes[1]));

            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = buf_type;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
            {
                buf.m.planes = planes;
                buf.length = 2;
            }

            if (ioctl(fd, VIDIOC_QUERYBUF, &buf))
            {
                fprintf(stderr, "%s ioctl VIDIOC_QUERYBUF failed %d %s\n", devpath, errno, strerror(errno));
                goto OUT;
            }

            // fprintf(stderr, "planes count = %d\n", buf.length);

            if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
            {
                data[i] = mmap(NULL, buf.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.planes[0].m.mem_offset);
                data_length[i] = buf.m.planes[0].length;
            }
            else
            {
                data[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
                data_length[i] = buf.length;
            }

            memset(data[i], 0, data_length[i]);
        }

        for (int i = 0; i < 3/*requestbuffers.count*/; i++)
        {
            struct v4l2_plane planes[2];
            memset(&planes[0], 0, sizeof(planes[0]));
            memset(&planes[1], 0, sizeof(planes[1]));

            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));

            buf.type = buf_type;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
            {
                buf.m.planes = planes;
                buf.length = 2;
            }

            if (ioctl(fd, VIDIOC_QBUF, &buf))
            {
                fprintf(stderr, "%s ioctl VIDIOC_QBUF failed\n", devpath);
                goto OUT;
            }
        }
    }

    // HACK
    ion.alloc(cap_width * cap_height * 3, &bgr_ion);

    // HACK
    {
        data_phy_addr[0] = bgr_ion.phy_addr - 3145728 * 3;
        data_phy_addr[1] = bgr_ion.phy_addr - 3145728 * 2;
        data_phy_addr[2] = bgr_ion.phy_addr - 3145728;
    }

    return 0;

OUT:

    close();
    return -1;
}

int capture_v4l2_aw_isp_impl::start_streaming()
{
    {
        int ret = awisp->ispStart(ispid);
        if (ret != 0)
        {
            fprintf(stderr, "ispStart failed\n");
            return -1;
        }
    }

    v4l2_buf_type type = buf_type;
    if (ioctl(fd, VIDIOC_STREAMON, &type))
    {
        fprintf(stderr, "%s ioctl VIDIOC_STREAMON failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    return 0;
}

int capture_v4l2_aw_isp_impl::read_frame(unsigned char* bgrdata)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int r = select(fd + 1, &fds, NULL, NULL, &tv);

    if (r == -1)
    {
        fprintf(stderr, "select %s failed\n", devpath);
        return -1;
    }

    if (r == 0)
    {
        fprintf(stderr, "select %s timeout\n", devpath);
        return -1;
    }

    struct v4l2_plane planes[2];
    memset(&planes[0], 0, sizeof(planes[0]));
    memset(&planes[1], 0, sizeof(planes[1]));

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = buf_type;
    buf.memory = V4L2_MEMORY_MMAP;

    if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
    {
        buf.m.planes = planes;
        buf.length = 2;
    }

    if (ioctl(fd, VIDIOC_DQBUF, &buf))
    {
        fprintf(stderr, "%s ioctl VIDIOC_DQBUF failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    // consume data
    {
        unsigned int yuv_phy_addr = data_phy_addr[buf.index];

        g2d_blt_h blit;
        memset(&blit, 0, sizeof(blit));

        blit.flag_h = G2D_BLT_NONE_H;

        // blit.src_image_h.bbuff = 1;
        blit.src_image_h.format = G2D_FORMAT_YUV420UVC_V1U1V0U0;
        blit.src_image_h.laddr[0] = yuv_phy_addr;
        blit.src_image_h.laddr[1] = yuv_phy_addr + cap_width * cap_height;

        blit.src_image_h.width = cap_width;
        blit.src_image_h.height = cap_height;
        blit.src_image_h.align[0] = 0;
        blit.src_image_h.align[1] = 0;
        blit.src_image_h.clip_rect.x = (cap_width - crop_width) / 2;
        blit.src_image_h.clip_rect.y = (cap_height - crop_height) / 2;
        blit.src_image_h.clip_rect.w = crop_width;
        blit.src_image_h.clip_rect.h = crop_height;
        blit.src_image_h.gamut = G2D_BT601;
        blit.src_image_h.bpremul = 0;
        // blit.src_image_h.alpha = 0xff;
        blit.src_image_h.mode = G2D_PIXEL_ALPHA;//G2D_GLOBAL_ALPHA;
        blit.src_image_h.use_phy_addr = 1;
        blit.src_image_h.fd = -1;
        // blit.src_image_h.use_phy_addr = 0;
        // blit.src_image_h.fd = yuv_ion.fd;

        // blit.dst_image_h.bbuff = 1;
        blit.dst_image_h.format = G2D_FORMAT_BGR888;
        // blit.dst_image_h.format = G2D_FORMAT_RGB565;
        // blit.dst_image_h.laddr[0] = rgb_phy_addr;

        blit.dst_image_h.width = output_width;
        blit.dst_image_h.height = output_height;
        blit.dst_image_h.align[0] = 0;
        blit.dst_image_h.clip_rect.x = 0;
        blit.dst_image_h.clip_rect.y = 0;
        blit.dst_image_h.clip_rect.w = output_width;
        blit.dst_image_h.clip_rect.h = output_height;
        blit.dst_image_h.gamut = G2D_BT601;
        blit.dst_image_h.bpremul = 0;
        // blit.dst_image_h.alpha = 0xff;
        blit.dst_image_h.mode = G2D_PIXEL_ALPHA;//G2D_GLOBAL_ALPHA;
        // blit.dst_image_h.use_phy_addr = 1;
        // blit.dst_image_h.fd = -1;
        blit.dst_image_h.use_phy_addr = 0;
        blit.dst_image_h.fd = bgr_ion.fd;

        if (ioctl(g2d_fd, G2D_CMD_BITBLT_H, &blit) < 0)
        {
            fprintf(stderr, "ioctl G2D_CMD_BITBLT_H failed %d %s\n", errno, strerror(errno));
        }

        ion.flush(&bgr_ion, 0, output_width * output_height * 3);

        // copy to bgrdata
        memcpy(bgrdata, (const unsigned char*)bgr_ion.virt_addr, output_width * output_height * 3);
    }

    // requeue buf
    // memset(&planes[0], 0, sizeof(planes[0]));
    // memset(&planes[1], 0, sizeof(planes[1]));

    if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
    {
        buf.m.planes = planes;
        buf.length = 2;
    }

    if (ioctl(fd, VIDIOC_QBUF, &buf))
    {
        fprintf(stderr, "%s ioctl VIDIOC_QBUF failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    return 0;
}

int capture_v4l2_aw_isp_impl::stop_streaming()
{
    v4l2_buf_type type = buf_type;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type))
    {
        fprintf(stderr, "%s ioctl VIDIOC_STREAMOFF failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    {
        int ret = awisp->ispStop(ispid);
        if (ret != 0)
        {
            fprintf(stderr, "ispStop failed\n");
            return -1;
        }
    }

    return 0;
}

int capture_v4l2_aw_isp_impl::close()
{
    if (awisp)
    {
        int ret = awisp->ispApiUnInit();
        if (ret != 0)
        {
            fprintf(stderr, "ispApiUnInit failed\n");
        }
    }

    ion.free(&bgr_ion);

    for (int i = 0; i < 3; i++)
    {
        if (data[i])
        {
            munmap(data[i], data_length[i]);
            data[i] = 0;
            data_length[i] = 0;
            data_phy_addr[i] = 0;
        }
    }

    if (awisp)
    {
        DestroyAWIspApi(awisp);
        awisp = 0;
    }

    ispid = -1;

    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }

    if (g2d_fd >= 0)
    {
        ::close(g2d_fd);
        g2d_fd = -1;
    }

    ion.close();

    buf_type = (v4l2_buf_type)0;

    cap_pixelformat = 0;
    cap_width = 0;
    cap_height = 0;
    cap_numerator = 0;
    cap_denominator = 0;

    crop_width = 0;
    crop_height = 0;

    output_width = 0;
    output_height = 0;

    return 0;
}

bool capture_v4l2_aw_isp::supported()
{
    if (!awispapi.ready)
        return false;

    return true;
}

capture_v4l2_aw_isp::capture_v4l2_aw_isp() : d(new capture_v4l2_aw_isp_impl)
{
}

capture_v4l2_aw_isp::~capture_v4l2_aw_isp()
{
    delete d;
}

int capture_v4l2_aw_isp::open(int width, int height, float fps)
{
    return d->open(width, height, fps);
}

int capture_v4l2_aw_isp::get_width() const
{
    return d->output_width;
}

int capture_v4l2_aw_isp::get_height() const
{
    return d->output_height;
}

float capture_v4l2_aw_isp::get_fps() const
{
    return d->cap_numerator ? d->cap_denominator / (float)d->cap_numerator : 0;
}

int capture_v4l2_aw_isp::start_streaming()
{
    return d->start_streaming();
}

int capture_v4l2_aw_isp::read_frame(unsigned char* bgrdata)
{
    return d->read_frame(bgrdata);
}

int capture_v4l2_aw_isp::stop_streaming()
{
    return d->stop_streaming();
}

int capture_v4l2_aw_isp::close()
{
    return d->close();
}

} // namespace cv

#else // defined __linux__

namespace cv {

bool capture_v4l2_aw_isp::supported()
{
    return false;
}

capture_v4l2_aw_isp::capture_v4l2_aw_isp() : d(0)
{
}

capture_v4l2_aw_isp::~capture_v4l2_aw_isp()
{
}

int capture_v4l2_aw_isp::open(int width, int height, float fps)
{
    return -1;
}

int capture_v4l2_aw_isp::get_width() const
{
    return -1;
}

int capture_v4l2_aw_isp::get_height() const
{
    return -1;
}

float capture_v4l2_aw_isp::get_fps() const
{
    return 0.f;
}

int capture_v4l2_aw_isp::start_streaming()
{
    return -1;
}

int capture_v4l2_aw_isp::read_frame(unsigned char* bgrdata)
{
    return -1;
}

int capture_v4l2_aw_isp::stop_streaming()
{
    return -1;
}

int capture_v4l2_aw_isp::close()
{
    return -1;
}

} // namespace cv

#endif // defined __linux__
