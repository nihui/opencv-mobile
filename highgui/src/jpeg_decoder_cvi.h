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

#ifndef JPEG_DECODER_CVI_H
#define JPEG_DECODER_CVI_H

#include <vector>

class jpeg_decoder_cvi_impl;
class jpeg_decoder_cvi
{
public:
    static bool supported(const unsigned char* jpgdata, size_t jpgsize);

    jpeg_decoder_cvi();
    ~jpeg_decoder_cvi();

    int init(const unsigned char* jpgdata, size_t jpgsize, int* width, int* height, int* ch);

    int decode(const unsigned char* jpgdata, size_t jpgsize, unsigned char* outbgr) const;

    int deinit();

private:
    jpeg_decoder_cvi_impl* const d;
};

#endif // JPEG_DECODER_CVI_H
