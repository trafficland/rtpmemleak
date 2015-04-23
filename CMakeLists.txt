cmake_minimum_required (VERSION 2.8.9) 
set(PROJECT_NAME "rtpmemleak")
project(${PROJECT_NAME})
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_COMPILER  "g++")
set(CMAKE_CXX_FLAGS "-std=c++1y -pedantic -Wall -Wextra -D__STDC_FORMAT_MACROS -D__STDC_CONSTANT_MACROS")
link_directories(~/dev/ffmpeg_build/lib/)
include_directories(~/dev/ffmpeg_build/include/)

file(GLOB TL_SRC
    "src/*.h"
    "src/*.cpp"
)
add_executable(${PROJECT_NAME} ${TL_SRC})

if(APPLE)
    MESSAGE("!!! Writing build for OS X based system. !!!")
    FIND_LIBRARY(CORE_FOUNDATION CoreFoundation)
    FIND_LIBRARY(CORE_VIDEO CoreVideo)
    FIND_LIBRARY(VIDEO_DECODE_ACC VideoDecodeAcceleration)
    target_link_libraries(${PROJECT_NAME} bz2 iconv lzma ${CORE_FOUNDATION} ${CORE_VIDEO} ${VIDEO_DECODE_ACC} z swresample swscale x264 avutil avcodec avformat)
elseif(UNIX)
    MESSAGE("@@@ Writing build for *nix based system. @@@")
    target_link_libraries(${PROJECT_NAME} swscale avdevice avformat avcodec avutil bz2 lzma z x264 dl swresample rt)
endif()