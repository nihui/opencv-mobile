project(opencv-mobile-test)

find_package(OpenCV REQUIRED)

add_executable(opencv-mobile-test main.cpp)

target_link_libraries(opencv-mobile-test ${OpenCV_LIBS})
