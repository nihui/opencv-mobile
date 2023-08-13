project(opencv-mobile-test)
cmake_minimum_required(VERSION 2.8.12)

find_package(OpenCV REQUIRED)

add_executable(opencv-mobile-test main.cpp)

target_link_libraries(opencv-mobile-test ${OpenCV_LIBS})
