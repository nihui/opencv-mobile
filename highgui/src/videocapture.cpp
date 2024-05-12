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

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

#include <stdio.h>
#include <string.h>

#if defined __linux__
#include "capture_cvi.h"
#include "capture_v4l2_aw_isp.h"
#include "capture_v4l2_rk_aiq.h"
#include "capture_v4l2.h"
#endif

namespace cv {

class VideoCaptureImpl
{
public:
    VideoCaptureImpl();

public:
    bool is_opened;
    int width;
    int height;
    float fps;

#if defined __linux__
    capture_v4l2_aw_isp cap_v4l2_aw_isp;
    capture_v4l2_rk_aiq cap_v4l2_rk_aiq;
    capture_cvi cap_cvi;
    capture_v4l2 cap_v4l2;
#endif
};

VideoCaptureImpl::VideoCaptureImpl()
{
    is_opened = false;
    width = 640;
    height = 480;
    fps = 30;
}

VideoCapture::VideoCapture() : d(new VideoCaptureImpl)
{
}

VideoCapture::~VideoCapture()
{
    release();

    delete d;
}

bool VideoCapture::open(int index)
{
    if (d->is_opened)
    {
        release();
    }

#if defined __linux__
    if (capture_v4l2_aw_isp::supported())
    {
        int ret = d->cap_v4l2_aw_isp.open(d->width, d->height, d->fps);
        if (ret == 0)
        {
            d->width = d->cap_v4l2_aw_isp.get_width();
            d->height = d->cap_v4l2_aw_isp.get_height();
            d->fps = d->cap_v4l2_aw_isp.get_fps();

            ret = d->cap_v4l2_aw_isp.start_streaming();
            if (ret == 0)
            {
                d->is_opened = true;
            }
            else
            {
                d->cap_v4l2_aw_isp.close();
            }
        }
    }

    if (capture_v4l2_rk_aiq::supported())
    {
        int ret = d->cap_v4l2_rk_aiq.open(d->width, d->height, d->fps);
        if (ret == 0)
        {
            d->width = d->cap_v4l2_rk_aiq.get_width();
            d->height = d->cap_v4l2_rk_aiq.get_height();
            d->fps = d->cap_v4l2_rk_aiq.get_fps();

            ret = d->cap_v4l2_rk_aiq.start_streaming();
            if (ret == 0)
            {
                d->is_opened = true;
            }
            else
            {
                d->cap_v4l2_rk_aiq.close();
            }
        }
    }

    if (capture_cvi::supported())
    {
        int ret = d->cap_cvi.open(d->width, d->height, d->fps);
        if (ret == 0)
        {
            d->width = d->cap_cvi.get_width();
            d->height = d->cap_cvi.get_height();
            d->fps = d->cap_cvi.get_fps();

            ret = d->cap_cvi.start_streaming();
            if (ret == 0)
            {
                d->is_opened = true;
            }
            else
            {
                d->cap_cvi.close();
            }
        }
    }

    if (capture_v4l2::supported())
    {
        int ret = d->cap_v4l2.open(d->width, d->height, d->fps);
        if (ret == 0)
        {
            d->width = d->cap_v4l2.get_width();
            d->height = d->cap_v4l2.get_height();
            d->fps = d->cap_v4l2.get_fps();

            ret = d->cap_v4l2.start_streaming();
            if (ret == 0)
            {
                d->is_opened = true;
            }
            else
            {
                d->cap_v4l2.close();
            }
        }
    }
#endif

    return d->is_opened;
}

bool VideoCapture::isOpened() const
{
    return d->is_opened;
}

void VideoCapture::release()
{
    if (!d->is_opened)
        return;

#if defined __linux__
    if (capture_v4l2_aw_isp::supported())
    {
        d->cap_v4l2_aw_isp.stop_streaming();

        d->cap_v4l2_aw_isp.close();
    }

    if (capture_v4l2_rk_aiq::supported())
    {
        d->cap_v4l2_rk_aiq.stop_streaming();

        d->cap_v4l2_rk_aiq.close();
    }

    if (capture_cvi::supported())
    {
        d->cap_cvi.stop_streaming();

        d->cap_cvi.close();
    }

    if (capture_v4l2::supported())
    {
        d->cap_v4l2.stop_streaming();

        d->cap_v4l2.close();
    }
#endif

    d->is_opened = false;
    d->width = 640;
    d->height = 480;
    d->fps = 30;
}

VideoCapture& VideoCapture::operator>>(Mat& image)
{
    if (!d->is_opened)
        return *this;

#if defined __linux__
    if (capture_v4l2_aw_isp::supported())
    {
        image.create(d->height, d->width, CV_8UC3);

        d->cap_v4l2_aw_isp.read_frame((unsigned char*)image.data);
    }

    if (capture_v4l2_rk_aiq::supported())
    {
        image.create(d->height, d->width, CV_8UC3);

        d->cap_v4l2_rk_aiq.read_frame((unsigned char*)image.data);
    }

    if (capture_cvi::supported())
    {
        image.create(d->height, d->width, CV_8UC3);

        d->cap_cvi.read_frame((unsigned char*)image.data);
    }

    if (capture_v4l2::supported())
    {
        image.create(d->height, d->width, CV_8UC3);

        d->cap_v4l2.read_frame((unsigned char*)image.data);
    }
#endif

    return *this;
}

bool VideoCapture::set(int propId, double value)
{
    if (propId == CAP_PROP_FRAME_WIDTH)
    {
        d->width = (int)value;
        return true;
    }

    if (propId == CAP_PROP_FRAME_HEIGHT)
    {
        d->height = (int)value;
        return true;
    }

    if (propId == CAP_PROP_FPS)
    {
        d->fps = (float)value;
        return true;
    }

    fprintf(stderr, "ignore unsupported cv cap propId %d = %f\n", propId, value);
    return true;
}

double VideoCapture::get(int propId) const
{
    if (propId == CAP_PROP_FRAME_WIDTH)
    {
        return (double)d->width;
    }

    if (propId == CAP_PROP_FRAME_HEIGHT)
    {
        return (double)d->height;
    }

    if (propId == CAP_PROP_FPS)
    {
        return (double)d->fps;
    }

    fprintf(stderr, "ignore unsupported cv cap propId %d\n", propId);
    return 0.0;
}

} // namespace cv
