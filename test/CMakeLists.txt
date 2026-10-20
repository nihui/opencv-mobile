cmake_minimum_required(VERSION 3.10)
project(opencv-mobile-test)

set(OPENCV_MOBILE_TEST_CXX_STANDARD "" CACHE STRING "C++ standard for the test target")

if(APPLE OR IOS)
    # set OpenCV_DIR to where opencv2.framework resides
    find_library(OpenCV_LIBS NAMES opencv2 PATHS ${OpenCV_DIR})
    if(NOT OpenCV_LIBS AND EXISTS "${OpenCV_DIR}/opencv2.framework")
        set(OpenCV_LIBS "${OpenCV_DIR}/opencv2.framework")
    endif()
    if(OpenCV_LIBS AND EXISTS "${OpenCV_LIBS}/Headers/core/version.hpp")
        file(STRINGS "${OpenCV_LIBS}/Headers/core/version.hpp" _opencv_version_major_line REGEX "^#define CV_VERSION_MAJOR")
        file(STRINGS "${OpenCV_LIBS}/Headers/core/version.hpp" _opencv_version_minor_line REGEX "^#define CV_VERSION_MINOR")
        file(STRINGS "${OpenCV_LIBS}/Headers/core/version.hpp" _opencv_version_patch_line REGEX "^#define CV_VERSION_REVISION")
        string(REGEX REPLACE ".* +([0-9]+)$" "\\1" OpenCV_VERSION_MAJOR "${_opencv_version_major_line}")
        string(REGEX REPLACE ".* +([0-9]+)$" "\\1" OpenCV_VERSION_MINOR "${_opencv_version_minor_line}")
        string(REGEX REPLACE ".* +([0-9]+)$" "\\1" OpenCV_VERSION_PATCH "${_opencv_version_patch_line}")
        set(OpenCV_VERSION "${OpenCV_VERSION_MAJOR}.${OpenCV_VERSION_MINOR}.${OpenCV_VERSION_PATCH}")
    endif()
else()
    # set OpenCV_DIR to where OpenCVConfig.cmake resides
    find_package(OpenCV REQUIRED)
endif()

add_executable(opencv-mobile-test main.cpp)

if(NOT OPENCV_MOBILE_TEST_CXX_STANDARD AND DEFINED OpenCV_VERSION)
    if(OpenCV_VERSION VERSION_GREATER_EQUAL 5)
        set(OPENCV_MOBILE_TEST_CXX_STANDARD 17)
    elseif(OpenCV_VERSION VERSION_GREATER_EQUAL 4)
        set(OPENCV_MOBILE_TEST_CXX_STANDARD 11)
    endif()
endif()

if(OPENCV_MOBILE_TEST_CXX_STANDARD)
    set_property(TARGET opencv-mobile-test PROPERTY CXX_STANDARD ${OPENCV_MOBILE_TEST_CXX_STANDARD})
    set_property(TARGET opencv-mobile-test PROPERTY CXX_STANDARD_REQUIRED ON)
endif()

target_link_libraries(opencv-mobile-test ${OpenCV_LIBS})
