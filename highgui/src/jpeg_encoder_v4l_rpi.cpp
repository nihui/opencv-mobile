//
// Copyright (C) 2025 nihui
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

#include "jpeg_encoder_v4l_rpi.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#if __ARM_NEON
#include <arm_neon.h>
#endif

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace cv {

// 0 = unknown
// 1 = raspberry-pi
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

        if (strncmp(buf, "Raspberry Pi", 12) == 0)
        {
            // raspberry pi family
            device_model = 1;

            if (strncmp(buf, "Raspberry Pi 5", 14) == 0)
            {
                // rpi5 has no hw jpg codec
                device_model = 0;
            }
        }
    }

    if (device_model > 0)
    {
        fprintf(stderr, "opencv-mobile HW JPG encoder with v4l rpi\n");
    }

    return device_model;
}

static bool is_device_whitelisted()
{
    return get_device_model() > 0;
}

class v4l2_m2m_device
{
public:
    v4l2_m2m_device();
    ~v4l2_m2m_device();

    int open(int index);
    void close();

    int check_capability();

    int set_input_format_size(__u32 format, __u32 colorspace, __u32 width, __u32 height, __u32 bytesperline);
    int set_output_format_size(__u32 format, __u32 colorspace, __u32 width, __u32 height, __u32 bytesperline);

    int control(__u32 id, __s32 value);

    int request_input_buffer(__u32 memory);
    int request_output_buffer(__u32 memory);

    int stream_on();
    int stream_off();

    typedef __u32 (*PFN_send_buffer)(void* data, __u32 data_length, __u32 width, __u32 height, __u32 bytesperline, void* userdata);
    typedef __u32 (*PFN_recv_buffer)(const void* data, __u32 data_length, __u32 width, __u32 height, __u32 bytesperline, void* userdata);

    int send(PFN_send_buffer sb_callback, void* userdata) const;
    int recv(PFN_recv_buffer rb_callback, void* userdata) const;

public:
    int fd;
    int index;

    __u32 input_width;
    __u32 input_height;
    __u32 input_bytesperline;

    __u32 output_width;
    __u32 output_height;
    __u32 output_bytesperline;

    v4l2_buf_type input_buf_type;
    __u32 input_memory;
    void* input_data;
    __u32 input_data_length;
    // int input_dmafd;

    v4l2_buf_type output_buf_type;
    __u32 output_memory;
    void* output_data;
    __u32 output_data_length;
    int output_dmafd;

    int input_stream_on;
    int output_stream_on;

    mutable struct timeval g_tv;
};

v4l2_m2m_device::v4l2_m2m_device()
{
    fd = -1;
    index = -1;

    input_width = 0;
    input_height = 0;
    input_bytesperline = 0;

    output_width = 0;
    output_height = 0;
    output_bytesperline = 0;

    input_data = 0;
    input_data_length = 0;
    // input_dmafd = -1;

    output_data = 0;
    output_data_length = 0;
    output_dmafd = -1;

    input_stream_on = 0;
    output_stream_on = 0;
}

v4l2_m2m_device::~v4l2_m2m_device()
{
    close();
}

int v4l2_m2m_device::open(int _index)
{
    char devpath[32];
    sprintf(devpath, "/dev/video%d", _index);

    fd = ::open(devpath, O_RDWR | O_NONBLOCK, 0);
    if (fd < 0)
    {
        fprintf(stderr, "open %s failed %d %s\n", _index, errno, strerror(errno));
        return -1;
    }

    index = _index;

    return 0;
}

void v4l2_m2m_device::close()
{
    stream_off();

    if (input_data)
    {
        munmap(input_data, input_data_length);
        input_data = 0;
        input_data_length = 0;
    }

    // if (input_dmafd >= 0)
    // {
    //     ::close(input_dmafd);
    //     input_dmafd = -1;
    // }

    if (output_data)
    {
        munmap(output_data, output_data_length);
        output_data = 0;
        output_data_length = 0;
    }

    if (output_dmafd >= 0)
    {
        ::close(output_dmafd);
        output_dmafd = -1;
    }

    if (fd >= 0)
    {
        ::close(fd);
        fd = -1;
    }

    index = -1;

    input_width = 0;
    input_height = 0;
    input_bytesperline = 0;

    output_width = 0;
    output_height = 0;
    output_bytesperline = 0;
}

