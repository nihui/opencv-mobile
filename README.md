# opencv-mobile

![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)
![release](https://github.com/nihui/opencv-mobile/workflows/release/badge.svg)
![download](https://img.shields.io/github/downloads/nihui/opencv-mobile/total.svg)

✔️ This project provides the minimal build of opencv library for the **Android**, **iOS** and **ARM Linux** platforms.

✔️ We provide prebuild binary packages for opencv **2.4.13.7**, **3.4.13** and **4.5.1**.

✔️ We also provide prebuild binary package for **iOS with bitcode enabled**, that the official package lacks.

✔️ All the binaries are compiled from source on github action, **no virus**, **no backdoor**, **no secret code**.

|opencv 4.5.1 android|package size|
|---|---|
|The official opencv|229MB|
|opencv-mobile|15.5MB|

|opencv 4.5.1 ios|package size|
|---|---|
|The official opencv|173MB|
|opencv-mobile|14.8MB|

|opencv 4.5.1 ios with bitcode|package size|
|---|---|
|The official opencv|missing :(|
|opencv-mobile|51.6MB|

# Download

## Android

(armeabi-v7a arm64-v8a x86 x86_64) build with ndk r21d and android api 24

* [opencv-mobile-2.4.13.7-android.zip (7.77MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-2.4.13.7-android.zip)
* [opencv-mobile-3.4.13-android.zip (14.9MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-3.4.13-android.zip)
* [opencv-mobile-4.5.1-android.zip (15.5MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-4.5.1-android.zip)

## iOS

(armv7 arm64 arm64e i386 x86_64) build with Xcode 12.2

* [opencv-mobile-2.4.13.7-ios.zip (9.81MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-2.4.13.7-ios.zip)
* [opencv-mobile-3.4.13-ios.zip (14.3MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-3.4.13-ios.zip)
* [opencv-mobile-4.5.1-ios.zip (14.8MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-4.5.1-ios.zip)

## iOS with bitcode

(armv7 arm64 arm64e i386 x86_64) build with Xcode 12.2

* [opencv-mobile-2.4.13.7-ios-bitcode.zip (35.2MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-2.4.13.7-ios-bitcode.zip)
* [opencv-mobile-3.4.13-ios-bitcode.zip (49.4MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-3.4.13-ios-bitcode.zip)
* [opencv-mobile-4.5.1-ios-bitcode.zip (51.6MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-4.5.1-ios-bitcode.zip)

## ARM Linux

(arm-linux-gnueabi arm-linux-gnueabihf aarch64-linux-gnu) build with ubuntu cross compiler

* [opencv-mobile-2.4.13.7-armlinux.zip (7.97MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-2.4.13.7-armlinux.zip)
* [opencv-mobile-3.4.13-armlinux.zip (14.1MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-3.4.13-armlinux.zip)
* [opencv-mobile-4.5.1-armlinux.zip (14.6MB)](https://github.com/nihui/opencv-mobile/releases/download/v5/opencv-mobile-4.5.1-armlinux.zip)

# Usage Android

1. Extract archive to ```<project dir>/app/src/main/jni/```
2. Modify ```<project dir>/app/src/main/jni/CMakeListst.txt``` to find and link opencv

```cmake
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.5.1-android/sdk/native/jni)
find_package(OpenCV REQUIRED)

target_link_libraries(your_jni_target ${OpenCV_LIBS})
```

# Usage iOS

1. Extract archive, and drag ```opencv2.framework``` into your project

# Usage ARM Linux

1. Extract archive to ```<project dir>/```
2. Modify ```<project dir>/CMakeListst.txt``` to find and link opencv

```cmake
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.5.1-armlinux/arm-linux-gnueabihf/lib/cmake/opencv4)
find_package(OpenCV REQUIRED)

target_link_libraries(your_target ${OpenCV_LIBS})
```

# How-to-build your custom package

## step1 download opencv source
```shell
wget -q https://github.com/opencv/opencv/archive/4.5.1.zip -O opencv-4.5.1.zip
unzip -q opencv-4.5.1.zip
cd opencv-4.5.1
```

## step2 strip zlib dependency and use stb-based highgui implementation (optional)
```shell
truncate -s 0 cmake/OpenCVFindLibsGrfmt.cmake
rm -rf modules/gapi
rm -rf modules/highgui
cp -r ../highgui modules/
```

## step3 patch opencv source for no-rtti build (optional)
```shell
patch -p1 -i ../opencv-4.5.1-no-rtti.patch
```

## step4 apply your opencv options to opencv4_cmake_options.txt

## step5 build your opencv package with cmake
```shell
mkdir -p build
cd build
cmake -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_BUILD_TYPE=Release \
  `cat ../../opencv4_cmake_options.txt` \
  -DBUILD_opencv_world=OFF ..
```

## step6 make a package
```shell
zip -r -9 opencv-mobile-4.5.1.zip install
```

# Some notes

* The minimal opencv build contains most basic opencv operators and common image processing functions, with some handy additions like keypoint feature extraction and matching, image inpainting and opticalflow estimation.

* Many computer vision algorithms that reside in dedicated modules are discarded, such as face detection etc. [You could try deep-learning based algorithms with nerual network inference library optimized for mobile.](https://github.com/Tencent/ncnn)

* Image IO functions in highgui module, like ```cv::imread``` and ```cv::imwrite```, are re-implemented using [stb](https://github.com/nothings/stb) for smaller code size. GUI functions, like ```cv::imshow```, are discarded.

* cuda and opencl are disabled because there is no cuda on mobile, no opencl on ios, and opencl on android is slow. opencv on gpu is not suitable for real productions. Write metal on ios and opengles/vulkan on android if you need good gpu acceleration.

* C++ RTTI and exceptions are disabled for minimal build. Be careful when you write ```cv::Mat roi = image(roirect);```  :P

# opencv modules included

|module|comment|
|---|---|
|opencv_core|Mat, matrix operations, etc|
|opencv_imgproc|resize, cvtColor, warpAffine, etc|
|opencv_highgui|imread, imwrite|
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
|opencv_imgcodecs|link with opencv_highgui instead|
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


