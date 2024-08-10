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

    int show_bgr(const unsigned char* bgrdata, int width, int height);
    int show_gray(const unsigned char* graydata, int width, int height);

    int close();

public:
    int ttyfd;
    long tty_oldmode;

    int fd;

    struct fb_var_screeninfo info;

    // 0=rgb32
    // 1=bgr32
    // 2=rgb24
    // 3=bgr24
    // 4=rgb565
    // 5=bgr565
    // 6=mono
    int pixel_format;

    __u32 screen_size;
    void* fb_base;
};

display_fb_impl::display_fb_impl()
{
    ttyfd = -1;
    tty_oldmode = 0x00/*KD_TEXT*/;
    fd = -1;

    memset(&info, 0, sizeof(info));

    pixel_format = -1;

    screen_size = 0;
    fb_base = 0;
}

display_fb_impl::~display_fb_impl()
{
    close();
}

int display_fb_impl::open()
{
    if (fb_base)
        return 0;

    ttyfd = ::open("/dev/tty0", O_RDWR);
    if (ttyfd < 0)
    {
        ttyfd = ::open("/dev/tty", O_RDWR);
        if (ttyfd < 0)
        {
            ttyfd = ::open("/dev/console", O_RDWR);
            if (ttyfd < 0)
            {
                fprintf(stderr, "open /dev/tty0 failed\n");
                goto OUT;
            }
        }
    }

    if (ioctl(ttyfd, 0x4b3b/*KDGETMODE*/, &tty_oldmode))
    {
        fprintf(stderr, "ioctl KDGETMODE failed %d %s\n", errno, strerror(errno));
    }

    if (ioctl(ttyfd, 0x4b3a/*KDSETMODE*/, 0x01/*KD_GRAPHICS*/))
    {
        fprintf(stderr, "ioctl KDSETMODE KD_GRAPHICS failed %d %s\n", errno, strerror(errno));
    }

    // disable tty blinking cursor
    {
        const char buf[] = "\033[9;0]\033[?33l\033[?25l\033[?1c";
        ::write(ttyfd, buf, sizeof(buf));
    }

    fd = ::open("/dev/fb0", O_RDWR);
    if (fd < 0)
    {
        fprintf(stderr, "open /dev/fb0 failed\n");
        goto OUT;
    }

    memset(&info, 0, sizeof(info));
    if (ioctl(fd, FBIOGET_VSCREENINFO, &info))
    {
        fprintf(stderr, "ioctl FBIOGET_VSCREENINFO failed %d %s\n", errno, strerror(errno));
        goto OUT;
    }

    // resolve pixel_format
    {
        const fb_bitfield rgba[3] = { info.red, info.green, info.blue };
        if (info.bits_per_pixel == 32)
        {
            const fb_bitfield rgb32[3] = {{0, 8, 0}, {8, 8, 0}, {16, 8, 0}};
            if (memcmp(rgba, rgb32, 3 * sizeof(fb_bitfield)) == 0)
            {
                pixel_format = 0;
            }
            const fb_bitfield bgr32[3] = {{16, 8, 0}, {8, 8, 0}, {0, 8, 0}};
            if (memcmp(rgba, bgr32, 3 * sizeof(fb_bitfield)) == 0)
            {
                pixel_format = 1;
            }
        }
        if (info.bits_per_pixel == 24)
        {
            const fb_bitfield rgb24[3] = {{16, 8, 0}, {8, 8, 0}, {0, 8, 0}};
            if (memcmp(rgba, rgb24, 3 * sizeof(fb_bitfield)) == 0)
            {
                pixel_format = 2;
            }
            const fb_bitfield bgr24[3] = {{0, 8, 0}, {8, 8, 0}, {16, 8, 0}};
            if (memcmp(rgba, bgr24, 3 * sizeof(fb_bitfield)) == 0)
            {
                pixel_format = 3;
            }
        }
        if (info.bits_per_pixel == 16)
        {
            const fb_bitfield rgb565[3] = {{0, 5, 0}, {5, 6, 0}, {11, 5, 0}};
            if (memcmp(rgba, rgb565, 3 * sizeof(fb_bitfield)) == 0)
            {
                pixel_format = 4;
            }
            const fb_bitfield bgr565[3] = {{11, 5, 0}, {5, 6, 0}, {0, 5, 0}};
            if (memcmp(rgba, bgr565, 3 * sizeof(fb_bitfield)) == 0)
            {
                pixel_format = 5;
            }
        }
        if (info.bits_per_pixel == 8)
        {
            pixel_format = 6;
        }
    }

    if (pixel_format == -1)
    {
        fprintf(stderr, "FBIOGET_VSCREENINFO bits_per_pixel %d not supported\n", info.bits_per_pixel);
        goto OUT;
    }

    fprintf(stderr, "pixel_format = %d\n", pixel_format);

    {
    fprintf(stderr, "xres = %u\n", info.xres);
    fprintf(stderr, "yres = %u\n", info.yres);
    fprintf(stderr, "xres_virtual = %u\n", info.xres_virtual);
    fprintf(stderr, "yres_virtual = %u\n", info.yres_virtual);
    fprintf(stderr, "xoffset = %u\n", info.xoffset);
    fprintf(stderr, "yoffset = %u\n", info.yoffset);
    fprintf(stderr, "bits_per_pixel = %u\n", info.bits_per_pixel);
    fprintf(stderr, "grayscale = %u\n", info.grayscale);

    screen_size = info.xres * info.yres * info.bits_per_pixel / 8;
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

int display_fb_impl::show_bgr(const unsigned char* bgrdata, int width, int height)
{
    if (pixel_format == 0)
    {
        // BGR888 to RGBA8888
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = bgrdata + y * width * 3;
            unsigned char* pout = (unsigned char*)fb_base + y * width * 4;// FIXME HARDCODE

            int x = 0;
#if __ARM_NEON
            uint8x16_t _v255 = vdupq_n_u8(255);
            for (; x + 15 < width; x += 16)
            {
                __builtin_prefetch(p + 48);
                uint8x16x3_t _bgr = vld3q_u8(p);
                uint8x16x4_t _rgba;
                _rgba.val[0] = _bgr.val[2];
                _rgba.val[1] = _bgr.val[1];
                _rgba.val[2] = _bgr.val[0];
                _rgba.val[3] = _v255;
                vst4q_u8(pout, _rgba);
                p += 48;
                pout += 64;
            }
            for (; x + 7 < width; x += 8)
            {
                uint8x8x3_t _bgr = vld3_u8(p);
                uint8x8x4_t _rgba;
                _rgba.val[0] = _bgr.val[2];
                _rgba.val[1] = _bgr.val[1];
                _rgba.val[2] = _bgr.val[0];
                _rgba.val[3] = vget_low_u8(_v255);
                vst4_u8(pout, _rgba);
                p += 24;
                pout += 32;
            }
#endif // __ARM_NEON
            for (; x < width; x++)
            {
                pout[0] = p[2];
                pout[1] = p[1];
                pout[2] = p[0];
                pout[3] = 255;
                p += 3;
                pout += 4;
            }

            // pout += xres_virtual - 240;
        }
    }
    if (pixel_format == 1)
    {
        // BGR888 to BGRA8888
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = bgrdata + y * width * 3;
            unsigned char* pout = (unsigned char*)fb_base + y * width * 4;// FIXME HARDCODE

            int x = 0;
#if __ARM_NEON
            uint8x16_t _v255 = vdupq_n_u8(255);
            for (; x + 15 < width; x += 16)
            {
                __builtin_prefetch(p + 48);
                uint8x16x3_t _bgr = vld3q_u8(p);
                uint8x16x4_t _bgra;
                _bgra.val[0] = _bgr.val[0];
                _bgra.val[1] = _bgr.val[1];
                _bgra.val[2] = _bgr.val[2];
                _bgra.val[3] = _v255;
                vst4q_u8(pout, _bgra);
                p += 48;
                pout += 64;
            }
            for (; x + 7 < width; x += 8)
            {
                uint8x8x3_t _bgr = vld3_u8(p);
                uint8x8x4_t _bgra;
                _bgra.val[0] = _bgr.val[0];
                _bgra.val[1] = _bgr.val[1];
                _bgra.val[2] = _bgr.val[2];
                _bgra.val[3] = vget_low_u8(_v255);
                vst4_u8(pout, _bgra);
                p += 24;
                pout += 32;
            }
#endif // __ARM_NEON
            for (; x < width; x++)
            {
                pout[0] = p[0];
                pout[1] = p[1];
                pout[2] = p[2];
                pout[3] = 255;
                p += 3;
                pout += 4;
            }

            // pout += xres_virtual - 240;
        }
    }
    if (pixel_format == 2)
    {
        // BGR888 to RGB888
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = bgrdata + y * width * 3;
            unsigned char* pout = (unsigned char*)fb_base + y * width * 3;// FIXME HARDCODE

            int x = 0;
#if __ARM_NEON
            for (; x + 15 < width; x += 16)
            {
                __builtin_prefetch(p + 48);
                uint8x16x3_t _bgr = vld3q_u8(p);
                uint8x16x3_t _rgb;
                _rgb.val[0] = _bgr.val[2];
                _rgb.val[1] = _bgr.val[1];
                _rgb.val[2] = _bgr.val[0];
                vst3q_u8(pout, _rgb);
                p += 48;
                pout += 48;
            }
            for (; x + 7 < width; x += 8)
            {
                uint8x8x3_t _bgr = vld3_u8(p);
                uint8x8x3_t _rgb;
                _rgb.val[0] = _bgr.val[2];
                _rgb.val[1] = _bgr.val[1];
                _rgb.val[2] = _bgr.val[0];
                vst3_u8(pout, _rgb);
                p += 24;
                pout += 24;
            }
#endif // __ARM_NEON
            for (; x < width; x++)
            {
                pout[0] = p[2];
                pout[1] = p[1];
                pout[2] = p[0];
                p += 3;
                pout += 3;
            }

            // pout += xres_virtual - 240;
        }
    }
    if (pixel_format == 3)
    {
        // BGR888 to BGR888
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = bgrdata + y * width * 3;
            unsigned char* pout = (unsigned char*)fb_base + y * width * 3;// FIXME HARDCODE

            memcpy(pout, p, width * 3);

            // pout += xres_virtual - 240;
        }
    }
    if (pixel_format == 4)
    {
        // BGR888 to RGB565
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = bgrdata + y * width * 3;
            unsigned short* pout = (unsigned short*)fb_base + y * width;// FIXME HARDCODE

            int x = 0;
#if __ARM_NEON
            for (; x + 15 < width; x += 16)
            {
                __builtin_prefetch(p + 48);
                uint8x16x3_t _bgr = vld3q_u8(p);
                uint16x8_t _p0 = vshll_n_u8(vget_low_u8(_bgr.val[2]), 8);
                uint16x8_t _p1 = vshll_n_u8(vget_high_u8(_bgr.val[2]), 8);
                _p0 = vsriq_n_u16(_p0, vshll_n_u8(vget_low_u8(_bgr.val[1]), 8), 5);
                _p1 = vsriq_n_u16(_p1, vshll_n_u8(vget_high_u8(_bgr.val[1]), 8), 5);
                _p0 = vsriq_n_u16(_p0, vshll_n_u8(vget_low_u8(_bgr.val[0]), 8), 11);
                _p1 = vsriq_n_u16(_p1, vshll_n_u8(vget_high_u8(_bgr.val[0]), 8), 11);
                vst1q_u16(pout, _p0);
                vst1q_u16(pout + 8, _p1);
                p += 48;
                pout += 16;
            }
            for (; x + 7 < width; x += 8)
            {
                uint8x8x3_t _bgr = vld3_u8(p);
                uint16x8_t _p = vshll_n_u8(_bgr.val[2], 8);
                _p = vsriq_n_u16(_p, vshll_n_u8(_bgr.val[1], 8), 5);
                _p = vsriq_n_u16(_p, vshll_n_u8(_bgr.val[0], 8), 11);
                vst1q_u16(pout, _p);
                p += 24;
                pout += 8;
            }
#endif // __ARM_NEON
            for (; x < width; x++)
            {
                pout[0] = (p[2] >> 3 << 11) | (p[1] >> 2 << 5) | (p[0] >> 3);
                p += 3;
                pout += 1;
            }

            // pout += xres_virtual - 240;
        }
    }
    if (pixel_format == 5)
    {
        // BGR888 to BGR565
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = bgrdata + y * width * 3;
            unsigned short* pout = (unsigned short*)fb_base + y * width;// FIXME HARDCODE

            int x = 0;
#if __ARM_NEON
            for (; x + 15 < width; x += 16)
            {
                __builtin_prefetch(p + 48);
                uint8x16x3_t _bgr = vld3q_u8(p);
                uint16x8_t _p0 = vshll_n_u8(vget_low_u8(_bgr.val[0]), 8);
                uint16x8_t _p1 = vshll_n_u8(vget_high_u8(_bgr.val[0]), 8);
                _p0 = vsriq_n_u16(_p0, vshll_n_u8(vget_low_u8(_bgr.val[1]), 8), 5);
                _p1 = vsriq_n_u16(_p1, vshll_n_u8(vget_high_u8(_bgr.val[1]), 8), 5);
                _p0 = vsriq_n_u16(_p0, vshll_n_u8(vget_low_u8(_bgr.val[2]), 8), 11);
                _p1 = vsriq_n_u16(_p1, vshll_n_u8(vget_high_u8(_bgr.val[2]), 8), 11);
                vst1q_u16(pout, _p0);
                vst1q_u16(pout + 8, _p1);
                p += 48;
                pout += 16;
            }
            for (; x + 7 < width; x += 8)
            {
                uint8x8x3_t _bgr = vld3_u8(p);
                uint16x8_t _p = vshll_n_u8(_bgr.val[0], 8);
                _p = vsriq_n_u16(_p, vshll_n_u8(_bgr.val[1], 8), 5);
                _p = vsriq_n_u16(_p, vshll_n_u8(_bgr.val[2], 8), 11);
                vst1q_u16(pout, _p);
                p += 24;
                pout += 8;
            }
#endif // __ARM_NEON
            for (; x < width; x++)
            {
                pout[0] = (p[0] >> 3 << 11) | (p[1] >> 2 << 5) | (p[2] >> 3);
                p += 3;
                pout += 1;
            }

            // pout += xres_virtual - 240;
        }
    }
    if (pixel_format == 6)
    {
        // BGR888 to GRAY
        // coeffs for r g b = 0.299f, 0.587f, 0.114f
        const unsigned char Y_shift = 8; //14
        const unsigned char R2Y = 77;
        const unsigned char G2Y = 150;
        const unsigned char B2Y = 29;
#if __ARM_NEON
        uint8x8_t _R2Y = vdup_n_u8(R2Y);
        uint8x8_t _G2Y = vdup_n_u8(G2Y);
        uint8x8_t _B2Y = vdup_n_u8(B2Y);
#endif // __ARM_NEON
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = bgrdata + y * width * 3;
            unsigned char* pout = (unsigned char*)fb_base + y * width;// FIXME HARDCODE

            int x = 0;
#if __ARM_NEON
            for (; x + 7 < width; x += 8)
            {
                uint8x8x3_t _bgr = vld3_u8(p);
                uint16x8_t _y16 = vmull_u8(_bgr.val[2], _R2Y);
                _y16 = vmlal_u8(_y16, _bgr.val[1], _G2Y);
                _y16 = vmlal_u8(_y16, _bgr.val[0], _B2Y);
                uint8x8_t _y = vqrshrn_n_u16(_y16, Y_shift);
                vst1_u8(pout, _y);
                p += 24;
                pout += 8;
            }
#endif // __ARM_NEON
            for (; x < width; x++)
            {
                pout[0] = std::min((p[2] * R2Y + p[1] * G2Y + p[0] * B2Y) >> Y_shift, 255);
                p += 3;
                pout += 1;
            }

            // pout += xres_virtual - 240;
        }
    }

    return 0;
}

