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

#ifndef DISPLAY_FB_H
#define DISPLAY_FB_H

#include <vector>

namespace cv {

class display_fb_impl;
class display_fb
{
public:
    static bool supported();

    display_fb();
    ~display_fb();

    int open();

    int get_width() const;
    int get_height() const;

    int show_bgr(const unsigned char* bgrdata, int width, int height);
    int show_gray(const unsigned char* graydata, int width, int height);

    int close();

private:
    display_fb_impl* const d;
};

} // namespace cv

#endif // DISPLAY_FB_H
