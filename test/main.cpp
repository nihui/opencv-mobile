#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#ifdef OPENCV_MOBILE_TEST_POINTER_SIZE
typedef char opencv_mobile_test_pointer_size[sizeof(void*) == OPENCV_MOBILE_TEST_POINTER_SIZE ? 1 : -1];
#endif

int main()
{
    cv::Mat bgr = cv::imread("in.jpg", 1);

    cv::resize(bgr, bgr, cv::Size(200, 200));

    cv::imwrite("out.jpg", bgr);

    return 0;
}
