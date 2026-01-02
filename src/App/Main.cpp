#include "Context.h"
#include "Threading.h"
#include "ECS.h"
#include "Entity.h"
#include "Log.h"
#include "PhysicsThread.h"

#if ENABLE_VULKAN
#include "Vk/VK_Context.h"
#include "Vk/VK_Swapchain.h"
#include "Vk/VK_Renderer.h"
#endif

using namespace Nexus;

namespace {
WindowPtr g_window = nullptr;
ContextPtr g_context = nullptr;
std::unique_ptr<WindowThread> g_windowThread;
std::unique_ptr<RHIThread> g_rhiThread;
std::unique_ptr<PhysicsThread> g_physicsThread;
std::unique_ptr<Registry> g_ecsRegistry;
PhysicsSystemPtr g_physicsSystem = nullptr;
std::atomic<bool> g_quit{false};

#if ENABLE_VULKAN
std::unique_ptr<VK_Swapchain> g_swapchain;
std::unique_ptr<VK_Renderer> g_renderer;
#endif

struct Position { float x, y; };

Status InitializeEngine() {
    g_ecsRegistry = std::make_unique<Registry>();

    auto entity = g_ecsRegistry->create();
    g_ecsRegistry->emplace<Position>(entity, 0.0f, 0.0f);

    g_windowThread = std::make_unique<WindowThread>();
    g_windowThread->startThread();

    NX_RETURN_IF_ERROR(g_windowThread->createWindowAsync("Nexus Engine", 1280, 720, g_window));

    g_context = CreateContext();
    if (!g_context) return InternalError("Context creation failed");

    NX_RETURN_IF_ERROR(g_context->initialize());
    NX_RETURN_IF_ERROR(g_context->initializeWindowSurface(g_window->getNativeHandle()));

#if ENABLE_VULKAN
    auto vkContext = static_cast<VK_Context*>(g_context);
    g_swapchain = std::make_unique<VK_Swapchain>(vkContext->getInstance(), vkContext->getPhysicalDevice(), vkContext->getDevice(), vkContext->getSurface());
    NX_RETURN_IF_ERROR(g_swapchain->initialize(1280, 720));

    g_renderer = std::make_unique<VK_Renderer>(vkContext, g_swapchain.get());
    NX_RETURN_IF_ERROR(g_renderer->initialize());

    g_rhiThread = std::make_unique<RHIThread>(vkContext);
    g_rhiThread->setRenderer(g_renderer.get());
    g_rhiThread->startThread();
#endif

    g_physicsSystem = new PhysicsSystem();
    NX_RETURN_IF_ERROR(g_physicsSystem->initialize());

    g_physicsThread = std::make_unique<PhysicsThread>(g_physicsSystem);
    g_physicsThread->startThread();

    return OkStatus();
}

void ShutdownEngine() {
    if (g_rhiThread) g_rhiThread->stop();

    if (g_physicsThread) g_physicsThread->stop();

    if (g_physicsSystem) {
        g_physicsSystem->shutdown();
        delete g_physicsSystem;
        g_physicsSystem = nullptr;
    }

#if ENABLE_VULKAN
    g_renderer.reset();
    g_swapchain.reset();
#endif

    if (g_context) {
        g_context->shutdown();
        delete g_context;
        g_context = nullptr;
    }

    if (g_windowThread) g_windowThread->stop();

    g_window = nullptr;
}

void RunMainLoop() {
    uint32_t lastWidth = 1280;
    uint32_t lastHeight = 720;

    while (!g_quit) {
        if (g_window->shouldClose()) {
            g_quit = true;
            break;
        }

        uint32_t currentWidth = static_cast<SDL_Window_Wrapper*>(g_window)->getWidth();
        uint32_t currentHeight = static_cast<SDL_Window_Wrapper*>(g_window)->getHeight();
        if (currentWidth != lastWidth || currentHeight != lastHeight) {
            lastWidth = currentWidth;
            lastHeight = currentHeight;
            if (g_rhiThread) {
                RenderCommand cmd;
                cmd.type = RenderCommandType::Resize;
                cmd.width = currentWidth;
                cmd.height = currentHeight;
                g_rhiThread->pushCommand(cmd);
            }
        }

#if ENABLE_VULKAN
        if (g_rhiThread) {
            g_rhiThread->requestSync();
            g_rhiThread->resumeSync();

            RenderCommand cmd;
            cmd.type = RenderCommandType::Draw;
            g_rhiThread->pushCommand(cmd);
        }
#else
        g_context->sync();
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}
} // namespace

int main(int argc, char* argv[]) {
    Log::init();
    Log::info("Nexus Engine Starting...");

    if (auto status = InitializeEngine(); !status.ok()) {
        Log::critical("Engine init failed: {}", status.message());
        return -1;
    }

    RunMainLoop();
    Log::info("Nexus Engine Shutting down...");
    ShutdownEngine();

    return 0;
}
