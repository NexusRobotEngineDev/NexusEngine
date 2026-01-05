add_library(Core STATIC
    RenderSystem.h
    RenderSystem.cpp
    DrawCommandGenerator.h
    DrawCommandGenerator.cpp
    MeshManager.h
    MeshManager.cpp
)

target_link_libraries(Core PUBLIC BridgeImpl Bridge)
target_include_directories(Core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
