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

#ifndef JPEG_ENCODER_V4L_RPI_H
#define JPEG_ENCODER_V4L_RPI_H

#include <vector>

namespace cv {

class jpeg_encoder_v4l_rpi_impl;
class jpeg_encoder_v4l_rpi
{
public:
    static bool supported(int width, int height, int ch);

    jpeg_encoder_v4l_rpi();
    ~jpeg_encoder_v4l_rpi();

    int init(int width, int height, int ch, int quality);

    int encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const;

    int encode(const unsigned char* bgrdata, const char* outfilepath) const;

    int deinit();

private:
    jpeg_encoder_v4l_rpi_impl* const d;
};

} // namespace cv

#endif // JPEG_ENCODER_V4L_RPI_H
