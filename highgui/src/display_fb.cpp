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

#include "display_fb.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#if __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON

#include "opencv2/imgproc.hpp"

namespace cv {

class display_fb_impl
{
public:
    display_fb_impl();
    ~display_fb_impl();

    int open();

    int show_image(const unsigned char* bgrdata, int width, int height);

    int close();

public:
    int fd;

    __u32 xres;
    __u32 yres;
    __u32 xres_virtual;
    __u32 yres_virtual;
    __u32 xoffset;
    __u32 yoffset;
    __u32 bits_per_pixel;
    __u32 grayscale;

    __u32 screen_size;
    void* fb_base;
};

display_fb_impl::display_fb_impl()
{
    fd = -1;

    screen_size = 0;
    fb_base = 0;
}

display_fb_impl::~display_fb_impl()
{
    close();
}

int display_fb_impl::open()
{
    fd = ::open("/dev/fb0", O_RDWR);
    if (fd < 0)
    {
        fprintf(stderr, "open /dev/fb0 failed\n");
        return -1;
    }

    struct fb_var_screeninfo screen_info;
    memset(&screen_info, 0, sizeof(screen_info));
    if (ioctl(fd, FBIOGET_VSCREENINFO, &screen_info))
    {
        fprintf(stderr, "ioctl FBIOGET_VSCREENINFO failed %d %s\n", errno, strerror(errno));
        goto OUT;
    }

    {
    xres = screen_info.xres;
    yres = screen_info.yres;
    xres_virtual = screen_info.xres_virtual;
    yres_virtual = screen_info.yres_virtual;
    xoffset = screen_info.xoffset;
    yoffset = screen_info.yoffset;
    bits_per_pixel = screen_info.bits_per_pixel;
    grayscale = screen_info.grayscale;

    fprintf(stderr, "xres = %u\n", xres);
    fprintf(stderr, "yres = %u\n", yres);
    fprintf(stderr, "xres_virtual = %u\n", xres_virtual);
    fprintf(stderr, "yres_virtual = %u\n", yres_virtual);
    fprintf(stderr, "xoffset = %u\n", xoffset);
    fprintf(stderr, "yoffset = %u\n", yoffset);
    fprintf(stderr, "bits_per_pixel = %u\n", bits_per_pixel);
    fprintf(stderr, "grayscale = %u\n", grayscale);

    screen_size = xres * yres * bits_per_pixel / 8;
    fb_base = mmap(NULL, screen_size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (!fb_base)
    {
        fprintf(stderr, "mmap failed %d %s\n", errno, strerror(errno));
        goto OUT;
    }

    }

    return 0;

OUT:

    close();
    return -1;
}

int display_fb_impl::show_image(const unsigned char* bgrdata, int width, int height)
{
    {
        // lseek(fd, 0, SEEK_SET);
        // write(fd, bgr.data, xres_virtual * 240 * 2);
    }

    // BGR888 to RGB565
#if 1
    for (int y = 0; y < height; y++)
    {
        const unsigned char* p = bgrdata + y * width * 3;
        unsigned short* pout = (unsigned short*)fb_base + y * width;// FIXME HARDCODE

        int x = 0;
#if __ARM_NEON
        for (; x + 15 < width; x += 16)
        {
            __builtin_prefetch(p + 48);
            uint8x16x3_t vsrc = vld3q_u8(p);

            uint16x8_t vdst0 = vshll_n_u8(vget_low_u8(vsrc.val[2]), 8);
            uint16x8_t vdst1 = vshll_n_u8(vget_high_u8(vsrc.val[2]), 8);
            vdst0 = vsriq_n_u16(vdst0, vshll_n_u8(vget_low_u8(vsrc.val[1]), 8), 5);
            vdst1 = vsriq_n_u16(vdst1, vshll_n_u8(vget_high_u8(vsrc.val[1]), 8), 5);
            vdst0 = vsriq_n_u16(vdst0, vshll_n_u8(vget_low_u8(vsrc.val[0]), 8), 11);
            vdst1 = vsriq_n_u16(vdst1, vshll_n_u8(vget_high_u8(vsrc.val[0]), 8), 11);

            vst1q_u16(pout, vdst0);
            vst1q_u16(pout + 8, vdst1);

            p += 48;
            pout += 16;
        }
        for (; x + 7 < width; x += 8)
        {
            uint8x8x3_t vsrc = vld3_u8(p);

            uint16x8_t vdst = vshll_n_u8(vsrc.val[2], 8);
            vdst = vsriq_n_u16(vdst, vshll_n_u8(vsrc.val[1], 8), 5);
            vdst = vsriq_n_u16(vdst, vshll_n_u8(vsrc.val[0], 8), 11);

            vst1q_u16(pout, vdst);

            p += 24;
            pout += 8;
        }
#endif // __ARM_NEON
        for (; x < width; x++)
        {
            uchar b = p[0];
            uchar g = p[1];
            uchar r = p[2];

            pout[0] = (r >> 3 << 11) | (g >> 2 << 5) | (b >> 3);

            p += 3;
            pout += 1;
        }

        // pout += xres_virtual - 240;
    }
#endif

    // fsync(fb_fd);

    return 0;
}

int display_fb_impl::close()
{
    if (fb_base)
    {
        munmap(fb_base, screen_size);
        screen_size = 0;
        fb_base = 0;
    }

    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }

    return 0;
}

bool display_fb::supported()
{
    return true;
}

display_fb::display_fb() : d(new display_fb_impl)
{
}

display_fb::~display_fb()
{
    delete d;
}

int display_fb::open()
{
    return d->open();
}

int display_fb::get_width() const
{
    return d->xres;
}

int display_fb::get_height() const
{
    return d->yres;
}

int display_fb::show_image(const unsigned char* bgrdata, int width, int height)
{
    return d->show_image(bgrdata, width, height);
}

int display_fb::close()
{
    return d->close();
}

} // namespace cv
