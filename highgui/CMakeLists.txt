set(the_description "High-level GUI")
ocv_add_module(highgui opencv_imgproc)

set(highgui_srcs
    ${CMAKE_CURRENT_LIST_DIR}/src/exif.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/highgui.cpp
    )

file(GLOB highgui_ext_hdrs
     "${CMAKE_CURRENT_LIST_DIR}/include/opencv2/*.hpp"
     "${CMAKE_CURRENT_LIST_DIR}/include/opencv2/${name}/*.hpp"
     "${CMAKE_CURRENT_LIST_DIR}/include/opencv2/${name}/*.h")

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
