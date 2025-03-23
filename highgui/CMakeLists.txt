set(the_description "High-level GUI")
ocv_add_module(highgui opencv_imgproc)

option(WITH_CVI "build with cvi" OFF)
option(WITH_AW "build with aw" OFF)
option(WITH_RK "build with rk" OFF)

set(highgui_srcs
    ${CMAKE_CURRENT_LIST_DIR}/src/exif.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/highgui.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/kanna_rotate.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/videocapture.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/videowriter.cpp

    # dnn
    ${CMAKE_CURRENT_LIST_DIR}/src/nms.cpp
    )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND highgui_srcs
    ${CMAKE_CURRENT_LIST_DIR}/src/capture_v4l2.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/display_fb.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/writer_http.cpp)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  list(APPEND highgui_srcs
          ${CMAKE_CURRENT_LIST_DIR}/src/display_win32.cpp)
endif()

if(WITH_CVI)
  list(APPEND highgui_srcs
    ${CMAKE_CURRENT_LIST_DIR}/src/capture_cvi.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/jpeg_decoder_cvi.cpp)
  add_definitions(-DCV_WITH_CVI=1)
  message(STATUS "highgui cvi enabled")
endif()

if(WITH_AW)
  list(APPEND highgui_srcs
    ${CMAKE_CURRENT_LIST_DIR}/src/capture_v4l2_aw_isp.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/jpeg_decoder_aw.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/jpeg_encoder_aw.cpp)
  add_definitions(-DCV_WITH_AW=1)
  message(STATUS "highgui aw enabled")
endif()

if(WITH_RK)
  list(APPEND highgui_srcs
    ${CMAKE_CURRENT_LIST_DIR}/src/capture_v4l2_rk_aiq.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/jpeg_encoder_rk_mpp.cpp)
  add_definitions(-DCV_WITH_RK=1)
  message(STATUS "highgui rk enabled")
endif()

file(GLOB highgui_ext_hdrs
     "${CMAKE_CURRENT_LIST_DIR}/include/opencv2/*.hpp"
     "${CMAKE_CURRENT_LIST_DIR}/include/opencv2/${name}/*.hpp"
     "${CMAKE_CURRENT_LIST_DIR}/include/opencv2/${name}/*.h"
     "${CMAKE_CURRENT_LIST_DIR}/include/opencv2/dnn/*.hpp"
     "${CMAKE_CURRENT_LIST_DIR}/include/opencv2/dnn/*.h")

if(UNIX OR OPENCV_VERSION_MAJOR GREATER_EQUAL 3)
  #these variables are set by CHECK_MODULE macro
  foreach(P ${HIGHGUI_INCLUDE_DIRS})
    ocv_include_directories(${P})
  endforeach()

  foreach(P ${HIGHGUI_LIBRARY_DIRS})
    link_directories(${P})
  endforeach()
endif()

source_group("Src" FILES ${highgui_srcs})
source_group("Include" FILES ${highgui_ext_hdrs})
ocv_set_module_sources(HEADERS ${highgui_ext_hdrs} SOURCES ${highgui_srcs})
ocv_module_include_directories()

ocv_create_module(${HIGHGUI_LIBRARIES})

if(OPENCV_VERSION_MAJOR EQUAL 2)
  if(BUILD_SHARED_LIBS)
    add_definitions(-DHIGHGUI_EXPORTS)
  endif()

  if(MSVC)
    set_target_properties(${the_module} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:atlthunk.lib /NODEFAULTLIB:atlsd.lib /NODEFAULTLIB:libcmt.lib /DEBUG")
  endif()

  ocv_warnings_disable(CMAKE_CXX_FLAGS -Wno-deprecated-declarations -Wno-clobbered)
else()
  macro(ocv_highgui_configure_target)
  if(APPLE)
    add_apple_compiler_options(the_module)
  endif()

  if(MSVC)
    set_target_properties(${the_module} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:atlthunk.lib /NODEFAULTLIB:atlsd.lib /NODEFAULTLIB:libcmt.lib /DEBUG")
  endif()

  ocv_warnings_disable(CMAKE_CXX_FLAGS -Wno-deprecated-declarations)
  endmacro()

  if(NOT BUILD_opencv_world)
    ocv_highgui_configure_target()
  endif()
endif()
