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
    set(CMAKE_C_SHOWINCLUDES_PREFIX "Note: including file: ")
    set(CMAKE_CXX_SHOWINCLUDES_PREFIX "Note: including file: ")
endif()

find_package(absl CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)


add_subdirectory(src/Bridge)
add_subdirectory(src/Core)

add_executable(NexusApp src/App/Main.cpp)
target_link_libraries(NexusApp PRIVATE Core BridgeImpl Bridge)

add_subdirectory(tests)
