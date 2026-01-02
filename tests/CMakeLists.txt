enable_testing()

find_package(GTest CONFIG REQUIRED)

add_executable(NexusTests
    Test_Main.cpp
    Bridge/Test_SDL_Window.cpp
    Bridge/Test_Threading_Safety.cpp
    Bridge/Test_MuJoCo_Physics.cpp
    Bridge/Test_Material_System.cpp
)

target_link_libraries(NexusTests PRIVATE
    GTest::gtest
    BridgeImpl
)

include(GoogleTest)
gtest_discover_tests(NexusTests)
