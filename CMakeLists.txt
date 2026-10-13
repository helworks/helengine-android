cmake_minimum_required(VERSION 3.22)
project(helengine_android LANGUAGES CXX)

if(NOT ANDROID)
    message(FATAL_ERROR "Use the Android NDK CMake toolchain to build the native host.")
endif()

add_library(helengine_android SHARED
    src/platform/android/android_activity.cpp
    src/platform/android/android_boot_frame.cpp)
target_compile_features(helengine_android PRIVATE cxx_std_17)
target_compile_options(helengine_android PRIVATE -Wall -Wextra -Werror)
target_link_libraries(helengine_android PRIVATE android log)
