# opencv-mobile

![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg?style=for-the-badge)
![build](https://img.shields.io/github/actions/workflow/status/nihui/opencv-mobile/release.yml?style=for-the-badge)
![download](https://img.shields.io/github/downloads/nihui/opencv-mobile/total.svg?style=for-the-badge)

![Android](https://img.shields.io/badge/Android-3DDC84?style=for-the-badge&logo=android&logoColor=white)
![iOS](https://img.shields.io/badge/iOS-000000?style=for-the-badge&logo=ios&logoColor=white)
![ARM Linux](https://img.shields.io/badge/ARM_Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![Windows](https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![Ubuntu](https://img.shields.io/badge/Ubuntu-E95420?style=for-the-badge&logo=ubuntu&logoColor=white)
![MacOS](https://img.shields.io/badge/mac%20os-000000?style=for-the-badge&logo=apple&logoColor=white)
![Firefox](https://img.shields.io/badge/Firefox-FF7139?style=for-the-badge&logo=Firefox-Browser&logoColor=white)
![Chrome](https://img.shields.io/badge/Chrome-4285F4?style=for-the-badge&logo=Google-chrome&logoColor=white)

✔️ This project provides the minimal build of opencv library for the **Android**, **iOS** and **ARM Linux** platforms.

✔️ Packages for **Windows**, **Linux**, **MacOS** and **WebAssembly** are available now.

✔️ We provide prebuild binary packages for opencv **2.4.13.7**, **3.4.20** and **4.8.0**.

✔️ We also provide prebuild binary package for **iOS/iOS-Simulator with bitcode enabled**, that the official package lacks.

✔️ We also provide prebuild binary package for **Mac-Catalyst** and **Apple xcframework**, that the official package lacks.

✔️ All the binaries are compiled from source on github action, **no virus**, **no backdoor**, **no secret code**.

|opencv 4.8.0 android|package size|
|---|---|
|The official opencv|189MB|
|opencv-mobile|17.7MB|

|opencv 4.8.0 ios|package size|
|---|---|
|The official opencv|197MB|
|opencv-mobile|9.88MB|

|opencv 4.8.0 ios with bitcode|package size|
|---|---|
|The official opencv|missing :(|
|opencv-mobile|34MB|

# Download

https://github.com/nihui/opencv-mobile/releases/latest

|Platform|Arch|opencv-2.4.13.7|opencv-3.4.20|opencv-4.8.0|
|:-:|:-:|:-:|:-:|:-:|
|Android|armeabi-v7a<br />arm64-v8a<br />x86<br />x86_64|[![download-icon]][opencv2-android-url]|[![download-icon]][opencv3-android-url]|[![download-icon]][opencv4-android-url]|
|iOS|armv7<br />arm64<br />arm64e|[![download-icon]][opencv2-ios-url]<br />[![bitcode-icon]][opencv2-ios-bitcode-url]|[![download-icon]][opencv3-ios-url]<br />[![bitcode-icon]][opencv3-ios-bitcode-url]|[![download-icon]][opencv4-ios-url]<br />[![bitcode-icon]][opencv4-ios-bitcode-url]|
|iOS-Simulator|i386<br />x86_64<br />arm64|[![download-icon]][opencv2-ios-simulator-url]<br />[![bitcode-icon]][opencv2-ios-simulator-bitcode-url]|[![download-icon]][opencv3-ios-simulator-url]<br />[![bitcode-icon]][opencv3-ios-simulator-bitcode-url]|[![download-icon]][opencv4-ios-simulator-url]<br />[![bitcode-icon]][opencv4-ios-simulator-bitcode-url]|
|MacOS|x86_64<br />arm64|[![download-icon]][opencv2-macos-url]|[![download-icon]][opencv3-macos-url]|[![download-icon]][opencv4-macos-url]|
|Mac-Catalyst|x86_64<br />arm64|[![download-icon]][opencv2-mac-catalyst-url]<br />[![bitcode-icon]][opencv2-mac-catalyst-bitcode-url]|[![download-icon]][opencv3-mac-catalyst-url]<br />[![bitcode-icon]][opencv3-mac-catalyst-bitcode-url]|[![download-icon]][opencv4-mac-catalyst-url]<br />[![bitcode-icon]][opencv4-mac-catalyst-bitcode-url]|
|Apple<br />xcframework|ios<br />(armv7,arm64,arm64e)<br />ios-simulator<br />(i386,x86_64,arm64)<br />ios-maccatalyst<br />(x86_64,arm64)<br />macos<br />(x86_64,arm64)|[![download-icon]][opencv2-apple-url]<br />[![bitcode-icon]][opencv2-apple-bitcode-url]|[![download-icon]][opencv3-apple-url]<br />[![bitcode-icon]][opencv3-apple-bitcode-url]|[![download-icon]][opencv4-apple-url]<br />[![bitcode-icon]][opencv4-apple-bitcode-url]|
|ARM-Linux|arm-linux-gnueabi<br />arm-linux-gnueabihf<br />aarch64-linux-gnu|[![download-icon]][opencv2-armlinux-url]|[![download-icon]][opencv3-armlinux-url]|[![download-icon]][opencv4-armlinux-url]|
|Windows-VS2015|x86<br />x64|[![download-icon]][opencv2-windows-vs2015-url]|[![download-icon]][opencv3-windows-vs2015-url]|[![download-icon]][opencv4-windows-vs2015-url]|
|Windows-VS2017|x86<br />x64|[![download-icon]][opencv2-windows-vs2017-url]|[![download-icon]][opencv3-windows-vs2017-url]|[![download-icon]][opencv4-windows-vs2017-url]|
|Windows-VS2019|x86<br />x64|[![download-icon]][opencv2-windows-vs2019-url]|[![download-icon]][opencv3-windows-vs2019-url]|[![download-icon]][opencv4-windows-vs2019-url]|
|Windows-VS2022|x86<br />x64|[![download-icon]][opencv2-windows-vs2022-url]|[![download-icon]][opencv3-windows-vs2022-url]|[![download-icon]][opencv4-windows-vs2022-url]|
|Ubuntu-20.04|x86_64|[![download-icon]][opencv2-ubuntu-2004-url]|[![download-icon]][opencv3-ubuntu-2004-url]|[![download-icon]][opencv4-ubuntu-2004-url]|
|Ubuntu-22.04|x86_64|[![download-icon]][opencv2-ubuntu-2204-url]|[![download-icon]][opencv3-ubuntu-2204-url]|[![download-icon]][opencv4-ubuntu-2204-url]|
|WebAssembly|basic<br />simd<br />threads<br />simd+threads|[![download-icon]][opencv2-webassembly-url]|[![download-icon]][opencv3-webassembly-url]|[![download-icon]][opencv4-webassembly-url]|

[download-icon]: https://img.shields.io/badge/download-blue?style=for-the-badge
[bitcode-icon]: https://img.shields.io/badge/+bitcode-blue?style=for-the-badge

[opencv2-android-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-android.zip
[opencv3-android-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-android.zip
[opencv4-android-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-android.zip

[opencv2-ios-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios.zip
[opencv3-ios-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios.zip
[opencv4-ios-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-ios.zip

[opencv2-ios-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios-bitcode.zip
[opencv3-ios-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios-bitcode.zip
[opencv4-ios-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-ios-bitcode.zip

[opencv2-ios-simulator-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios-simulator.zip
[opencv3-ios-simulator-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios-simulator.zip
[opencv4-ios-simulator-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-ios-simulator.zip

[opencv2-ios-simulator-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios-simulator-bitcode.zip
[opencv3-ios-simulator-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios-simulator-bitcode.zip
[opencv4-ios-simulator-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-ios-simulator-bitcode.zip

[opencv2-macos-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-macos.zip
[opencv3-macos-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-macos.zip
[opencv4-macos-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-macos.zip

[opencv2-mac-catalyst-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-mac-catalyst.zip
[opencv3-mac-catalyst-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-mac-catalyst.zip
[opencv4-mac-catalyst-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-mac-catalyst.zip

[opencv2-mac-catalyst-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-mac-catalyst-bitcode.zip
[opencv3-mac-catalyst-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-mac-catalyst-bitcode.zip
[opencv4-mac-catalyst-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-mac-catalyst-bitcode.zip

[opencv2-apple-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-apple.zip
[opencv3-apple-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-apple.zip
[opencv4-apple-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-apple.zip

[opencv2-apple-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-apple-bitcode.zip
[opencv3-apple-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-apple-bitcode.zip
[opencv4-apple-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-apple-bitcode.zip

[opencv2-armlinux-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-armlinux.zip
[opencv3-armlinux-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-armlinux.zip
[opencv4-armlinux-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-armlinux.zip

[opencv2-windows-vs2015-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2015.zip
[opencv3-windows-vs2015-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2015.zip
[opencv4-windows-vs2015-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-windows-vs2015.zip

[opencv2-windows-vs2017-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2017.zip
[opencv3-windows-vs2017-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2017.zip
[opencv4-windows-vs2017-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-windows-vs2017.zip

[opencv2-windows-vs2019-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2019.zip
[opencv3-windows-vs2019-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2019.zip
[opencv4-windows-vs2019-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-windows-vs2019.zip

[opencv2-windows-vs2022-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2022.zip
[opencv3-windows-vs2022-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2022.zip
[opencv4-windows-vs2022-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-windows-vs2022.zip

[opencv2-ubuntu-2004-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ubuntu-2004.zip
[opencv3-ubuntu-2004-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ubuntu-2004.zip
[opencv4-ubuntu-2004-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-ubuntu-2004.zip

[opencv2-ubuntu-2204-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ubuntu-2204.zip
[opencv3-ubuntu-2204-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ubuntu-2204.zip
[opencv4-ubuntu-2204-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-ubuntu-2204.zip

[opencv2-webassembly-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-webassembly.zip
[opencv3-webassembly-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-webassembly.zip
[opencv4-webassembly-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.0-webassembly.zip

* Android package build with ndk r25c and android api 24
* iOS / iOS-Simulator / MacOS / Mac-Catalyst package build with Xcode 13.4.1
* ARM Linux package build with cross compiler on Ubuntu-22.04
* WebAssembly package build with Emscripten 3.1.28

# Usage Android

1. Extract archive to ```<project dir>/app/src/main/jni/```
2. Modify ```<project dir>/app/src/main/jni/CMakeListst.txt``` to find and link opencv

```cmake
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.8.0-android/sdk/native/jni)
find_package(OpenCV REQUIRED)

target_link_libraries(your_jni_target ${OpenCV_LIBS})
```

# Usage iOS and MacOS

1. Extract archive, and drag ```opencv2.framework``` into your project

# Usage ARM Linux, Windows, Linux, WebAssembly

1. Extract archive to ```<project dir>/```
2. Modify ```<project dir>/CMakeListst.txt``` to find and link opencv
3. Pass ```-DOpenCV_STATIC=ON``` to cmake option for windows build

```cmake
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.8.0-armlinux/arm-linux-gnueabihf/lib/cmake/opencv4)
find_package(OpenCV REQUIRED)

target_link_libraries(your_target ${OpenCV_LIBS})
```

# How-to-build your custom package

**step 1. download opencv source**
```shell
wget -q https://github.com/opencv/opencv/archive/4.8.0.zip -O opencv-4.8.0.zip
unzip -q opencv-4.8.0.zip
cd opencv-4.8.0
```

**step 2. strip zlib dependency and use stb-based highgui implementation (optional)**
```shell
patch -p1 -i ../opencv-4.8.0-no-zlib.patch
truncate -s 0 cmake/OpenCVFindLibsGrfmt.cmake
rm -rf modules/gapi
rm -rf modules/highgui
cp -r ../highgui modules/
```

**step 3. patch opencv source for no-rtti build (optional)**
```shell
patch -p1 -i ../opencv-4.8.0-no-rtti.patch
```

**step 4. apply your opencv options to opencv4_cmake_options.txt**

**step 5. build your opencv package with cmake**
```shell
mkdir -p build
cd build
cmake -DCMAKE_INSTALL_PREFIX=install \
-DCMAKE_BUILD_TYPE=Release \
`cat ../../opencv4_cmake_options.txt` \
-DBUILD_opencv_world=OFF ..
```

**step 6. make a package**
```shell
zip -r -9 opencv-mobile-4.8.0.zip install
```

# Some notes

* The minimal opencv build contains most basic opencv operators and common image processing functions, with some handy additions like keypoint feature extraction and matching, image inpainting and opticalflow estimation.

* Many computer vision algorithms that reside in dedicated modules are discarded, such as face detection etc. [You could try deep-learning based algorithms with nerual network inference library optimized for mobile.](https://github.com/Tencent/ncnn)

* Image IO functions in highgui module, like ```cv::imread``` and ```cv::imwrite```, are re-implemented using [stb](https://github.com/nothings/stb) for smaller code size. GUI functions, like ```cv::imshow```, are discarded.

* cuda and opencl are disabled because there is no cuda on mobile, no opencl on ios, and opencl on android is slow. opencv on gpu is not suitable for real productions. Write metal on ios and opengles/vulkan on android if you need good gpu acceleration.

* C++ RTTI and exceptions are disabled for minimal build on mobile platforms and webassembly build. Be careful when you write ```cv::Mat roi = image(roirect);```  :P

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
