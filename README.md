# opencv-mobile

![release](https://github.com/nihui/opencv-mobile/workflows/release/badge.svg)

✔️ This project provides the minimal build of opencv library for the **android** and **ios** platforms.

✔️ We provide prebuild binary packages for opencv **2.4.13.7**, **3.4.13** and **4.5.1**.

✔️ We also provide prebuild binary package for **ios with bitcode enabled**, that the official package lacks.

✔️ All the binaries are compiled from source on github action, **no virus**, **no backdoor**, **no secret code**.

|opencv 4.5.1 android|package size|
|---|---|
|The official opencv|229MB|
|opencv-mobile|16.7MB|

|opencv 4.5.1 ios|package size|
|---|---|
|The official opencv|173MB|
|opencv-mobile|14.4MB|

|opencv 4.5.1 ios with bitcode|package size|
|---|---|
|The official opencv|missing :(|
|opencv-mobile|50.2MB|

# Download

## android

(armeabi-v7a arm64-v8a x86 x86_64) build with ndk r21c

* [opencv-mobile-2.4.13.7-android.zip (9.68MB)](https://github.com/nihui/opencv-mobile/releases/download/v1/opencv-mobile-2.4.13.7-android.zip)
* [opencv-mobile-3.4.13-android.zip (16.1MB)](https://github.com/nihui/opencv-mobile/releases/download/v1/opencv-mobile-3.4.13-android.zip)
* [opencv-mobile-4.5.1-android.zip (16.7MB)](https://github.com/nihui/opencv-mobile/releases/download/v1/opencv-mobile-4.5.1-android.zip)

## ios

(armv7 arm64 arm64e i386 x86_64) build with Xcode 12.2

* [opencv-mobile-2.4.13.7-ios.zip (9.46MB)](https://github.com/nihui/opencv-mobile/releases/download/v1/opencv-mobile-2.4.13.7-ios.zip)
* [opencv-mobile-3.4.13-ios.zip (13.9MB)](https://github.com/nihui/opencv-mobile/releases/download/v1/opencv-mobile-3.4.13-ios.zip)
* [opencv-mobile-4.5.1-ios.zip (14.4MB)](https://github.com/nihui/opencv-mobile/releases/download/v1/opencv-mobile-4.5.1-ios.zip)

## ios with bitcode

(armv7 arm64 arm64e i386 x86_64) build with Xcode 12.2

* [opencv-mobile-2.4.13.7-ios-bitcode.zip (33.7MB)](https://github.com/nihui/opencv-mobile/releases/download/v1/opencv-mobile-2.4.13.7-ios-bitcode.zip)
* [opencv-mobile-3.4.13-ios-bitcode.zip (48.0MB)](https://github.com/nihui/opencv-mobile/releases/download/v1/opencv-mobile-3.4.13-ios-bitcode.zip)
* [opencv-mobile-4.5.1-ios-bitcode.zip (50.2MB)](https://github.com/nihui/opencv-mobile/releases/download/v1/opencv-mobile-4.5.1-ios-bitcode.zip)

# Usage android

1. Extract archive to ```<project dir>/app/src/main/jni/```
2. Modify ```<project dir>/app/src/main/jni/CMakeListst.txt``` to find and link opencv

```cmake
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.5.1-android/sdk/native/jni)
find_package(OpenCV REQUIRED)

target_link_libraries(your_jni_target ${OpenCV_LIBS})
```

# Usage ios

1. Extract archive, and drag ```opencv2.framework``` into your project
2. In Build Phases -> Link Binary with Libraries, add ```libz.tbd```

# Some notes

* The minimal opencv build contains most basic opencv operators and common image processing functions, with some handy additions like keypoint feature extraction and matching, image inpainting and opticalflow estimation.

* Many computer vision algorithms that reside in dedicated modules are discarded, such as face detection etc. [You could try deep-learning based algorithms with nerual network inference library optimized for mobile.](https://github.com/Tencent/ncnn)

* cuda and opencl are disabled because there is no cuda on mobile, no opencl on ios, and opencl on android is slow. opencv on gpu is not suitable for real productions. Write metal on ios and opengles/vulkan on android if you need good gpu acceleration.

* C++ RTTI and exceptions are disabled for minimal build. Be careful when you write ```cv::Mat roi = image(roirect);```  :P

# opencv modules included

|module|comment|
|---|---|
|opencv_core|Mat, matrix operations, etc|
|opencv_imgproc|resize, cvtColor, warpAffine, etc|
|opencv_features2d|keypoint feature and matcher, etc (not included in opencv 2.x package)|
|opencv_photo|inpaint, etc|
|opencv_video|opticalflow, etc|

# opencv modules discarded

|module|comment|
|---|---|
|opencv_androidcamera|use android Camera api instead|
|opencv_calib3d|camera calibration, rare uses on mobile|
|opencv_contrib|experimental functions, build part of the source externally if you need|
|opencv_dnn|very slow on mobile, try ncnn for nerual network inference on mobile|
|opencv_dynamicuda|no cuda on mobile|
|opencv_flann|feature matching, rare uses on mobile, build the source externally if you need|
|opencv_gapi|graph based image processing, little gain on mobile|
|opencv_gpu|no cuda/opencl on mobile|
|opencv_highgui|use android Bitmap or ios UIImage api instead|
|opencv_imgcodecs|use android Bitmap or ios UIImage api instead|
|opencv_java|wrap your c++ code with jni|
|opencv_js|write native code on mobile|
|opencv_lagacy|various good-old cv routines, build part of the source externally if you need|
|opencv_ml|train your ML algorithm on powerful pc or server|
|opencv_nonfree|the SIFT and SURF, use ORB which is faster and better|
|opencv_objdetect|HOG, cascade detector, use deep learning detector which is faster and better|
|opencv_ocl|no opencl on mobile|
|opencv_python|no python on mobile|
|opencv_shape|shape matching, rare uses on mobile, build the source externally if you need|
|opencv_stitching|image stitching, rare uses on mobile, build the source externally if you need|
|opencv_superres|do video super-resolution on powerful pc or server|
|opencv_ts|test modules, useless in production anyway|
|opencv_videoio|use android MediaCodec or ios AVFoundation api instead|
|opencv_videostab|do video stablization on powerful pc or server|
|opencv_viz|vtk is not available on mobile, write your own data visualization routines|


