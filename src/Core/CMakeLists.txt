file(GLOB CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Editor/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Editor/*.cpp"
)

add_library(Core STATIC ${CORE_SOURCES})

find_package(nlohmann_json CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
target_link_libraries(Core PUBLIC BridgeImpl Bridge nlohmann_json::nlohmann_json assimp::assimp)

target_include_directories(Core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
