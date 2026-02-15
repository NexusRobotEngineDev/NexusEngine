file(GLOB CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Editor/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Editor/*.cpp"
)

add_library(Core STATIC ${CORE_SOURCES})

find_package(nlohmann_json CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(cppzmq CONFIG REQUIRED)
find_package(tinyxml2 CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

target_link_libraries(Core PUBLIC BridgeImpl Bridge nlohmann_json::nlohmann_json assimp::assimp cppzmq tinyxml2::tinyxml2 httplib::httplib Cesium3DTilesSelection CesiumGltf CesiumUtility spdlog::spdlog fmt::fmt)

target_include_directories(Core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
