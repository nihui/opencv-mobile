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

#include "capture_v4l2.h"

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

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#if __ARM_NEON
#define STBI_NEON
#endif
#if __riscv_vector
#define STBI_RVV
#endif
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_PNM
#include "stb_image.h"

#include "opencv2/imgproc.hpp"

namespace cv {

class capture_v4l2_impl
{
public:
    capture_v4l2_impl();
    ~capture_v4l2_impl();

    int open(int index, int width, int height, float fps);

    int start_streaming();

    int read_frame(unsigned char* bgrdata);

    int stop_streaming();

    int close();

public:
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
};

capture_v4l2_impl::capture_v4l2_impl()
{
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

capture_v4l2_impl::~capture_v4l2_impl()
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

static float fit_score(int cap_width, int cap_height, int width, int height)
{
    int intersect_area = std::min(cap_width, width) * std::min(cap_height, height);
    int union_area = cap_width * cap_height + width * height - intersect_area;
    float iou = (float)intersect_area / union_area;
    float ioo = std::min(1.f, (float)intersect_area / (width * height));
    return iou * 20 + ioo * 80;
}

int capture_v4l2_impl::open(int index, int width, int height, float fps)
{
    int dev_index = -1;

    // try index first
    while (1)
    {
        sprintf(devpath, "/dev/video%d", index);

        fd = ::open(devpath, O_RDWR | O_NONBLOCK, 0);
        if (fd < 0)
            break;

        // query cap
        {
            struct v4l2_capability caps;
            memset(&caps, 0, sizeof(caps));

            if (ioctl(fd, VIDIOC_QUERYCAP, &caps))
            {
                fprintf(stderr, "%s ioctl VIDIOC_QUERYCAP failed %d %s\n", devpath, errno, strerror(errno));
                ::close(fd);
                break;
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
                ::close(fd);
                break;
            }

            if (!(caps.capabilities & V4L2_CAP_STREAMING))
            {
                fprintf(stderr, "%s is not V4L2_CAP_STREAMING\n", devpath);
                ::close(fd);
                break;
            }
        }

        dev_index = index;
        break;
    }

    if (dev_index == -1)
    {
        // enumerate /dev/video%d and find capture + streaming
        for (int i = 0; i < 64; i++)
        {
            sprintf(devpath, "/dev/video%d", i);

            fd = ::open(devpath, O_RDWR | O_NONBLOCK, 0);
            if (fd < 0)
                continue;

            // query cap
            {
                struct v4l2_capability caps;
                memset(&caps, 0, sizeof(caps));

                if (ioctl(fd, VIDIOC_QUERYCAP, &caps))
                {
                    fprintf(stderr, "%s ioctl VIDIOC_QUERYCAP failed %d %s\n", devpath, errno, strerror(errno));
                    ::close(fd);
                    continue;
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
                    ::close(fd);
                    continue;
                }

                if (!(caps.capabilities & V4L2_CAP_STREAMING))
                {
                    fprintf(stderr, "%s is not V4L2_CAP_STREAMING\n", devpath);
                    ::close(fd);
                    continue;
                }
            }

            dev_index = i;
            break;
        }
    }

    if (dev_index == -1)
    {
        fprintf(stderr, "cannot find v4l device with VIDEO_CAPTURE and STREAMING\n");
        goto OUT;
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

        // if (fmtdesc.flags & V4L2_FMT_FLAG_COMPRESSED)
        // {
        //     continue;
        // }

        if (fmtdesc.pixelformat != V4L2_PIX_FMT_MJPEG)
        {
            // we could only handle mjpg atm
            // TODO handle nv21
            continue;
        }

        if (cap_pixelformat == 0)
        {
            cap_pixelformat = fmtdesc.pixelformat;
        }

        // enumerate size
        float max_fs = 0.f;
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
                float fs = fit_score(w, h, width, height);
                fprintf(stderr, "       size = %d x %d   %.2f\n", w, h, fs);

                if (fs > max_fs)
                {
                    cap_width = w;
                    cap_height = h;
                    max_fs = fs;
                }
            }
            if (frmsizeenum.type == V4L2_FRMSIZE_TYPE_CONTINUOUS)
            {
                __u32 minw = frmsizeenum.stepwise.min_width;
                __u32 maxw = frmsizeenum.stepwise.max_width;
                __u32 minh = frmsizeenum.stepwise.min_height;
                __u32 maxh = frmsizeenum.stepwise.max_height;
                __u32 w;
                __u32 h;
                {
                    if (width / (float)height > maxw / (float)maxh)
                    {
                        // fatter
                        h = (width * maxh / maxw + 1) / 2 * 2;
                        w = (width + 15) / 16 * 16;
                    }
                    else
                    {
                        // thinner
                        w = (height * maxw / maxh + 15) / 16 * 16;
                        h = (height + 1) / 2 * 2;
                    }

                    if (w < minw || h < minh)
                    {
                        w = minw;
                        h = minh;
                    }
                }
                float fs = fit_score(w, h, width, height);
                fprintf(stderr, "       size = %d x %d   %.2f\n", w, h, fs);

                if (fs > max_fs)
                {
                    cap_width = w;
                    cap_height = h;
                    max_fs = fs;
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
                sw = least_common_multiple(sw, 16);
                sh = least_common_multiple(sh, 2);
                __u32 w;
                __u32 h;
                {
                    if (width / (float)height > maxw / (float)maxh)
                    {
                        // fatter
                        h = (width * maxh / maxw + sh - 1) / sh * sh;
                        w = (width + sw - 1) / sw * sw;
                    }
                    else
                    {
                        // thinner
                        w = (height * maxw / maxh + sw - 1) / sw * sw;
                        h = (height + sh - 1) / sh * sh;
                    }

                    if (w < minw || h < minh)
                    {
                        w = minw;
                        h = minh;
                    }
                }
                float fs = fit_score(w, h, width, height);
                fprintf(stderr, "       size = %d x %d   %.2f\n", w, h, fs);

                if (fs > max_fs)
                {
                    cap_width = w;
                    cap_height = h;
                    max_fs = fs;
                }
            }

            if (frmsizeenum.type != V4L2_FRMSIZE_TYPE_DISCRETE)
                break;
        }
    }

    // enumerate fps
    {
        float min_fps_diff = 99999;
        for (int i = 0; ; i++)
        {
            struct v4l2_frmivalenum frmivalenum;
            memset(&frmivalenum, 0, sizeof(frmivalenum));
            frmivalenum.index = i;
            frmivalenum.pixel_format = cap_pixelformat;
            frmivalenum.width = cap_width;
            frmivalenum.height = cap_height;

            if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmivalenum))
            {
                if (errno == EINVAL)
                    break;

                fprintf(stderr, "%s ioctl VIDIOC_ENUM_FRAMEINTERVALS failed %d %s\n", devpath, errno, strerror(errno));
                goto OUT;
            }

            if (frmivalenum.type == V4L2_FRMIVAL_TYPE_DISCRETE)
            {
                __u32 numer = frmivalenum.discrete.numerator;
                __u32 denom = frmivalenum.discrete.denominator;
                float fps_diff = fabs((float)denom / numer - fps);
                fprintf(stderr, "           fps = %d / %d   %.2f\n", numer, denom, fps_diff);

                if (fps_diff < min_fps_diff)
                {
                    cap_numerator = numer;
                    cap_denominator = denom;
                    min_fps_diff = fps_diff;
                }
            }
            if (frmivalenum.type == V4L2_FRMIVAL_TYPE_CONTINUOUS)
            {
                __u32 min_numer = frmivalenum.stepwise.min.numerator;
                __u32 max_numer = frmivalenum.stepwise.max.numerator;
                __u32 min_denom = frmivalenum.stepwise.min.denominator;
                __u32 max_denom = frmivalenum.stepwise.max.denominator;
                float fps_min = (float)min_denom / min_numer;
                float fps_max = (float)max_denom / max_numer;
                float fps2 = std::max(std::min(fps, fps_max), fps_min);
                __u32 numer = min_numer;
                __u32 denom = min_numer * fps2;
                float fps_diff = fabs(fps2 - fps);
                fprintf(stderr, "           fps = %d / %d  ~   %d / %d   %.2f\n", min_numer, min_denom, max_numer, max_denom, fps_diff);

                if (fps_diff < min_fps_diff)
                {
                    cap_numerator = numer;
                    cap_denominator = denom;
                    min_fps_diff = fps_diff;
                }
            }
            if (frmivalenum.type == V4L2_FRMIVAL_TYPE_STEPWISE)
            {
                __u32 min_numer = frmivalenum.stepwise.min.numerator;
                __u32 max_numer = frmivalenum.stepwise.max.numerator;
                __u32 snumer = frmivalenum.stepwise.step.numerator;
                __u32 min_denom = frmivalenum.stepwise.min.denominator;
                __u32 max_denom = frmivalenum.stepwise.max.denominator;
                __u32 sdenom = frmivalenum.stepwise.step.denominator;
                float fps_min = (float)min_denom / min_numer;
                float fps_max = (float)max_denom / max_numer;
                float fps2 = std::max(std::min(fps, fps_max), fps_min);
                __u32 numer;
                __u32 denom;
                if (min_numer == max_numer)
                {
                    numer = min_numer;
                    denom = min_denom + (int)(fps2 * min_numer - min_denom + sdenom / 2) / sdenom * sdenom;
                }
                else
                {
                    numer = min_numer + (int)(min_denom / fps2 - min_numer + snumer / 2) / snumer * snumer;
                    denom = min_denom;
                }
                fps2 = (float)(denom) / numer;
                float fps_diff = fabs(fps2 - fps);
                fprintf(stderr, "           fps = %d / %d  ~   %d / %d  (+%d +%d)   %.2f\n", min_numer, min_denom, max_numer, max_denom, snumer, sdenom, fps_diff);

                if (fps_diff < min_fps_diff)
                {
                    cap_numerator = numer;
                    cap_denominator = denom;
                    min_fps_diff = fps_diff;
                }
            }

            if (frmivalenum.type != V4L2_FRMIVAL_TYPE_DISCRETE)
                break;
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

    return 0;

OUT:

    close();
    return -1;
}

int capture_v4l2_impl::start_streaming()
{
    v4l2_buf_type type = buf_type;
    if (ioctl(fd, VIDIOC_STREAMON, &type))
    {
        fprintf(stderr, "%s ioctl VIDIOC_STREAMON failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    return 0;
}

int capture_v4l2_impl::read_frame(unsigned char* bgrdata)
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
        int i = buf.index;

        __u32 offset = 0;
        __u32 bytesused = 0;
        if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            offset = buf.m.planes[0].data_offset;
            bytesused = buf.m.planes[0].bytesused - offset;
        }
        else
        {
            bytesused = buf.bytesused;
            // offset = buf.m.offset;
        }

        // fprintf(stderr, "buf.index %d\n", buf.index);
        // fprintf(stderr, "offset %d\n", offset);
        // fprintf(stderr, "bytesused %d\n", bytesused);
        // V4L2_PIX_FMT_NV21
        // yuv420sp2bgr((const unsigned char*)data[i] + offset, final_width, final_height, bgrdata);
        // {
        //     FILE* fp = fopen("tmp.bin", "wb");
        //     fwrite((const unsigned char*)data[i] + offset, 1, bytesused, fp);
        //     fclose(fp);
        // }

        int w;
        int h;
        int c;
        int desired_channels = 3;
        unsigned char* pixeldata = stbi_load_from_memory((const unsigned char*)data[i] + offset, bytesused, &w, &h, &c, desired_channels);

        // crop and resize to output
        cv::Mat cap_rgb(cap_height, cap_width, CV_8UC3, (void*)pixeldata);
        cv::Mat output_bgr(output_height, output_width, CV_8UC3, (void*)bgrdata);
        cv::Mat crop_rgb = cap_rgb(cv::Rect((cap_width-crop_width)/2, (cap_height-crop_height)/2, crop_width, crop_height));
        cv::resize(crop_rgb, output_bgr, cv::Size(output_width, output_height));
        cv::cvtColor(output_bgr, output_bgr, cv::COLOR_RGB2BGR);

        stbi_image_free(pixeldata);
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

int capture_v4l2_impl::stop_streaming()
{
    v4l2_buf_type type = buf_type;
    if (ioctl(fd, VIDIOC_STREAMOFF, &type))
    {
        fprintf(stderr, "%s ioctl VIDIOC_STREAMOFF failed %d %s\n", devpath, errno, strerror(errno));
        return -1;
    }

    return 0;
}

int capture_v4l2_impl::close()
{
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

    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }

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

bool capture_v4l2::supported()
{
    return true;
}

capture_v4l2::capture_v4l2() : d(new capture_v4l2_impl)
{
}

capture_v4l2::~capture_v4l2()
{
    delete d;
}

int capture_v4l2::open(int index, int width, int height, float fps)
{
    return d->open(index, width, height, fps);
}

int capture_v4l2::get_width() const
{
    return d->output_width;
}

int capture_v4l2::get_height() const
{
    return d->output_height;
}

float capture_v4l2::get_fps() const
{
    return d->cap_numerator ? d->cap_denominator / (float)d->cap_numerator : 0;
}

int capture_v4l2::start_streaming()
{
    return d->start_streaming();
}

int capture_v4l2::read_frame(unsigned char* bgrdata)
{
    return d->read_frame(bgrdata);
}

int capture_v4l2::stop_streaming()
{
    return d->stop_streaming();
}

int capture_v4l2::close()
{
    return d->close();
}

} // namespace cv
