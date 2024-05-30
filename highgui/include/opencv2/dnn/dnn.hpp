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

#ifndef OPENCV_DNN_HPP
#define OPENCV_DNN_HPP

#include "opencv2/core.hpp"

namespace cv {
namespace dnn {

enum SoftNMSMethod
{
    SOFTNMS_LINEAR = 1,
    SOFTNMS_GAUSSIAN = 2
};

CV_EXPORTS void NMSBoxes(const std::vector<Rect>& bboxes, const std::vector<float>& scores,
                         const float score_threshold, const float nms_threshold,
                         CV_OUT std::vector<int>& indices,
                         const float eta = 1.f, const int top_k = 0);

CV_EXPORTS void NMSBoxesBatched(const std::vector<Rect>& bboxes, const std::vector<float>& scores, const std::vector<int>& class_ids,
                                const float score_threshold, const float nms_threshold,
                                CV_OUT std::vector<int>& indices,
                                const float eta = 1.f, const int top_k = 0);

CV_EXPORTS_W void softNMSBoxes(const std::vector<Rect>& bboxes, const std::vector<float>& scores,
                               CV_OUT std::vector<float>& updated_scores,
                               const float score_threshold, const float nms_threshold,
                               CV_OUT std::vector<int>& indices,
                               size_t top_k = 0, const float sigma = 0.5, SoftNMSMethod method = SOFTNMS_GAUSSIAN);

} // namespace dnn
} // namespace cv

#endif // OPENCV_DNN_HPP