int v4l2_m2m_device::check_capability()
{
    struct v4l2_capability caps;
    memset(&caps, 0, sizeof(caps));

    if (ioctl(fd, VIDIOC_QUERYCAP, &caps))
    {
        fprintf(stderr, "/dev/video%d ioctl VIDIOC_QUERYCAP failed %d %s\n", index, errno, strerror(errno));
        return -1;
    }

    if (caps.capabilities & V4L2_CAP_VIDEO_M2M)
    {
        input_buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        output_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    else if (caps.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)
    {
        input_buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        output_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    }
    else
    {
        fprintf(stderr, "/dev/video%d is not V4L2_CAP_VIDEO_M2M or V4L2_CAP_VIDEO_M2M_MPLANE\n", index);
        return -1;
    }

    if (!(caps.capabilities & V4L2_CAP_STREAMING))
    {
        fprintf(stderr, "/dev/video%d is not V4L2_CAP_STREAMING\n", index);
        return -1;
    }

    return 0;
}

int v4l2_m2m_device::set_input_format_size(__u32 format, __u32 colorspace, __u32 width, __u32 height, __u32 bytesperline)
{
    // set input format and size
    {
        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = input_buf_type;
        if (input_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
        {
            fmt.fmt.pix.width = width;
            fmt.fmt.pix.height = height;
            fmt.fmt.pix.pixelformat = format;
            fmt.fmt.pix.bytesperline = bytesperline;
            fmt.fmt.pix.colorspace = colorspace;
        }
        else // if (input_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
        {
            fmt.fmt.pix_mp.width = width;
            fmt.fmt.pix_mp.height = height;
            fmt.fmt.pix_mp.pixelformat = format;
            fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
            fmt.fmt.pix_mp.colorspace = colorspace;
            fmt.fmt.pix_mp.num_planes = 1;
            fmt.fmt.pix_mp.plane_fmt[0].bytesperline = bytesperline;
        }

        if (ioctl(fd, VIDIOC_S_FMT, &fmt))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_S_FMT input failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        if (ioctl(fd, VIDIOC_G_FMT, &fmt))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_G_FMT input failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        if (input_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
        {
            input_width = fmt.fmt.pix.width;
            input_height = fmt.fmt.pix.height;
            input_bytesperline = fmt.fmt.pix.bytesperline;
        }
        else // if (input_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
        {
            input_width = fmt.fmt.pix_mp.width;
            input_height = fmt.fmt.pix_mp.height;
            input_bytesperline = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
        }


        // fprintf(stderr, "/dev/video%d input width = %d %d\n", index, width, input_width);
        // fprintf(stderr, "/dev/video%d input height = %d %d\n", index, height, input_height);
        // fprintf(stderr, "/dev/video%d input bytesperline = %d\n", index, input_bytesperline);
    }

    return 0;
}

int v4l2_m2m_device::set_output_format_size(__u32 format, __u32 colorspace, __u32 width, __u32 height, __u32 bytesperline)
{
    // set output format and size
    {
        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = output_buf_type;
        if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
        {
            fmt.fmt.pix.width = width;
            fmt.fmt.pix.height = height;
            fmt.fmt.pix.pixelformat = format;
            fmt.fmt.pix.bytesperline = bytesperline;
            fmt.fmt.pix.colorspace = colorspace;
        }
        else // if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            fmt.fmt.pix_mp.width = width;
            fmt.fmt.pix_mp.height = height;
            fmt.fmt.pix_mp.pixelformat = format;
            fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
            fmt.fmt.pix_mp.colorspace = colorspace;
            fmt.fmt.pix_mp.num_planes = 1;
            fmt.fmt.pix_mp.plane_fmt[0].bytesperline = bytesperline;
        }

        if (ioctl(fd, VIDIOC_S_FMT, &fmt))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_S_FMT output failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        if (ioctl(fd, VIDIOC_G_FMT, &fmt))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_G_FMT output failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
        {
            output_width = fmt.fmt.pix.width;
            output_height = fmt.fmt.pix.height;
            output_bytesperline = fmt.fmt.pix.bytesperline;
        }
        else // if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            output_width = fmt.fmt.pix_mp.width;
            output_height = fmt.fmt.pix_mp.height;
            output_bytesperline = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
        }

        // fprintf(stderr, "/dev/video%d output width = %d %d\n", index, width, output_width);
        // fprintf(stderr, "/dev/video%d output height = %d %d\n", index, height, output_height);
        // fprintf(stderr, "/dev/video%d output bytesperline = %d\n", index, output_bytesperline);
    }

    return 0;
}

int v4l2_m2m_device::control(__u32 id, __s32 value)
{
    struct v4l2_control ctl;
    memset(&ctl, 0, sizeof(ctl));
    ctl.id = id;
    ctl.value = value;

    if (ioctl(fd, VIDIOC_S_CTRL, &ctl))
    {
        fprintf(stderr, "/dev/video%d ioctl VIDIOC_S_CTRL %x failed %d %s\n", index, id, errno, strerror(errno));
        return -1;
    }

    return 0;
}

int v4l2_m2m_device::request_input_buffer(__u32 _memory)
{
    struct v4l2_requestbuffers requestbuffers;
    memset(&requestbuffers, 0, sizeof(requestbuffers));
    requestbuffers.count = 1;
    requestbuffers.type = input_buf_type;
    requestbuffers.memory = _memory;
    if (ioctl(fd, VIDIOC_REQBUFS, &requestbuffers))
    {
        fprintf(stderr, "/dev/video%d ioctl VIDIOC_REQBUFS input failed %d %s\n", index, errno, strerror(errno));
        return -1;
    }

    // fprintf(stderr, "requestbuffers.count = %d\n", requestbuffers.count);

    {
        struct v4l2_plane plane;
        memset(&plane, 0, sizeof(plane));

        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = input_buf_type;
        buf.memory = _memory;
        buf.index = 0;

        if (input_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
        {
            buf.m.planes = &plane;
            buf.length = 1;
        }

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_QUERYBUF input failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        // fprintf(stderr, "planes count = %d\n", buf.length);

        if (input_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
        {
            input_data_length = buf.m.planes[0].length;
        }
        else
        {
            input_data_length = buf.length;
        }

        // fprintf(stderr, "/dev/video%d input_data %d\n", index, input_data_length);

        if (_memory == V4L2_MEMORY_MMAP)
        {
            if (input_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
            {
                input_data = mmap(NULL, input_data_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.planes[0].m.mem_offset);
            }
            else
            {
                input_data = mmap(NULL, input_data_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
            }
        }
    }

    input_memory = _memory;

    return 0;
}

int v4l2_m2m_device::request_output_buffer(__u32 _memory)
{
    struct v4l2_requestbuffers requestbuffers;
    memset(&requestbuffers, 0, sizeof(requestbuffers));
    requestbuffers.count = 1;
    requestbuffers.type = output_buf_type;
    requestbuffers.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &requestbuffers))
    {
        fprintf(stderr, "/dev/video%d ioctl VIDIOC_REQBUFS output failed %d %s\n", index, errno, strerror(errno));
        return -1;
    }

    // fprintf(stderr, "requestbuffers.count = %d\n", requestbuffers.count);

    {
        struct v4l2_plane plane;
        memset(&plane, 0, sizeof(plane));

        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = output_buf_type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = 0;

        if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            buf.m.planes = &plane;
            buf.length = 1;
        }

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_QUERYBUF output failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        // fprintf(stderr, "planes count = %d\n", buf.length);

        if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            output_data_length = buf.m.planes[0].length;
        }
        else
        {
            output_data_length = buf.length;
        }

        // fprintf(stderr, "/dev/video%d output_data %d\n", index, output_data_length);

        if (_memory == V4L2_MEMORY_MMAP)
        {
            if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
            {
                output_data = mmap(NULL, output_data_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.planes[0].m.mem_offset);
            }
            else
            {
                output_data = mmap(NULL, output_data_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
            }
        }
    }

    if (_memory == V4L2_MEMORY_DMABUF)
    {
        struct v4l2_exportbuffer expbuf;
        memset(&expbuf, 0, sizeof(expbuf));
        expbuf.type = output_buf_type;
        expbuf.index = 0;
        expbuf.plane = 0;

        if (ioctl(fd, VIDIOC_EXPBUF, &expbuf))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_EXPBUF output failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        output_dmafd = expbuf.fd;
    }

    // queue output buffer
    {
        struct v4l2_plane plane;
        memset(&plane, 0, sizeof(plane));

        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));

        buf.type = output_buf_type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = 0;

        if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            buf.m.planes = &plane;
            buf.length = 1;
        }

        if (ioctl(fd, VIDIOC_QBUF, &buf))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_QBUF output failed\n", index);
            return -1;
        }
    }

    output_memory = _memory;

    return 0;
}

