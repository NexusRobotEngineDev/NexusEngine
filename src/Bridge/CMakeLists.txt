
option(ENABLE_VULKAN "Enable Vulkan backend" ON)
option(ENABLE_SDL "Enable SDL backend" ON)
option(ENABLE_RMLUI "Enable RmlUi" ON)
option(ENABLE_MUJOCO "Enable MuJoCo Physics" ON)

set(GRAPHICS_BACKEND_COUNT 0)
if(ENABLE_VULKAN)
    math(EXPR GRAPHICS_BACKEND_COUNT "${GRAPHICS_BACKEND_COUNT} + 1")
    find_package(Vulkan REQUIRED)
endif()

set(WINDOW_BACKEND_COUNT 0)
if(ENABLE_SDL)
    math(EXPR WINDOW_BACKEND_COUNT "${WINDOW_BACKEND_COUNT} + 1")
    find_package(SDL3 CONFIG REQUIRED)
endif()

set(PHYSICS_BACKEND_COUNT 0)
if(ENABLE_MUJOCO)
    math(EXPR PHYSICS_BACKEND_COUNT "${PHYSICS_BACKEND_COUNT} + 1")
    find_package(mujoco CONFIG REQUIRED)
endif()

if(ENABLE_RMLUI)
    find_package(RmlUi CONFIG REQUIRED)
endif()

configure_file(Config.h.in "${CMAKE_CURRENT_BINARY_DIR}/Config.h")

add_library(Bridge INTERFACE)
target_include_directories(Bridge INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

file(GLOB BRIDGE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

if(ENABLE_VULKAN)
    file(GLOB VK_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Vk/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Vk/*.cpp"
    )
    list(APPEND BRIDGE_SOURCES ${VK_SOURCES})
endif()

if(ENABLE_SDL)
    file(GLOB SDL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Sdl/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Sdl/*.cpp"
    )
    list(APPEND BRIDGE_SOURCES ${SDL_SOURCES})
endif()

if(ENABLE_MUJOCO)
    file(GLOB MUJOCO_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/MuJoCo/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/MuJoCo/*.cpp"
    )
    list(APPEND BRIDGE_SOURCES ${MUJOCO_SOURCES})
endif()

add_library(BridgeImpl STATIC ${BRIDGE_SOURCES})
target_link_libraries(BridgeImpl PUBLIC Bridge absl::status absl::statusor EnTT::EnTT)

if(ENABLE_VULKAN)
    find_package(unofficial-shaderc CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(fmt CONFIG REQUIRED)
    target_link_libraries(BridgeImpl PUBLIC Vulkan::Vulkan unofficial::shaderc::shaderc spdlog::spdlog fmt::fmt)
    target_compile_definitions(BridgeImpl PUBLIC VULKAN_HPP_NO_EXCEPTIONS ENABLE_VULKAN)
endif()

if(ENABLE_SDL)
    target_compile_definitions(BridgeImpl PUBLIC ENABLE_SDL)
    target_link_libraries(BridgeImpl PUBLIC SDL3::SDL3)
endif()

if(ENABLE_RMLUI)
    target_compile_definitions(BridgeImpl PUBLIC ENABLE_RMLUI)
    target_link_libraries(BridgeImpl PUBLIC RmlUi::RmlUi)
endif()

if(ENABLE_MUJOCO)
    target_compile_definitions(BridgeImpl PUBLIC ENABLE_MUJOCO)
    target_link_libraries(BridgeImpl PUBLIC mujoco::mujoco)
endif()
