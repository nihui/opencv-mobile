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

#ifndef OPENCV_HIGHGUI_HPP
#define OPENCV_HIGHGUI_HPP

#include "opencv2/core.hpp"

enum
{
    CV_LOAD_IMAGE_UNCHANGED     = -1,
    CV_LOAD_IMAGE_GRAYSCALE     = 0,
    CV_LOAD_IMAGE_COLOR         = 1,
};

enum
{
    CV_IMWRITE_JPEG_QUALITY     = 1
};

enum
{
    CV_CAP_PROP_FRAME_WIDTH     = 3,
    CV_CAP_PROP_FRAME_HEIGHT    = 4,
    CV_CAP_PROP_FPS             = 5,
};

namespace cv {

enum ImreadModes
{
    IMREAD_UNCHANGED            = -1,
    IMREAD_GRAYSCALE            = 0,
    IMREAD_COLOR                = 1
};

enum ImwriteFlags
{
    IMWRITE_JPEG_QUALITY        = 1
};

enum VideoCaptureProperties
{
    CAP_PROP_FRAME_WIDTH        = 3,
    CAP_PROP_FRAME_HEIGHT       = 4,
    CAP_PROP_FPS                = 5,
};

CV_EXPORTS_W Mat imread(const String& filename, int flags = IMREAD_COLOR);

CV_EXPORTS_W bool imwrite(const String& filename, InputArray img, const std::vector<int>& params = std::vector<int>());

CV_EXPORTS_W Mat imdecode(InputArray buf, int flags);

CV_EXPORTS_W bool imencode(const String& ext, InputArray img, CV_OUT std::vector<uchar>& buf, const std::vector<int>& params = std::vector<int>());

CV_EXPORTS_W void imshow(const String& winname, InputArray mat);

CV_EXPORTS_W int waitKey(int delay = 0);

class VideoCaptureImpl;
class CV_EXPORTS_W VideoCapture
{
public:
    VideoCapture();

    ~VideoCapture();

    bool open(int index);

    bool isOpened() const;

    void release();

    VideoCapture& operator>>(Mat& bgr_image);

    bool set(int propId, double value);

    double get(int propId) const;

private:
    VideoCaptureImpl* const d;
};

class VideoWriterImpl;
class CV_EXPORTS_W VideoWriter
{
public:
    VideoWriter();

    ~VideoWriter();

    bool open(const String& name, int port);

    bool isOpened() const;

    void release();

    VideoWriter& operator<<(const Mat& bgr_image);

    void write(const Mat& image);

protected:
    VideoWriterImpl* const d;
};

} // namespace cv

#endif // OPENCV_HIGHGUI_HPP