int display_fb_impl::show_gray(const unsigned char* graydata, int width, int height)
{
    if (pixel_format == 0 || pixel_format == 1)
    {
        // GRAY to RGBA8888 or BGRA8888
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = graydata + y * width;
            unsigned char* pout = (unsigned char*)fb_base + y * width * 4;// FIXME HARDCODE

            int x = 0;
#if __ARM_NEON
            uint8x16_t _v255 = vdupq_n_u8(255);
            for (; x + 15 < width; x += 16)
            {
                __builtin_prefetch(p + 16);
                uint8x16_t _gray = vld1q_u8(p);
                uint8x16x4_t _rgba;
                _rgba.val[0] = _gray;
                _rgba.val[1] = _gray;
                _rgba.val[2] = _gray;
                _rgba.val[3] = _v255;
                vst4q_u8(pout, _rgba);
                p += 16;
                pout += 64;
            }
            for (; x + 7 < width; x += 8)
            {
                uint8x8_t _gray = vld1_u8(p);
                uint8x8x4_t _rgba;
                _rgba.val[0] = _gray;
                _rgba.val[1] = _gray;
                _rgba.val[2] = _gray;
                _rgba.val[3] = vget_low_u8(_v255);
                vst4_u8(pout, _rgba);
                p += 8;
                pout += 32;
            }
#endif // __ARM_NEON
            for (; x < width; x++)
            {
                pout[0] = p[0];
                pout[1] = p[0];
                pout[2] = p[0];
                pout[3] = 255;
                p += 1;
                pout += 4;
            }

            // pout += xres_virtual - 240;
        }
    }
    if (pixel_format == 2 || pixel_format == 3)
    {
        // GRAY to RGB888 or BGR888
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = graydata + y * width;
            unsigned char* pout = (unsigned char*)fb_base + y * width * 3;// FIXME HARDCODE

            int x = 0;
#if __ARM_NEON
            for (; x + 15 < width; x += 16)
            {
                __builtin_prefetch(p + 16);
                uint8x16_t _gray = vld1q_u8(p);
                uint8x16x3_t _rgb;
                _rgb.val[0] = _gray;
                _rgb.val[1] = _gray;
                _rgb.val[2] = _gray;
                vst3q_u8(pout, _rgb);
                p += 16;
                pout += 48;
            }
            for (; x + 7 < width; x += 8)
            {
                uint8x8_t _gray = vld1_u8(p);
                uint8x8x3_t _rgb;
                _rgb.val[0] = _gray;
                _rgb.val[1] = _gray;
                _rgb.val[2] = _gray;
                vst3_u8(pout, _rgb);
                p += 8;
                pout += 24;
            }
#endif // __ARM_NEON
            for (; x < width; x++)
            {
                pout[0] = p[0];
                pout[1] = p[0];
                pout[2] = p[0];
                p += 1;
                pout += 3;
            }

            // pout += xres_virtual - 240;
        }
    }
    if (pixel_format == 4 || pixel_format == 5)
    {
        // GRAY to RGB565 or BGR565
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = graydata + y * width;
            unsigned short* pout = (unsigned short*)fb_base + y * width;// FIXME HARDCODE

            int x = 0;
#if __ARM_NEON
            for (; x + 15 < width; x += 16)
            {
                __builtin_prefetch(p + 16);
                uint8x16_t _gray = vld1q_u8(p);
                uint16x8_t _p0 = vshll_n_u8(vget_low_u8(_gray), 8);
                uint16x8_t _p1 = vshll_n_u8(vget_high_u8(_gray), 8);
                _p0 = vsriq_n_u16(_p0, vshll_n_u8(vget_low_u8(_gray), 8), 5);
                _p1 = vsriq_n_u16(_p1, vshll_n_u8(vget_high_u8(_gray), 8), 5);
                _p0 = vsriq_n_u16(_p0, vshll_n_u8(vget_low_u8(_gray), 8), 11);
                _p1 = vsriq_n_u16(_p1, vshll_n_u8(vget_high_u8(_gray), 8), 11);
                vst1q_u16(pout, _p0);
                vst1q_u16(pout + 8, _p1);
                p += 16;
                pout += 16;
            }
            for (; x + 7 < width; x += 8)
            {
                uint8x8_t _gray = vld1_u8(p);
                uint16x8_t _p = vshll_n_u8(_gray, 8);
                _p = vsriq_n_u16(_p, vshll_n_u8(_gray, 8), 5);
                _p = vsriq_n_u16(_p, vshll_n_u8(_gray, 8), 11);
                vst1q_u16(pout, _p);
                p += 8;
                pout += 8;
            }
#endif // __ARM_NEON
            for (; x < width; x++)
            {
                pout[0] = (p[0] >> 3 << 11) | (p[0] >> 2 << 5) | (p[0] >> 3);
                p += 1;
                pout += 1;
            }

            // pout += xres_virtual - 240;
        }
    }
    if (pixel_format == 6)
    {
        // GRAY to GRAY
        for (int y = 0; y < height; y++)
        {
            const unsigned char* p = graydata + y * width;
            unsigned char* pout = (unsigned char*)fb_base + y * width;// FIXME HARDCODE

            memcpy(pout, p, width);

            // pout += xres_virtual - 240;
        }
    }

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

    memset(&info, 0, sizeof(info));

    pixel_format = -1;

    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }

    if (ttyfd >= 0)
    {
        if (ioctl(ttyfd, 0x4b3a/*KDSETMODE*/, tty_oldmode))
        {
            fprintf(stderr, "ioctl KDSETMODE tty_oldmode failed %d %s\n", errno, strerror(errno));
        }

        // enable tty blinking cursor
        {
            const char buf[] = "\033[9;15]\033[?33h\033[?25h\033[?0c";
            ::write(ttyfd, buf, sizeof(buf));
        }

        ::close(ttyfd);
        ttyfd = -1;
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
    return d->info.xres;
}

int display_fb::get_height() const
{
    return d->info.yres;
}

int display_fb::show_bgr(const unsigned char* bgrdata, int width, int height)
{
    return d->show_bgr(bgrdata, width, height);
}

int display_fb::show_gray(const unsigned char* graydata, int width, int height)
{
    return d->show_gray(graydata, width, height);
}

int display_fb::close()
{
    return d->close();
}

} // namespace cv