int v4l2_m2m_device::stream_on()
{
    if (!input_stream_on)
    {
        if (ioctl(fd, VIDIOC_STREAMON, &input_buf_type))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_STREAMON input failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        input_stream_on = 1;
    }

    if (!output_stream_on)
    {
        if (ioctl(fd, VIDIOC_STREAMON, &output_buf_type))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_STREAMON output failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        output_stream_on = 1;
    }

    return 0;
}

int v4l2_m2m_device::stream_off()
{
    if (input_stream_on)
    {
        if (ioctl(fd, VIDIOC_STREAMOFF, &input_buf_type))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_STREAMOFF input failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        input_stream_on = 0;
    }

    if (output_stream_on)
    {
        if (ioctl(fd, VIDIOC_STREAMOFF, &output_buf_type))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_STREAMOFF output failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        output_stream_on = 0;
    }

    return 0;
}

int v4l2_m2m_device::send(PFN_send_buffer sb_callback, void* userdata) const
{
    if (!sb_callback)
    {
        fprintf(stderr, "send_buffer callback is NULL\n");
        return -1;
    }

    struct v4l2_plane plane;
    memset(&plane, 0, sizeof(plane));

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = input_buf_type;
    buf.memory = input_memory;
    buf.index = 0;

    int input_dmafd = -1;
    __u32 bytesused = 0;
    if (input_memory == V4L2_MEMORY_MMAP)
    {
        bytesused = sb_callback(input_data, input_data_length, input_width, input_height, input_bytesperline, userdata);
    }
    else // if (input_memory == V4L2_MEMORY_DMABUF)
    {
        bytesused = sb_callback(&input_dmafd, input_data_length, input_width, input_height, input_bytesperline, userdata);

        if (input_dmafd == -1)
        {
            fprintf(stderr, "send_buffer callback set invalid input_dmafd\n");
            return -1;
        }

        // fprintf(stderr, "%s input_dmafd = %d\n", devpath, input_dmafd);
    }

    gettimeofday(&g_tv, NULL);

    buf.timestamp.tv_sec = g_tv.tv_sec;
    buf.timestamp.tv_usec = g_tv.tv_usec;

    buf.bytesused = bytesused;
    buf.field = V4L2_FIELD_ANY;

    // fprintf(stderr, "%s inbuf flags = %x    %d   %lu %lu\n", devpath, buf.flags, bytesused, buf.timestamp.tv_sec, buf.timestamp.tv_usec);

    if (input_memory == V4L2_MEMORY_DMABUF)
    {
        buf.m.fd = input_dmafd;
    }

    if (input_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
    {
        if (input_memory == V4L2_MEMORY_DMABUF)
        {
            plane.m.fd = input_dmafd;
            plane.bytesused = bytesused;

            plane.length = input_data_length;//1920*1088/2*3;
        }

        buf.m.planes = &plane;
        buf.length = 1;
    }

    if (ioctl(fd, VIDIOC_QBUF, &buf))
    {
        fprintf(stderr, "/dev/video%d ioctl VIDIOC_QBUF input failed %d %s\n", index, errno, strerror(errno));
        return -1;
    }

    return 0;
}

int v4l2_m2m_device::recv(PFN_recv_buffer rb_callback, void* userdata) const
{
    if (!rb_callback)
    {
        fprintf(stderr, "recv_buffer callback is NULL\n");
        return -1;
    }

    while (1)
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
            fprintf(stderr, "select /dev/video%d failed\n", index);
            return -1;
        }

        if (r == 0)
        {
            fprintf(stderr, "select /dev/video%d timeout\n", index);
            return -1;
        }

        // dqbuf input
        {
            struct v4l2_plane plane;
            memset(&plane, 0, sizeof(plane));

            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = input_buf_type;
            buf.memory = input_memory;

            // if (input_memory == V4L2_MEMORY_DMABUF)
            // {
            //     buf.m.fd = input_dmafd;
            // }

            if (input_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
            {
                // if (input_memory == V4L2_MEMORY_DMABUF)
                // {
                //     plane.m.fd = input_dmafd;
                // }

                buf.m.planes = &plane;
                buf.length = 1;
            }

            if (ioctl(fd, VIDIOC_DQBUF, &buf))
            {
                fprintf(stderr, "/dev/video%d ioctl VIDIOC_DQBUF input failed %d %s\n", index, errno, strerror(errno));
                return -1;
            }
        }

        // dqbuf output
        struct v4l2_plane plane;
        memset(&plane, 0, sizeof(plane));

        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = output_buf_type;
        buf.memory = output_memory;

        if (output_memory == V4L2_MEMORY_DMABUF)
        {
            buf.m.fd = output_dmafd;
        }

        if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            if (output_memory == V4L2_MEMORY_DMABUF)
            {
                plane.m.fd = output_dmafd;
            }

            buf.m.planes = &plane;
            buf.length = 1;
        }

        if (ioctl(fd, VIDIOC_DQBUF, &buf))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_DQBUF output failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        // fprintf(stderr, "%s outbuf flags = %x    %d   %lu %lu\n", devpath, buf.flags, plane.length, buf.timestamp.tv_sec, buf.timestamp.tv_usec);

        bool done = true;

        if (buf.flags & V4L2_BUF_FLAG_TIMESTAMP_COPY)
        {
            done = (g_tv.tv_sec == buf.timestamp.tv_sec && g_tv.tv_usec == buf.timestamp.tv_usec);
        }

        if (buf.flags & V4L2_BUF_FLAG_LAST)
        {
            done = true;
        }

        if (done)
        {
            if (output_memory == V4L2_MEMORY_MMAP)
            {
                if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
                {
                    rb_callback(output_data, plane.bytesused, output_width, output_height, output_bytesperline, userdata);
                }
                else
                {
                    rb_callback(output_data, buf.bytesused, output_width, output_height, output_bytesperline, userdata);
                }
            }
            else // if (output_memory == V4L2_MEMORY_DMABUF)
            {
                rb_callback(&output_dmafd, output_data_length, output_width, output_height, output_bytesperline, userdata);
            }
        }

        // fprintf(stderr, "%s outbuf flags = %x    %d   %lu %lu  qqq\n", devpath, buf.flags, buf.bytesused, buf.timestamp.tv_sec, buf.timestamp.tv_usec);

        // requeue buf
        // memset(&plane, 0, sizeof(plane));

        if (output_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            buf.m.planes = &plane;
            buf.length = 1;
        }

        if (ioctl(fd, VIDIOC_QBUF, &buf))
        {
            fprintf(stderr, "/dev/video%d ioctl VIDIOC_QBUF output failed %d %s\n", index, errno, strerror(errno));
            return -1;
        }

        if (done)
            break;
    }

    return 0;
}

