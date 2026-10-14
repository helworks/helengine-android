cmake_minimum_required(VERSION 3.22)
project(helengine_android LANGUAGES C CXX)
set(CMAKE_OBJECT_PATH_MAX 240)

if(NOT ANDROID)
    message(FATAL_ERROR "Use the Android NDK CMake toolchain to build the native host.")
endif()

find_package(games-frame-pacing REQUIRED CONFIG)

add_library(helengine_android SHARED
    src/platform/android/android_activity.cpp
    src/platform/android/rendering/AndroidVulkanDevice.cpp
    src/platform/android/rendering/AndroidVulkanSwapchain.cpp
    src/platform/android/rendering/AndroidVulkanPipeline.cpp
    src/platform/android/rendering/AndroidVulkanRenderer.cpp)
target_sources(helengine_android PRIVATE src/platform/android/rendering/AndroidSceneGpu.cpp)
target_sources(helengine_android PRIVATE src/platform/android/rendering/AndroidSceneGpuMemory.cpp)
target_sources(helengine_android PRIVATE src/platform/android/diagnostics/AndroidMemoryTrimBridge.cpp)
target_sources(helengine_android PRIVATE src/platform/android/rendering/AndroidShadowMap.cpp)
target_sources(helengine_android PRIVATE src/platform/android/rendering/AndroidSceneRenderTarget.cpp)
target_sources(helengine_android PRIVATE src/platform/android/rendering/AndroidFramePacer.cpp)
target_compile_features(helengine_android PRIVATE cxx_std_17)
target_compile_definitions(helengine_android PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
target_compile_options(helengine_android PRIVATE -Wall -Wextra -Werror)
target_link_libraries(helengine_android PRIVATE android log vulkan dl games-frame-pacing::swappy_static)

if(HELENGINE_CORE_CPP_ROOT)
    find_package(games-controller REQUIRED CONFIG)
    target_compile_features(helengine_android PRIVATE cxx_std_20)
    if(NOT EXISTS "${HELENGINE_CORE_CPP_ROOT}/helcpp_config.hpp")
        message(FATAL_ERROR "The Android game build requires a generated core with helcpp_config.hpp.")
    endif()
    target_sources(helengine_android PRIVATE
        "${HELENGINE_CORE_CPP_ROOT}/helengine_core_unity.cpp"
        "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_startup_manifest.cpp"
        "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_scene_catalog_manifest.cpp"
        "${HELENGINE_CORE_CPP_ROOT}/runtime/runtime_code_module_manifest.cpp"
        src/platform/android/runtime/AndroidGameHost.cpp
        src/platform/android/runtime/AndroidAssetStream.cpp
        src/platform/android/runtime/AndroidAssetContentStreamSource.cpp
        src/platform/android/runtime/AndroidProcessMemoryReader.cpp
        src/platform/android/runtime/AndroidRuntimeDiagnosticsProvider.cpp
        src/platform/android/diagnostics/AndroidThermalStatusReader.cpp
        src/platform/android/runtime/AndroidDisplayDensity.cpp
        src/platform/android/runtime/AndroidLaunchOptions.cpp
        src/platform/android/runtime/AndroidInputBackend.cpp
        src/platform/android/runtime/AndroidGamepadBackend.cpp
        src/platform/android/runtime/AndroidPlatformSettingsBackend.cpp
        src/platform/android/runtime/AndroidAudioBackend.cpp
        src/platform/android/runtime/AndroidAudioOutput.cpp
        src/platform/android/runtime/AndroidAudioFocus.cpp
        src/platform/android/runtime/AndroidRenderManager3D.cpp
        src/platform/android/runtime/AndroidRenderManager2D.cpp
        src/platform/android/android_game_activity.cpp
        "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
    target_include_directories(helengine_android PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue")
    target_link_options(helengine_android PRIVATE "-uANativeActivity_onCreate")
    target_link_libraries(helengine_android PRIVATE aaudio games-controller::paddleboat_static)
    target_include_directories(helengine_android PRIVATE "${HELENGINE_CORE_CPP_ROOT}")
    target_compile_definitions(helengine_android PRIVATE HELENGINE_ANDROID_GAME=1)
    set_source_files_properties("${HELENGINE_CORE_CPP_ROOT}/helengine_core_unity.cpp" PROPERTIES COMPILE_OPTIONS "-w")
    set_source_files_properties("${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c" PROPERTIES COMPILE_OPTIONS "-w")
endif()
