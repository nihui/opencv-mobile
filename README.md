# opencv-mobile

This project provides the minimal build of opencv library for the android and ios platforms.

We provide prebuild binary packages for opencv 2.4.13.7, 3.4.13 and 4.5.1.

# download

|opencv|android|ios|ios-bitcode|
|---|---|---|---|
||armeabi-v7a arm64-v8a x86 x86_64|armv7 arm64 arm64e i386 x86_64|armv7 arm64 arm64e i386 x86_64|
|2.4.13.7|tbd|tbd|tbd|
|3.4.13|tbd|tbd|tbd|
|4.5.1|tbd|tbd|tbd|

# some notes

The minimal opencv build contains most basic opencv operators and common image processing functions, with some handy additions like keypoint feature extraction and matching, image inpainting and opticalflow estimation.

Many computer vision algorithms that reside in dedicated modules are discarded, such as face detection etc. You could try deep-learning based algorithms with nerual network inference library optimized for mobile. ncnn is a good one.

cuda and opencl are disabled because there is no cuda on mobile, no opencl on ios, and opencl on android is slow. opencv on gpu is not suitable for real productions. Write metal on ios and opengles/vulkan on android if you need good gpu acceleration.

C++ RTTI and exceptions are disabled for minimal build. Be careful when you write ```cv::Mat roi = image(roirect);```  :P

# opencv modules contained

|module|comment|
|---|---|
|opencv_core|Mat, matrix operations, etc|
|opencv_imgproc|resize, cvtColor, warpAffine, etc|
|opencv_features2d|keypoint feature and mather, etc (not included in opencv 2.x package)|
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


