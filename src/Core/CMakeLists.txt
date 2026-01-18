file(GLOB CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Editor/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Editor/*.cpp"
)

add_library(Core STATIC ${CORE_SOURCES})

target_link_libraries(Core PUBLIC BridgeImpl Bridge)

target_include_directories(Core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
