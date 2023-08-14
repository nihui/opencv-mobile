project(opencv-mobile-test)
cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)

if(APPLE OR IOS)
    # copy opencv2.framework to this project dir
    find_library(OpenCV_LIBS NAMES opencv2 PATHS ${OpenCV_DIR})
else()
    find_package(OpenCV REQUIRED)
endif()

add_executable(opencv-mobile-test main.cpp)

target_link_libraries(opencv-mobile-test ${OpenCV_LIBS})
