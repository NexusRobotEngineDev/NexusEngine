cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ENV{VCPKG_ROOT})
    message(FATAL_ERROR "Error: VCPKG_ROOT environment variable is not defined. Please set it to your vcpkg installation root.")
endif()


if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    include(DetectTriplet)
endif()

set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets" CACHE PATH "Overlay triplets")

file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" VCPKG_ROOT_PATH)
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT_PATH}/scripts/buildsystems/vcpkg.cmake")

project(NexusEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(absl CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)

include(FetchContent)

set(CESIUM_TESTS_ENABLED OFF CACHE BOOL "" FORCE)
set(CESIUM_USE_SYSTEM_SQLITE3 ON CACHE BOOL "" FORCE)
set(CESIUM_USE_SYSTEM_URIPARSER ON CACHE BOOL "" FORCE)
set(CESIUM_USE_SYSTEM_DRACO ON CACHE BOOL "" FORCE)
set(CESIUM_USE_SYSTEM_RAPIDJSON OFF CACHE BOOL "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

FetchContent_Declare(
    cesium-native
    GIT_REPOSITORY https://github.com/CesiumGS/cesium-native.git
    GIT_TAG        v0.35.0
    PATCH_COMMAND  ${CMAKE_COMMAND} -DCESIUM_SRC_DIR=<SOURCE_DIR> -P "${CMAKE_CURRENT_SOURCE_DIR}/patch_cesium.cmake"
)
set(CMAKE_C_COMPILER_WORKS TRUE)
FetchContent_MakeAvailable(cesium-native)


add_subdirectory(src/Bridge)
add_subdirectory(src/Core)

add_executable(NexusApp src/App/Main.cpp)
target_link_libraries(NexusApp PRIVATE Core BridgeImpl Bridge spdlog::spdlog fmt::fmt)
add_custom_command(TARGET NexusApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Data
        $<TARGET_FILE_DIR:NexusApp>/Data
)

add_subdirectory(tests)
