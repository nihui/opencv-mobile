#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

static void my_sleep(unsigned long long int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#elif defined(__unix__) || defined(__APPLE__)
    usleep(milliseconds * 1000);
#elif _POSIX_TIMERS
    struct timespec ts;
    ts.tv_sec = milliseconds * 0.001;
    ts.tv_nsec = 0;
    nanosleep(&ts, &ts);
#else
    // TODO How to handle it ?
#endif
}

static void test_image()
{
    cv::Mat bgr = cv::imread("in.jpg", 1);

    cv::resize(bgr, bgr, cv::Size(200, 200));

    cv::imwrite("out.jpg", bgr);
}

static void test_camera()
{
    cv::VideoCapture cap;
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 320);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 240);
    cap.open(0);

    const int w = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    const int h = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    fprintf(stderr, "%d x %d\n", w, h);

    cv::Mat bgr[9];
    for (int i = 0; i < 9; i++)
    {
        cap >> bgr[i];

        my_sleep(1000);
    }

    cap.release();

    // combine into big image
    {
        cv::Mat out(h * 3, w * 3, CV_8UC3);
        bgr[0].copyTo(out(cv::Rect(0, 0, w, h)));
        bgr[1].copyTo(out(cv::Rect(w, 0, w, h)));
        bgr[2].copyTo(out(cv::Rect(w * 2, 0, w, h)));
        bgr[3].copyTo(out(cv::Rect(0, h, w, h)));
        bgr[4].copyTo(out(cv::Rect(w, h, w, h)));
        bgr[5].copyTo(out(cv::Rect(w * 2, h, w, h)));
        bgr[6].copyTo(out(cv::Rect(0, h * 2, w, h)));
        bgr[7].copyTo(out(cv::Rect(w, h * 2, w, h)));
        bgr[8].copyTo(out(cv::Rect(w * 2, h * 2, w, h)));

        cv::imwrite("out.jpg", out);
    }
}

static void test_display()
{
    cv::Mat bgr = cv::imread("in.jpg", 1);

    cv::imshow("fb", bgr);
}

static void test_httpjpg()
{
    cv::VideoCapture cap;
    cap.open(0);

    cv::VideoWriter http;
    http.open("httpjpg", 7766);

    // open streaming url http://<server ip>:7766 in web browser

    cv::Mat bgr;
    while (1)
    {
        cap >> bgr;
        http << bgr;
    }
}

int main(int argc, char** argv)
{
    int test_mode = 0;
    if (argc == 2)
    {
        test_mode = atoi(argv[1]);
    }

    if (test_mode < 0 || test_mode > 3)
    {
        fprintf(stderr, "usage: %s [0/1/2/3]\n", argv[0]);
        fprintf(stderr, "0 = test_image\n");
        fprintf(stderr, "1 = test_camera\n");
        fprintf(stderr, "2 = test_display\n");
        fprintf(stderr, "3 = test_httpjpg\n");
        return -1;
    }

    if (test_mode == 0) test_image();
    if (test_mode == 1) test_camera();
    if (test_mode == 2) test_display();
    if (test_mode == 3) test_httpjpg();

    return 0;
}