class jpeg_encoder_v4l_rpi_impl
{
public:
    jpeg_encoder_v4l_rpi_impl();
    ~jpeg_encoder_v4l_rpi_impl();

    int init(int width, int height, int ch, int quality);

    int encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const;

    int encode(const unsigned char* bgrdata, const char* outfilepath) const;

    int deinit();

protected:
    int inited;
    int width;
    int height;
    int ch;
    int quality;
    v4l2_m2m_device isp;
    v4l2_m2m_device enc;
};

jpeg_encoder_v4l_rpi_impl::jpeg_encoder_v4l_rpi_impl()
{
    inited = 0;
    width = 0;
    height = 0;
    ch = 0;
    quality = 0;
}

jpeg_encoder_v4l_rpi_impl::~jpeg_encoder_v4l_rpi_impl()
{
    deinit();
}

int jpeg_encoder_v4l_rpi_impl::init(int _width, int _height, int _ch, int _quality)
{
    if (!is_device_whitelisted())
        return false;

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
    quality = _quality;

    // enumerate /sys/class/video4linux/videoN/name and find bcm2835-codec-isp and bcm2835-codec-encode_image
    int bcm2835_isp_index = -1;
    int bcm2835_encode_image_index = -1;
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

        if (strncmp(line, "bcm2835-codec-isp", 17) == 0)
            bcm2835_isp_index = i;

        if (strncmp(line, "bcm2835-codec-encode_image", 26) == 0)
            bcm2835_encode_image_index = i;

        if (bcm2835_isp_index != -1 && bcm2835_encode_image_index != -1)
            break;
    }

    if (bcm2835_isp_index == -1)
    {
        fprintf(stderr, "cannot find v4l device with name bcm2835-codec-isp\n");
        return -1;
    }

    if (bcm2835_encode_image_index == -1)
    {
        fprintf(stderr, "cannot find v4l device with name bcm2835-codec-encode_image\n");
        return -1;
    }


    isp.open(bcm2835_isp_index);
    enc.open(bcm2835_encode_image_index);

    isp.check_capability();
    enc.check_capability();

    // isp  2   2  64
    // enc  2  16  32


    //  yuv420p-align   isp-out enc-in
    //      width          2      2
    //      height         2     16
    //  bytesperline      64     32

    // negotiate
    //  me: isp，来了个1888x1082的BGR，你可以吐出的yuv420吗？
    // isp: 可以，但是stride会是1920
    //  me: enc，你可以吃1888x1082，并且stride是1920的yuv420吗？
    // enc: 不吃，不够高，我要吃1888x1088
    //  me: isp，你能吐出1888x1088的yuv420吗？
    // isp: 我可以只吐出前面1082高的数据，后面的我不管啦
    //  me: enc，给你吃1888x1088，但是stride是1920
    // enc: 我吃吃看，不过我还是喜欢stride是1088的
    //  me: isp，你能吐出1088的stride吗？
    // isp: 我觉得我不能
    //  me: enc，isp做不到啊，你还是忍着点吃1920的stride吧
    // enc: 那就吃吃看吧，我不确定会不会出什么问题...


    int yuv_width = width;
    int yuv_height = (height + 15) / 16 * 16;
    int yuv_bytesperline = (width + 63) / 64 * 64;

    __u32 input_pixel_format = 0;
    __u32 input_pixel_bytesperline = 0;
    if (ch == 1)
    {
        input_pixel_format = V4L2_PIX_FMT_GREY;
        input_pixel_bytesperline = yuv_width;
    }
    if (ch == 3 || ch == 4)
    {
        input_pixel_format = V4L2_PIX_FMT_BGR24;
        input_pixel_bytesperline = yuv_width * 3;
    }

    isp.set_input_format_size(input_pixel_format, V4L2_COLORSPACE_SRGB, yuv_width, yuv_height, input_pixel_bytesperline);
    isp.set_output_format_size(V4L2_PIX_FMT_YUV420, V4L2_COLORSPACE_JPEG, yuv_width, yuv_height, yuv_bytesperline);
    enc.set_input_format_size(V4L2_PIX_FMT_YUV420, V4L2_COLORSPACE_JPEG, width, height, yuv_bytesperline);
    enc.set_output_format_size(V4L2_PIX_FMT_JPEG, V4L2_COLORSPACE_DEFAULT, width, height, 0);

    enc.control(V4L2_CID_JPEG_COMPRESSION_QUALITY, quality);

    isp.request_input_buffer(V4L2_MEMORY_MMAP);
    isp.request_output_buffer(V4L2_MEMORY_DMABUF);

    enc.request_input_buffer(V4L2_MEMORY_DMABUF);
    enc.request_output_buffer(V4L2_MEMORY_MMAP);

    isp.stream_on();
    enc.stream_on();

    inited = 1;

    return 0;
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

