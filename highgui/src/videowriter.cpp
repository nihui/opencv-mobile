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

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

#include <stdio.h>
#include <string.h>

#if defined __linux__ && !__ANDROID__
#include "writer_http.h"
#endif

namespace cv {

class VideoWriterImpl
{
public:
    VideoWriterImpl();

public:
    bool is_opened;
    int width;
    int height;
    float fps;

#if defined __linux__ && !__ANDROID__
    writer_http wt_http;
#endif
};

VideoWriterImpl::VideoWriterImpl()
{
    is_opened = false;
    width = 0;
    height = 0;
    fps = 0;
}

VideoWriter::VideoWriter() : d(new VideoWriterImpl)
{
}

VideoWriter::~VideoWriter()
{
    release();

    delete d;
}

bool VideoWriter::open(const String& name, int port)
{
    if (d->is_opened)
    {
        release();
    }

    if (name == "httpjpg")
    {
#if defined __linux__ && !__ANDROID__
        if (writer_http::supported())
        {
            int ret = d->wt_http.open(port);
            if (ret == 0)
            {
                d->is_opened = true;
            }
        }
        else
#endif
        {
        }
    }

    return d->is_opened;
}

bool VideoWriter::isOpened() const
{
    return d->is_opened;
}

void VideoWriter::release()
{
    if (!d->is_opened)
        return;

#if defined __linux__ && !__ANDROID__
    if (writer_http::supported())
    {
        d->wt_http.close();
    }
    else
#endif
    {
    }

    d->is_opened = false;
    d->width = 0;
    d->height = 0;
    d->fps = 0;
}

VideoWriter& VideoWriter::operator<<(const Mat& image)
{
    write(image);

    return *this;
}

void VideoWriter::write(const Mat& image)
{
    if (!d->is_opened)
        return;

#if defined __linux__ && !__ANDROID__
    if (writer_http::supported())
    {
        // encode jpg
        std::vector<uchar> buf;
        cv::imencode(".JPG", image, buf);

        d->wt_http.write_jpgbuf(buf);
    }
    else
#endif
    {
    }
}

} // namespace cv
