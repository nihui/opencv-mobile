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

#if CV_WITH_CVI
#include "capture_cvi.h"
#endif
#if CV_WITH_AW
#include "capture_v4l2_aw_isp.h"
#endif
#if CV_WITH_RK
#include "capture_v4l2_rk_aiq.h"
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

#if CV_WITH_AW
    capture_v4l2_aw_isp cap_v4l2_aw_isp;
#endif
#if CV_WITH_RK
    capture_v4l2_rk_aiq cap_v4l2_rk_aiq;
#endif
#if CV_WITH_CVI
    capture_cvi cap_cvi;
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

#if CV_WITH_AW
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
#endif
#if CV_WITH_RK
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
#endif
#if CV_WITH_CVI
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

#if CV_WITH_AW
    if (capture_v4l2_aw_isp::supported())
    {
        d->cap_v4l2_aw_isp.stop_streaming();

        d->cap_v4l2_aw_isp.close();
    }
#endif
#if CV_WITH_RK
    if (capture_v4l2_rk_aiq::supported())
    {
        d->cap_v4l2_rk_aiq.stop_streaming();

        d->cap_v4l2_rk_aiq.close();
    }
#endif
#if CV_WITH_CVI
    if (capture_cvi::supported())
    {
        d->cap_cvi.stop_streaming();

        d->cap_cvi.close();
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

#if CV_WITH_AW
    if (capture_v4l2_aw_isp::supported())
    {
        image.create(d->height, d->width, CV_8UC3);

        d->cap_v4l2_aw_isp.read_frame((unsigned char*)image.data);
    }
#endif
#if CV_WITH_RK
    if (capture_v4l2_rk_aiq::supported())
    {
        image.create(d->height, d->width, CV_8UC3);

        d->cap_v4l2_rk_aiq.read_frame((unsigned char*)image.data);
    }
#endif
#if CV_WITH_CVI
    if (capture_cvi::supported())
    {
        image.create(d->height, d->width, CV_8UC3);

        d->cap_cvi.read_frame((unsigned char*)image.data);
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
