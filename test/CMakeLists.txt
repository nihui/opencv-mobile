project(opencv-mobile-test)
cmake_minimum_required(VERSION 3.5)

# opencv4 requires c++11
set(CMAKE_CXX_STANDARD 11)

if(APPLE OR IOS)
    # set OpenCV_DIR to where opencv2.framework resides
    find_library(OpenCV_LIBS NAMES opencv2 PATHS ${OpenCV_DIR})
else()
    # set OpenCV_DIR to where OpenCVConfig.cmake resides
    find_package(OpenCV REQUIRED)
endif()

add_executable(opencv-mobile-test main.cpp)

target_link_libraries(opencv-mobile-test ${OpenCV_LIBS})
