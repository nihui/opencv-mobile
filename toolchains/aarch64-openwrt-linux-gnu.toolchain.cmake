set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(DEFINED ENV{TOOLCHAIN_ROOT_PATH})
    file(TO_CMAKE_PATH $ENV{TOOLCHAIN_ROOT_PATH} TOOLCHAIN_ROOT_PATH)
else()
    message(FATAL_ERROR "TOOLCHAIN_ROOT_PATH env must be defined")
endif()

set(TOOLCHAIN_ROOT_PATH ${TOOLCHAIN_ROOT_PATH} CACHE STRING "root path to toolchain")

set(CMAKE_C_COMPILER "${TOOLCHAIN_ROOT_PATH}/bin/aarch64-openwrt-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT_PATH}/bin/aarch64-openwrt-linux-gnu-g++")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_C_FLAGS "-march=armv8-a")
set(CMAKE_CXX_FLAGS "-march=armv8-a")

# cache flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "c flags")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "c++ flags")
