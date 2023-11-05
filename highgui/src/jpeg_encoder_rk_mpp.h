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

#ifndef JPEG_ENCODER_RK_MPP_H
#define JPEG_ENCODER_RK_MPP_H

#include <vector>

class jpeg_encoder_rk_mpp_impl;
class jpeg_encoder_rk_mpp
{
public:
    static bool supported(int width, int height, int ch);

    jpeg_encoder_rk_mpp();
    ~jpeg_encoder_rk_mpp();

    int init(int width, int height, int ch, int quality);

    int encode(const unsigned char* bgrdata, std::vector<unsigned char>& outdata) const;

    int encode(const unsigned char* bgrdata, const char* outfilepath) const;

    int deinit();

private:
    jpeg_encoder_rk_mpp_impl* const d;
};

#endif // JPEG_ENCODER_RK_MPP_H
