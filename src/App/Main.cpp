#include "Context.h"
#include "Threading.h"
#include "ECS.h"
#include "Entity.h"
#include "Log.h"
#include "PhysicsThread.h"
#include "ResourceLoader.h"
#include <cmath>

#if ENABLE_VULKAN
#include "Vk/VK_Context.h"
#include "Vk/VK_Swapchain.h"
#include "RenderSystem.h"
#include "Editor/EditorUIManager.h"
#include "Vk/VK_Renderer.h"
#endif

#include "Core/Scene.h"
#include "Core/HierarchySystem.h"
#include "Core/RoboticsDynamicsSystem.h"
#include "Core/RosBridgeSystem.h"
#include "Core/SceneSerializer.h"
#include "Core/ModelLoader.h"
#include "Core/TextureManager.h"

using namespace Nexus;
using namespace Nexus::Core;

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
std::unique_ptr<Core::RenderSystem> g_renderer;
std::unique_ptr<EditorUIManager> g_editorUIManager;
#endif

std::unique_ptr<Scene> g_scene;
std::unique_ptr<TextureManager> g_textureManager;
#include <mutex>
#include <vector>
std::unique_ptr<RosBridgeSystem> g_rosBridge;

std::mutex g_eventMutex;
std::vector<SDL_Event> g_eventQueue;

struct Position { float x, y; };

struct InputState {
    std::atomic<bool> w{false}, a{false}, s{false}, d{false}, q{false}, e{false};
    std::atomic<bool> mouseRightDown{false};
    std::atomic<float> mouseDeltaX{0.0f}, mouseDeltaY{0.0f};
    float yaw{0.0f}, pitch{0.0f};
} g_input;

void OnWindowEvent(const void* event) {
    const SDL_Event* sdlEvent = static_cast<const SDL_Event*>(event);

    if (sdlEvent->type == SDL_EVENT_TEXT_INPUT && sdlEvent->text.text) {
        if (g_renderer && g_renderer->getBridgeRenderer() && g_renderer->getBridgeRenderer()->getUIBridge()) {
            g_renderer->getBridgeRenderer()->getUIBridge()->injectTextInput(std::string(sdlEvent->text.text));
        }
    }

    std::lock_guard<std::mutex> lock(g_eventMutex);
    g_eventQueue.push_back(*sdlEvent);
}

