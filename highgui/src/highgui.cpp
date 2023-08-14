//
// Copyright (C) 2021 nihui
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
#include <opencv2/imgproc.hpp>
#include <string.h>
#include <fstream>
#include <sstream>
#include "exif.hpp"

#define STB_IMAGE_IMPLEMENTATION
#if __ARM_NEON
#define STBI_NEON
#endif
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_PNM
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace cv {
//
//     1        2       3      4         5            6           7          8
//
//   888888  888888      88  88      8888888888  88                  88  8888888888
//   88          88      88  88      88  88      88  88          88  88      88  88
//   8888      8888    8888  8888    88          8888888888  8888888888          88
//   88          88      88  88
//   88          88  888888  888888
//
// ref http://sylvana.net/jpegcrop/exif_orientation.html
static void rotate_by_orientation(const Mat& src, Mat& dst, int orientation)
{
    if (orientation == 1)
    {
        dst = src;
    }
    if (orientation == 2)
    {
        cv::flip(src, dst, 1);
    }
    if (orientation == 3)
    {
        cv::flip(src, dst, -1);
    }
    if (orientation == 4)
    {
        cv::flip(src, dst, 0);
    }
    if (orientation == 5)
    {
        cv::transpose(src, dst);
    }
    if (orientation == 6)
    {
        Mat tmp;
        cv::flip(src, tmp, 0);
        cv::transpose(tmp, dst);
    }
    if (orientation == 7)
    {
        Mat tmp;
        cv::flip(src, tmp, -1);
        cv::transpose(tmp, dst);
    }
    if (orientation == 8)
    {
        Mat tmp;
        cv::flip(src, tmp, 1);
        cv::transpose(tmp, dst);
    }
}

Mat imread(const String& filename, int flags)
{
    int desired_channels = 0;
    if (flags == IMREAD_UNCHANGED)
    {
        desired_channels = 0;
    }
    else if (flags == IMREAD_GRAYSCALE)
    {
        desired_channels = 1;
    }
    else if (flags == IMREAD_COLOR)
    {
        desired_channels = 3;
    }
    else
    {
        // unknown flags
        return Mat();
    }

    int w;
    int h;
    int c;
    unsigned char* pixeldata = stbi_load(filename.c_str(), &w, &h, &c, desired_channels);
    if (!pixeldata)
    {
        // load failed
        return Mat();
    }

    if (desired_channels)
    {
        c = desired_channels;
    }

    // copy pixeldata to Mat
    Mat img;
    if (c == 1)
    {
        img.create(h, w, CV_8UC1);
    }
    else if (c == 3)
    {
        img.create(h, w, CV_8UC3);
    }
    else if (c == 4)
    {
        img.create(h, w, CV_8UC4);
    }
    else
    {
        // unexpected channels
        stbi_image_free(pixeldata);
        return Mat();
    }

    memcpy(img.data, pixeldata, w * h * c);

    stbi_image_free(pixeldata);

    // resolve exif orientation
    {
        std::ifstream ifs;
        ifs.open(filename.c_str(), std::ifstream::in);

        if (ifs.good())
        {
            ExifReader exif_reader(ifs);
            if (exif_reader.parse())
            {
                ExifEntry_t e = exif_reader.getTag(ORIENTATION);
                int orientation = e.field_u16;
                if (orientation >= 1 && orientation <= 8)
                    rotate_by_orientation(img, img, orientation);
            }
        }

        ifs.close();
    }

    // rgb to bgr
    if (c == 3)
    {
        cvtColor(img, img, COLOR_RGB2BGR);
    }
    if (c == 4)
    {
        cvtColor(img, img, COLOR_RGBA2BGRA);
    }

    return img;
}

bool imwrite(const String& filename, InputArray _img, const std::vector<int>& params)
{
    const char* _ext = strrchr(filename.c_str(), '.');
    if (!_ext)
    {
        // missing extension
        return false;
    }

    String ext = _ext;
    Mat img = _img.getMat();

    // bgr to rgb
    int c = 0;
    if (img.type() == CV_8UC1)
    {
        c = 1;
    }
    else if (img.type() == CV_8UC3)
    {
        c = 3;
        Mat img2;
        cvtColor(img, img2, COLOR_BGR2RGB);
        img = img2;
    }
    else if (img.type() == CV_8UC4)
    {
        c = 4;
        Mat img2;
        cvtColor(img, img2, COLOR_BGRA2RGBA);
        img = img2;
    }
    else
    {
        // unexpected image channels
        return false;
    }

    if (!img.isContinuous())
    {
        img = img.clone();
    }

    bool success = false;

    if (ext == ".jpg" || ext == ".jpeg" || ext == ".JPG" || ext == ".JPEG")
    {
        int quality = 95;
        for (size_t i = 0; i < params.size(); i += 2)
        {
            if (params[i] == IMWRITE_JPEG_QUALITY)
            {
                quality = params[i + 1];
                break;
            }
        }
        success = stbi_write_jpg(filename.c_str(), img.cols, img.rows, c, img.data, quality);
    }
    else if (ext == ".png" || ext == ".PNG")
    {
        success = stbi_write_png(filename.c_str(), img.cols, img.rows, c, img.data, 0);
    }
    else if (ext == ".bmp" || ext == ".BMP")
    {
        success = stbi_write_bmp(filename.c_str(), img.cols, img.rows, c, img.data);
    }
    else
    {
        // unknown extension type
        return false;
    }

    return success;
}

Mat imdecode(InputArray _buf, int flags)
{
    int desired_channels = 0;
    if (flags == IMREAD_UNCHANGED)
    {
        desired_channels = 0;
    }
    else if (flags == IMREAD_GRAYSCALE)
    {
        desired_channels = 1;
    }
    else if (flags == IMREAD_COLOR)
    {
        desired_channels = 3;
    }
    else
    {
        // unknown flags
        return Mat();
    }

    Mat buf = _buf.getMat();

    if (!buf.isContinuous())
    {
        buf = buf.clone();
    }

    size_t buf_size = buf.cols * buf.rows * buf.elemSize();

    int w;
    int h;
    int c;
    unsigned char* pixeldata = stbi_load_from_memory((const unsigned char*)buf.data, buf_size, &w, &h, &c, desired_channels);
    if (!pixeldata)
    {
        // load failed
        return Mat();
    }

    if (desired_channels)
    {
        c = desired_channels;
    }

    // copy pixeldata to Mat
    Mat img;
    if (c == 1)
    {
        img.create(h, w, CV_8UC1);
    }
    else if (c == 3)
    {
        img.create(h, w, CV_8UC3);
    }
    else if (c == 4)
    {
        img.create(h, w, CV_8UC4);
    }
    else
    {
        // unexpected channels
        stbi_image_free(pixeldata);
        return Mat();
    }

    memcpy(img.data, pixeldata, w * h * c);

    stbi_image_free(pixeldata);

    // resolve exif orientation
    {
        std::string s((const char*)buf.data, buf_size);
        std::istringstream iss(s);

        ExifReader exif_reader(iss);
        if (exif_reader.parse())
        {
            ExifEntry_t e = exif_reader.getTag(ORIENTATION);
            int orientation = e.field_u16;
            if (orientation >= 1 && orientation <= 8)
                rotate_by_orientation(img, img, orientation);
        }
    }

    // rgb to bgr
    if (c == 3)
    {
        cvtColor(img, img, COLOR_RGB2BGR);
    }
    if (c == 4)
    {
        cvtColor(img, img, COLOR_RGBA2BGRA);
    }

    return img;
}

static void imencode_write_func(void *context, void *data, int size)
{
    std::vector<uchar>* buf = (std::vector<uchar>*)context;
    buf->insert(buf->end(), (uchar*)data, (uchar*)data + size);
}

bool imencode(const String& ext, InputArray _img, std::vector<uchar>& buf, const std::vector<int>& params)
{
    Mat img = _img.getMat();

    // bgr to rgb
    int c = 0;
    if (img.type() == CV_8UC1)
    {
        c = 1;
    }
    else if (img.type() == CV_8UC3)
    {
        c = 3;
        Mat img2;
        cvtColor(img, img2, COLOR_BGR2RGB);
        img = img2;
    }
    else if (img.type() == CV_8UC4)
    {
        c = 4;
        Mat img2;
        cvtColor(img, img2, COLOR_BGRA2RGBA);
        img = img2;
    }
    else
    {
        // unexpected image channels
        return false;
    }

    if (!img.isContinuous())
    {
        img = img.clone();
    }

    bool success = false;

    if (ext == ".jpg" || ext == ".jpeg" || ext == ".JPG" || ext == ".JPEG")
    {
        int quality = 95;
        for (size_t i = 0; i < params.size(); i += 2)
        {
            if (params[i] == IMWRITE_JPEG_QUALITY)
            {
                quality = params[i + 1];
                break;
            }
        }
        success = stbi_write_jpg_to_func(imencode_write_func, (void*)&buf, img.cols, img.rows, c, img.data, quality);
    }
    else if (ext == ".png" || ext == ".PNG")
    {
        success = stbi_write_png_to_func(imencode_write_func, (void*)&buf, img.cols, img.rows, c, img.data, 0);
    }
    else if (ext == ".bmp" || ext == ".BMP")
    {
        success = stbi_write_bmp_to_func(imencode_write_func, (void*)&buf, img.cols, img.rows, c, img.data);
    }
    else
    {
        // unknown extension type
        return false;
    }

    return success;
}

void imshow(const String& winname, InputArray mat)
{
    fprintf(stderr, "imshow save image to %s.png", winname.c_str());
    imwrite(winname + ".png", mat);
}

int waitKey(int delay)
{
    (void)delay;
    fprintf(stderr, "waitKey stub");
    return -1;
}

} // namespace cv
