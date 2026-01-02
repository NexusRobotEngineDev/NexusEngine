#include <gtest/gtest.h>
#include <SDL3/SDL.h>
#include "Interfaces.h"
#include "Context.h"
#include "Vk/VK_Context.h"
#include "Sdl/SDL_Window.h"
#include "Threading.h"
#include <thread>
#include <chrono>

using namespace Nexus;

TEST(SDL_Window, DirectInitialization) {
    WindowPtr window = CreateNativeWindow();
    ASSERT_NE(window, nullptr);

    Status status = window->initialize();
    EXPECT_TRUE(status.ok());

    if (status.ok()) {
        Status createStatus = window->createWindow("Test Window", 800, 600);
        EXPECT_TRUE(createStatus.ok());

        if (createStatus.ok()) {
            window->shutdown();
        }
    }

    delete window;
}

TEST(SDL_Window, AsyncThreadInitialization) {
    WindowThread windowThread;
    windowThread.startThread();

    WindowPtr window = nullptr;
    Status status = windowThread.createWindowAsync("Async Test Window", 800, 600, window);

    EXPECT_TRUE(status.ok()) << "Async creation failed: " << status.message();
    ASSERT_NE(window, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_FALSE(window->shouldClose());

    windowThread.stop();

    window->shutdown();
    delete window;
}

TEST(Context, Initialization) {
    SDL_Init(SDL_INIT_VIDEO);

    ContextPtr context = CreateContext();
    if (context) {
        Status s = context->initialize();
        EXPECT_TRUE(s.ok()) << "Context init failed: " << s.message();
        context->shutdown();
        delete context;
    }

    SDL_Quit();
}