void ProcessEventSync(const SDL_Event& sdlEvent) {
    if (g_renderer) {
        g_renderer->processEvent(&sdlEvent);
    }

    switch (sdlEvent.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            bool pressed = (sdlEvent.type == SDL_EVENT_KEY_DOWN);
            switch (sdlEvent.key.scancode) {
                case SDL_SCANCODE_W: g_input.w = pressed; break;
                case SDL_SCANCODE_S: g_input.s = pressed; break;
                case SDL_SCANCODE_A: g_input.a = pressed; break;
                case SDL_SCANCODE_D: g_input.d = pressed; break;
                case SDL_SCANCODE_Q: g_input.q = pressed; break;
                case SDL_SCANCODE_E: g_input.e = pressed; break;
                default: break;
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (sdlEvent.button.button == SDL_BUTTON_RIGHT) {
                bool isDown = (sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                g_input.mouseRightDown = isDown;
                if (g_window) {
                    SDL_Window* sdlWin = (SDL_Window*)g_window->getNativeHandle();
                    if (sdlWin) {
                        SDL_SetWindowRelativeMouseMode(sdlWin, isDown);
                    }
                }
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (g_input.mouseRightDown) {
                g_input.mouseDeltaX = g_input.mouseDeltaX + (float)sdlEvent.motion.xrel;
                g_input.mouseDeltaY = g_input.mouseDeltaY + (float)sdlEvent.motion.yrel;
            }
            break;
        }
        default: break;
    }
}

Status InitializeEngine(const EngineConfig& config) {
    g_ecsRegistry = std::make_unique<Registry>();

    auto entity = g_ecsRegistry->create();
    g_ecsRegistry->emplace<Position>(entity, 0.0f, 0.0f);

    g_windowThread = std::make_unique<WindowThread>();
    g_windowThread->startThread();
    NX_RETURN_IF_ERROR(g_windowThread->createWindowAsync("Nexus Engine", 1280, 720, g_window));
    g_window->setEventCallback(OnWindowEvent);

#if ENABLE_SDL
    if (g_window) {
        SDL_Window* sdlWin = (SDL_Window*)g_window->getNativeHandle();
        if (sdlWin) {
            SDL_StartTextInput(sdlWin);
        }
    }
#endif

    g_context = CreateContext(config);
    if (!g_context) return InternalError("Context creation failed");

    NX_RETURN_IF_ERROR(g_context->initialize());
    NX_RETURN_IF_ERROR(g_context->initializeWindowSurface(g_window->getNativeHandle()));

#if ENABLE_VULKAN
    auto vkContext = static_cast<VK_Context*>(g_context);
    g_swapchain = std::make_unique<VK_Swapchain>(vkContext->getInstance(), vkContext->getPhysicalDevice(), vkContext->getDevice(), vkContext->getSurface());
    NX_RETURN_IF_ERROR(g_swapchain->initialize(1280, 720));

    g_renderer = std::make_unique<Core::RenderSystem>(vkContext, g_swapchain.get());
    NX_RETURN_IF_ERROR(g_renderer->initialize());

    g_rhiThread = std::make_unique<RHIThread>(vkContext);
    g_rhiThread->setRenderer(g_renderer.get());
    g_rhiThread->startThread();

    g_editorUIManager = std::make_unique<EditorUIManager>();
    g_editorUIManager->initialize(g_renderer->getBridgeRenderer()->getUIBridge());
    g_editorUIManager->loadLayout("Data/UI/editor_layout.json");

    g_textureManager = std::make_unique<TextureManager>(vkContext);
#endif

    g_scene = std::make_unique<Scene>("MainScene");
    SceneSerializer deserializer(*g_scene);
    if (!deserializer.deserialize("Data/main_scene.bin")) {
        NX_CORE_INFO("No existing scene found, creating default entity.");
        Entity camera = g_scene->createEntity("MainCamera");
        camera.getComponent<TransformComponent>().position = {0.0f, 0.0f, 2.0f};
        camera.addComponent<CameraComponent>();

#if ENABLE_VULKAN
        if (g_renderer && g_textureManager) {
            Entity droneRoot = ModelLoader::loadModel(g_textureManager.get(), g_scene.get(), g_renderer->getMeshManager(), "Data/Models/drone.glb");
            if (droneRoot.isValid()) {
                auto& transform = droneRoot.getComponent<TransformComponent>();
                transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
            }
        }
#endif
    } else {
#if ENABLE_VULKAN
        if (g_renderer && g_textureManager) {
            Scene dummyScene("Preload");
            ModelLoader::loadModel(g_textureManager.get(), &dummyScene, g_renderer->getMeshManager(), "Data/Models/drone.glb");
        }
#endif

        bool cameraPosFixed = false;
        auto& registry = g_scene->getRegistry();
        auto cameraView = registry.view<CameraComponent>();
        if (cameraView.begin() == cameraView.end()) {
            Entity camera = g_scene->createEntity("MainCamera");
            camera.getComponent<TransformComponent>().position = {0.0f, 0.0f, 2.0f};
            camera.addComponent<CameraComponent>();
            cameraPosFixed = true;
        } else {
            for (auto entity : cameraView) {
                if (registry.has<TransformComponent>(entity)) {
                    auto& transform = registry.get<TransformComponent>(entity);
                    if (transform.position[2] < 1.0f) {
                        transform.position = {0.0f, 0.0f, 2.0f};
                        cameraPosFixed = true;
                    }
                }
                break;
            }
        }
        if (cameraPosFixed) {
            NX_CORE_INFO("Camera position forced to (0,0,2) to ensure viewport visibility.");
        }

        auto boxView = registry.view<TagComponent>();
        for (auto entity : boxView) {
            Entity e(entity, &registry);
            if (e.getComponent<TagComponent>().name == "DefaultBox") {
                bool needsMesh = !e.hasComponent<MeshComponent>() || e.getComponent<MeshComponent>().indexCount == 0;
                if (needsMesh) {
                    if (e.hasComponent<MeshComponent>()) {
                        registry.remove<MeshComponent>(entity);
                    }
                    e.addComponent<MeshComponent>(g_renderer->getCubeMeshComponent());
                    NX_CORE_INFO("Restored MeshComponent for DefaultBox (indices fixed)");
                }
                break;
            }
        }
    }

    g_physicsSystem = new PhysicsSystem();
    auto physicsStatus = g_physicsSystem->initialize();
    if (!physicsStatus.ok()) {
        NX_CORE_WARN("Physics system unavailable: {}", physicsStatus.message());
        delete g_physicsSystem;
        g_physicsSystem = nullptr;
    } else {
        auto loadStatus = g_physicsSystem->loadModel("Data/Scenes/drone.xml");
        if (!loadStatus.ok()) {
            NX_CORE_WARN("Failed to load drone model: {}", loadStatus.message());
        }
        g_physicsThread = std::make_unique<PhysicsThread>(g_physicsSystem);
        g_physicsThread->startThread();
    }

    g_rosBridge = std::make_unique<RosBridgeSystem>();
    if (auto status = g_rosBridge->initialize(); !status.ok()) {
        NX_CORE_WARN("Failed to start ROS Bridge: {}", status.message());
    }

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

    if (g_rosBridge) {
        g_rosBridge->shutdown();
        g_rosBridge.reset();
    }

#if ENABLE_VULKAN
    if (g_editorUIManager) {
        g_editorUIManager->saveLayout("Data/UI/editor_layout.json");
    }
    if (g_scene) {
        SceneSerializer serializer(*g_scene);
        serializer.serialize("Data/main_scene.bin");
        g_scene.reset();
    }
    g_editorUIManager.reset();
    g_renderer.reset();
    g_swapchain.reset();
    g_textureManager.reset();
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

    std::vector<SDL_Event> localEvents;
    while (!g_quit) {

        {
            std::lock_guard<std::mutex> lock(g_eventMutex);
            localEvents = std::move(g_eventQueue);
            g_eventQueue.clear();
        }

        for (const auto& ev : localEvents) {
            ProcessEventSync(ev);
        }

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

        if (g_scene) {
            auto& registry = g_scene->getRegistry();
            auto view = registry.view<CameraComponent, TransformComponent>();
            float speed = 0.05f;
            float sensitivity = 0.005f;

            for (auto entity : view) {
                auto& transform = registry.get<TransformComponent>(entity);
                auto& camera = registry.get<CameraComponent>(entity);

                if (g_input.mouseRightDown) {
                    float dx = g_input.mouseDeltaX.exchange(0.0f);
                    float dy = g_input.mouseDeltaY.exchange(0.0f);

                    g_input.yaw += dx * sensitivity;
                    g_input.pitch += dy * sensitivity;

                    const float pitchLimit = 89.0f * (3.14159265f / 180.0f);
                    if (g_input.pitch > pitchLimit) g_input.pitch = pitchLimit;
                    if (g_input.pitch < -pitchLimit) g_input.pitch = -pitchLimit;
                } else {
                    g_input.mouseDeltaX.store(0.0f);
                    g_input.mouseDeltaY.store(0.0f);
                }

                float sf = std::sin(g_input.yaw);
                float cf = std::cos(g_input.yaw);
                float st = std::sin(g_input.pitch);
                float ct = std::cos(g_input.pitch);

                float forwardX = ct * sf;
                float forwardY = -st;
                float forwardZ = -ct * cf;

                float fLen = std::sqrt(forwardX*forwardX + forwardY*forwardY + forwardZ*forwardZ);
                if (fLen > 0.0001f) {
                    forwardX /= fLen; forwardY /= fLen; forwardZ /= fLen;
                }

                float rightX = forwardY * 0.0f - forwardZ * 1.0f;
                float rightY = forwardZ * 0.0f - forwardX * 0.0f;
                float rightZ = forwardX * 1.0f - forwardY * 0.0f;

                float rLen = std::sqrt(rightX*rightX + rightY*rightY + rightZ*rightZ);
                if (rLen > 0.0001f) {
                    rightX /= rLen; rightY /= rLen; rightZ /= rLen;
                }

                float upX = rightY * forwardZ - rightZ * forwardY;
                float upY = rightZ * forwardX - rightX * forwardZ;
                float upZ = rightX * forwardY - rightY * forwardX;

                camera.target[0] = transform.position[0] + forwardX;
                camera.target[1] = transform.position[1] + forwardY;
                camera.target[2] = transform.position[2] + forwardZ;

                camera.up[0] = upX;
                camera.up[1] = upY;
                camera.up[2] = upZ;

                if (g_input.mouseRightDown && (g_input.w || g_input.s || g_input.a || g_input.d || g_input.q || g_input.e)) {

                    if (g_input.w) {
                        transform.position[0] += forwardX * speed;
                        transform.position[1] += forwardY * speed;
                        transform.position[2] += forwardZ * speed;
                    }
                    if (g_input.s) {
                        transform.position[0] -= forwardX * speed;
                        transform.position[1] -= forwardY * speed;
                        transform.position[2] -= forwardZ * speed;
                    }
                    if (g_input.a) {
                        transform.position[0] -= rightX * speed;
                        transform.position[1] -= rightY * speed;
                        transform.position[2] -= rightZ * speed;
                    }
                    if (g_input.d) {
                        transform.position[0] += rightX * speed;
                        transform.position[1] += rightY * speed;
                        transform.position[2] += rightZ * speed;
                    }
                    if (g_input.q) {
                        transform.position[0] -= upX * speed;
                        transform.position[1] -= upY * speed;
                        transform.position[2] -= upZ * speed;
                    }
                    if (g_input.e) {
                        transform.position[0] += upX * speed;
                        transform.position[1] += upY * speed;
                        transform.position[2] += upZ * speed;
                    }

                    camera.target[0] = transform.position[0] + forwardX;
                    camera.target[1] = transform.position[1] + forwardY;
                    camera.target[2] = transform.position[2] + forwardZ;
                }
                break;
            }
        }

#if ENABLE_VULKAN
        if (g_rhiThread) {
            g_rhiThread->requestSync();
            if (g_scene) {
                RoboticsDynamicsSystem::update(g_scene->getRegistry(), g_physicsSystem);
                HierarchySystem::update(g_scene->getRegistry());
                if (g_rosBridge) {
                    g_rosBridge->publishReplicas(g_scene->getRegistry());
                }
            }
            g_rhiThread->resumeSync();

            RenderCommand cmd;
            cmd.type = RenderCommandType::Draw;
            cmd.registry = g_scene ? &g_scene->getRegistry() : nullptr;
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

    EngineConfig config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-validation") {
            config.enableValidationLayers = false;
        }
    }

    if (auto status = ResourceLoader::initialize(); !status.ok()) {
        Log::warn("ResourceLoader failed to detect base path: {}", status.message());
    }

    if (auto status = InitializeEngine(config); !status.ok()) {
        Log::critical("Engine init failed: {}", status.message());
        return -1;
    }

    RunMainLoop();
    Log::info("Nexus Engine Shutting down...");
    ShutdownEngine();

    return 0;
}