struct isp_context
{
    const unsigned char* bgrdata;
    int width;
    int height;
    int ch;
};

struct enc_context
{
    std::vector<unsigned char>* outdata;
    const char* outfilepath;
    const v4l2_m2m_device* enc;
};

static __u32 on_enc_send(void* input_data, __u32 input_data_length, __u32 width, __u32 height, __u32 bytesperline, void* userdata)
{
    int isp_output_dmafd = *(int*)userdata;
    int* enc_input_dmafd = (int*)input_data;

    // fprintf(stderr, "on_enc_send %d\n", isp_output_dmafd);
    *enc_input_dmafd = isp_output_dmafd;

    return 0;
}

static __u32 on_enc_recv(const void* output_data, __u32 output_data_length, __u32 width, __u32 height, __u32 bytesperline, void* userdata)
{
    std::vector<unsigned char>* outdata = (std::vector<unsigned char>*)userdata;
    // fprintf(stderr, "on_enc_recv %d\n", output_data_length);

    outdata->resize(output_data_length);
    memcpy((void*)outdata->data(), output_data, output_data_length);

    return output_data_length;
}

static __u32 on_enc_recv_file(const void* output_data, __u32 output_data_length, __u32 width, __u32 height, __u32 bytesperline, void* userdata)
{
    const char* outfilepath = (const char*)userdata;
    // fprintf(stderr, "on_enc_recv_file %d\n", output_data_length);

    FILE* fp = fopen(outfilepath, "wb");
    fwrite(output_data, 1, output_data_length, fp);
    fclose(fp);

    return output_data_length;
}

