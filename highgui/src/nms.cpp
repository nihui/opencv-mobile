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
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <limits>
#include <vector>
#include <algorithm>

namespace cv {
namespace dnn {

static inline bool SortScorePairDescend(const std::pair<float, int>& pair1, const std::pair<float, int>& pair2)
{
    return pair1.first > pair2.first;
}

// Get max scores with corresponding indices.
//    scores: a set of scores.
//    threshold: only consider scores higher than the threshold.
//    top_k: if -1, keep all; otherwise, keep at most top_k.
//    score_index_vec: store the sorted (score, index) pair.
inline void GetMaxScoreIndex(const std::vector<float>& scores, const float threshold, const int top_k,
                      std::vector<std::pair<float, int> >& score_index_vec)
{
    // Generate index score pairs.
    for (size_t i = 0; i < scores.size(); ++i)
    {
        if (scores[i] > threshold)
        {
            score_index_vec.push_back(std::make_pair(scores[i], i));
        }
    }

    // Sort the score pair according to the scores in descending order
    std::stable_sort(score_index_vec.begin(), score_index_vec.end(), SortScorePairDescend);

    // Keep top_k scores if needed.
    if (top_k > 0 && top_k < (int)score_index_vec.size())
    {
        score_index_vec.resize(top_k);
    }
}

// Do non maximum suppression given bboxes and scores.
// Inspired by Piotr Dollar's NMS implementation in EdgeBox.
// https://goo.gl/jV3JYS
//    bboxes: a set of bounding boxes.
//    scores: a set of corresponding confidences.
//    score_threshold: a threshold used to filter detection results.
//    nms_threshold: a threshold used in non maximum suppression.
//    top_k: if not > 0, keep at most top_k picked indices.
//    limit: early terminate once the # of picked indices has reached it.
//    indices: the kept indices of bboxes after nms.
template <typename BoxType>
inline void NMSFast_(const std::vector<BoxType>& bboxes,
      const std::vector<float>& scores, const float score_threshold,
      const float nms_threshold, const float eta, const int top_k,
      std::vector<int>& indices,
      float (*computeOverlap)(const BoxType&, const BoxType&),
      int limit = std::numeric_limits<int>::max())
{
    // Get top_k scores (with corresponding indices).
    std::vector<std::pair<float, int> > score_index_vec;
    GetMaxScoreIndex(scores, score_threshold, top_k, score_index_vec);

    // Do nms.
    float adaptive_threshold = nms_threshold;
    indices.clear();
    for (size_t i = 0; i < score_index_vec.size(); ++i)
    {
        const int idx = score_index_vec[i].second;
        bool keep = true;
        for (int k = 0; k < (int)indices.size() && keep; ++k)
        {
            const int kept_idx = indices[k];
            float overlap = computeOverlap(bboxes[idx], bboxes[kept_idx]);
            keep = overlap <= adaptive_threshold;
        }
        if (keep)
        {
            indices.push_back(idx);
            if ((int)indices.size() >= limit) {
                break;
            }
        }
        if (keep && eta < 1 && adaptive_threshold > 0.5) {
            adaptive_threshold *= eta;
        }
    }
}

static inline float rectOverlap(const Rect& a, const Rect& b)
{
    int Aa = a.area();
    int Ab = b.area();

    if (Aa + Ab == 0)
        return 0.f;

    int intersect = (a & b).area();

    return (float)intersect / (Aa + Ab - intersect);
}

void NMSBoxes(const std::vector<Rect>& bboxes, const std::vector<float>& scores,
                          const float score_threshold, const float nms_threshold,
                          std::vector<int>& indices, const float eta, const int top_k)
{
    NMSFast_(bboxes, scores, score_threshold, nms_threshold, eta, top_k, indices, rectOverlap);
}

static inline void NMSBoxesBatchedImpl(const std::vector<Rect>& bboxes,
                                       const std::vector<float>& scores, const std::vector<int>& class_ids,
                                       const float score_threshold, const float nms_threshold,
                                       std::vector<int>& indices, const float eta, const int top_k)
{
    int x1, y1, x2, y2, max_coord = 0;
    for (size_t i = 0; i < bboxes.size(); i++)
    {
        x1 = bboxes[i].x;
        y1 = bboxes[i].y;
        x2 = x1 + bboxes[i].width;
        y2 = y1 + bboxes[i].height;

        max_coord = std::max(x1, max_coord);
        max_coord = std::max(y1, max_coord);
        max_coord = std::max(x2, max_coord);
        max_coord = std::max(y2, max_coord);
    }

    // calculate offset and add offset to each bbox
    std::vector<Rect> bboxes_offset;
    for (size_t i = 0; i < bboxes.size(); i++)
    {
        int offset = class_ids[i] * (max_coord + 1);
        bboxes_offset.push_back(Rect(bboxes[i].x + offset, bboxes[i].y + offset, bboxes[i].width, bboxes[i].height));
    }

    NMSFast_(bboxes_offset, scores, score_threshold, nms_threshold, eta, top_k, indices, rectOverlap);
}

void NMSBoxesBatched(const std::vector<Rect>& bboxes,
                     const std::vector<float>& scores, const std::vector<int>& class_ids,
                     const float score_threshold, const float nms_threshold,
                     std::vector<int>& indices, const float eta, const int top_k)
{
    NMSBoxesBatchedImpl(bboxes, scores, class_ids, score_threshold, nms_threshold, indices, eta, top_k);
}

void softNMSBoxes(const std::vector<Rect>& bboxes,
                  const std::vector<float>& scores,
                  std::vector<float>& updated_scores,
                  const float score_threshold,
                  const float nms_threshold,
                  std::vector<int>& indices,
                  size_t top_k,
                  const float sigma,
                  SoftNMSMethod method)
{
    indices.clear();
    updated_scores.clear();

    std::vector<std::pair<float, size_t> > score_index_vec(scores.size());
    for (size_t i = 0; i < scores.size(); i++)
    {
        score_index_vec[i].first = scores[i];
        score_index_vec[i].second = i;
    }

    const auto score_cmp = [](const std::pair<float, size_t>& a, const std::pair<float, size_t>& b)
    {
        return a.first == b.first ? a.second > b.second : a.first < b.first;
    };

    top_k = top_k == 0 ? scores.size() : std::min(top_k, scores.size());
    ptrdiff_t start = 0;
    while (indices.size() < top_k)
    {
        auto it = std::max_element(score_index_vec.begin() + start, score_index_vec.end(), score_cmp);

        float bscore = it->first;
        size_t bidx = it->second;

        if (bscore < score_threshold)
        {
            break;
        }

        indices.push_back(static_cast<int>(bidx));
        updated_scores.push_back(bscore);
        std::swap(score_index_vec[start], *it); // first start elements are chosen

        for (size_t i = start + 1; i < scores.size(); ++i)
        {
            float& bscore_i = score_index_vec[i].first;
            const size_t bidx_i = score_index_vec[i].second;

            if (bscore_i < score_threshold)
            {
                continue;
            }

            float overlap = rectOverlap(bboxes[bidx], bboxes[bidx_i]);

            switch (method)
            {
                case SoftNMSMethod::SOFTNMS_LINEAR:
                    if (overlap > nms_threshold)
                    {
                        bscore_i *= 1.f - overlap;
                    }
                    break;
                case SoftNMSMethod::SOFTNMS_GAUSSIAN:
                    bscore_i *= exp(-(overlap * overlap) / sigma);
                    break;
                default:
                    CV_Error(CV_StsBadArg, "Not supported SoftNMS method");
            }
        }
        ++start;
    }
}

} // namespace dnn
} // namespace cv
