enable_testing()

find_package(GTest CONFIG REQUIRED)

file(GLOB_RECURSE TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(NexusTests ${TEST_SOURCES})

target_link_libraries(NexusTests PRIVATE
    GTest::gtest
    BridgeImpl
    Core
)

include(GoogleTest)
gtest_discover_tests(NexusTests)
