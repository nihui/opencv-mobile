#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <cstdio>

static int test_encode(const char* ext, const cv::Mat& img, std::vector<uchar>& buf)
{
    std::vector<uchar> expected;
    if (!cv::imencode(ext, img, expected) || !cv::imencode(ext, img, buf))
    {
        fprintf(stderr, "%s: encode failed\n", ext);
        return 1;
    }
    if (buf != expected)
    {
        fprintf(stderr, "%s: reused buffer differs from fresh buffer\n", ext);
        return 1;
    }

    cv::Mat decoded = cv::imdecode(buf, cv::IMREAD_UNCHANGED);
    cv::Mat reference = cv::imdecode(expected, cv::IMREAD_UNCHANGED);
    if (decoded.empty() || reference.empty() || decoded.size() != img.size() ||
        decoded.type() != reference.type() || cv::norm(decoded, reference, cv::NORM_INF) != 0)
    {
        fprintf(stderr, "%s: decoded reused buffer differs from current image\n", ext);
        return 1;
    }
    return 0;
}

int main()
{
    const char* extensions[] = {".jpg", ".jpeg", ".JPG", ".JPEG", ".png", ".PNG", ".bmp", ".BMP"};
    const int types[] = {CV_8UC1, CV_8UC3, CV_8UC4};
    std::vector<uchar> buf(37, 0xa5);

    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++)
    {
        for (size_t j = 0; j < sizeof(types) / sizeof(types[0]); j++)
        {
            cv::Mat first(19, 31, types[j], cv::Scalar(17, 43, 91, 255));
            cv::Mat second(27, 39, types[j], cv::Scalar(203, 111, 59, 127));
            cv::Mat roi = second(cv::Rect(2, 3, 11, 7));

            // Reuse one buffer across images, sizes, channels and formats.
            if (test_encode(extensions[i], first, buf) ||
                test_encode(extensions[i], roi, buf) ||
                test_encode(extensions[i], first, buf))
                return 1;
        }
    }

    const std::vector<uchar> saved = buf;
    cv::Mat valid(9, 13, CV_8UC3, cv::Scalar(31, 67, 127));
    cv::Mat invalid(9, 13, CV_32FC1, cv::Scalar(1));
    if (cv::imencode(".unsupported", valid, buf) || buf != saved ||
        cv::imencode(".png", invalid, buf) || buf != saved)
    {
        fprintf(stderr, "invalid arguments should fail without changing the buffer\n");
        return 1;
    }

    // A rejected JPEG write must not leave the previous image in the buffer.
    if (cv::imencode(".jpg", cv::Mat(), buf) || !buf.empty())
    {
        fprintf(stderr, "failed JPEG write left stale data\n");
        return 1;
    }
    if (test_encode(".png", valid, buf))
        return 1;

    return 0;
}
