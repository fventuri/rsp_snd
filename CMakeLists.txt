cmake_minimum_required(VERSION 3.20)
project(rsp_snd)

find_library(SDRPLAY_API_LIBRARIES libsdrplay_api.so.3)
if(NOT SDRPLAY_API_LIBRARIES)
    message(FATAL_ERROR "libsdrplay_api v3 not found.")
endif()
find_package(ALSA REQUIRED)
get_filename_component(SDRPLAY_API_LIBRARY_DIRS "${SDRPLAY_API_LIBRARIES}" DIRECTORY)

add_subdirectory(src)
