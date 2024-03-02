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

#ifndef CAPTURE_V4L2_AW_ISP_H
#define CAPTURE_V4L2_AW_ISP_H

#include <vector>

namespace cv {

class capture_v4l2_aw_isp_impl;
class capture_v4l2_aw_isp
{
public:
    static bool supported();

    capture_v4l2_aw_isp();
    ~capture_v4l2_aw_isp();

    int open(int width = 640, int height = 480, float fps = 30);

    int get_width() const;
    int get_height() const;
    float get_fps() const;

    int start_streaming();

    int read_frame(unsigned char* bgrdata);

    int stop_streaming();

    int close();

private:
    capture_v4l2_aw_isp_impl* const d;
};

} // namespace cv

#endif // CAPTURE_V4L2_AW_ISP_H
