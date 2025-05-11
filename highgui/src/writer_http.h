//
// Copyright (C) 2025 nihui&futz12
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

#ifndef WRITER_HTTP_H
#define WRITER_HTTP_H

#include <vector>

namespace cv {

class writer_http_impl;
class writer_http
{
public:
    static bool supported();

    writer_http();
    ~writer_http();

    int open(int port);

    void write_jpgbuf(const std::vector<unsigned char>& jpgbuf);

    void close();

private:
    writer_http_impl* const d;
};

} // namespace cv

#endif // WRITER_HTTP_H