static __u32 on_isp_send(void* input_data, __u32 input_data_length, __u32 width, __u32 height, __u32 bytesperline, void* userdata)
{
    struct isp_context* ctx = (struct isp_context*)userdata;

    // fprintf(stderr, "on_isp_send %lu\n", input_data_length);

    if (ctx->ch == 4)
    {
        for (int y = 0; y < ctx->height; y++)
        {
            my_memcpy_drop_alpha((unsigned char*)input_data + y * bytesperline, ctx->bgrdata + y * ctx->width * 4, ctx->width);
        }
    }
    else // if (ctx->ch == 1 || ctx->ch == 3)
    {
        for (int y = 0; y < ctx->height; y++)
        {
            my_memcpy((unsigned char*)input_data + y * bytesperline, ctx->bgrdata + y * ctx->width * ctx->ch, ctx->width * ctx->ch);
        }
    }

    return input_data_length;
}

static __u32 on_isp_recv(const void* output_data, __u32 output_data_length, __u32 width, __u32 height, __u32 bytesperline, void* userdata)
{
    struct enc_context* ctx = (struct enc_context*)userdata;

    int yuv_dmafd = *(int*)output_data;

    // fprintf(stderr, "on_isp_recv %d\n", yuv_dmafd);

    ctx->enc->send(on_enc_send, (void*)&yuv_dmafd);

    if (ctx->outdata)
    {
        ctx->enc->recv(on_enc_recv, (void*)ctx->outdata);
    }
    else
    {
        ctx->enc->recv(on_enc_recv_file, (void*)ctx->outfilepath);
    }

    return output_data_length;
}

int jpeg_encoder_v4l_rpi_impl::encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const
{
    outdata.clear();

    if (!is_device_whitelisted())
        return false;

    if (!inited)
    {
        fprintf(stderr, "not inited\n");
        return -1;
    }

    struct isp_context isp_ctx = { bgrdata, width, height, ch };

    isp.send(on_isp_send, (void*)&isp_ctx);

    struct enc_context enc_ctx = { &outdata, 0, &enc };

    isp.recv(on_isp_recv, (void*)&enc_ctx);

    return 0;
}

int jpeg_encoder_v4l_rpi_impl::encode(const unsigned char* bgrdata, const char* outfilepath) const
{
    if (!is_device_whitelisted())
        return false;

    if (!inited)
    {
        fprintf(stderr, "not inited\n");
        return -1;
    }

    struct isp_context isp_ctx = { bgrdata, width, height, ch };

    isp.send(on_isp_send, (void*)&isp_ctx);

    struct enc_context enc_ctx = { 0, outfilepath, &enc };

    isp.recv(on_isp_recv, (void*)&enc_ctx);

    return 0;
}

int jpeg_encoder_v4l_rpi_impl::deinit()
{
    if (!inited)
        return 0;

    isp.stream_off();
    enc.stream_off();

    isp.close();
    enc.close();

    width = 0;
    height = 0;
    ch = 0;
    quality = 0;

    inited = 0;

    return 0;
}

bool jpeg_encoder_v4l_rpi::supported(int width, int height, int ch)
{
    if (!is_device_whitelisted())
        return false;

    if (ch != 1 && ch != 3 && ch != 4)
        return false;

    if (width % 32 != 0 || height % 2 != 0)
        return false;

    if (width < 32 || height < 32)
        return false;

    if (width > 1920 || height > 1920)
        return false;

    return true;
}

jpeg_encoder_v4l_rpi::jpeg_encoder_v4l_rpi() : d(new jpeg_encoder_v4l_rpi_impl)
{
}

jpeg_encoder_v4l_rpi::~jpeg_encoder_v4l_rpi()
{
    delete d;
}

int jpeg_encoder_v4l_rpi::init(int width, int height, int ch, int quality)
{
    return d->init(width, height, ch, quality);
}

int jpeg_encoder_v4l_rpi::encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const
{
    return d->encode(bgrdata, outdata);
}

int jpeg_encoder_v4l_rpi::encode(const unsigned char* bgrdata, const char* outfilepath) const
{
    return d->encode(bgrdata, outfilepath);
}

int jpeg_encoder_v4l_rpi::deinit()
{
    return d->deinit();
}

} // namespace cv
