#include "Context.h"
#include "Threading.h"
#include "ECS.h"
#include "Entity.h"
#include "Log.h"
#include "PhysicsThread.h"
#include "ResourceLoader.h"
#include "MuJoCo/MuJoCo_PhysicsSystem.h"
#include <cmath>
#include <filesystem>

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
#include "Core/SceneLoader.h"
#include "Core/ModelLoader.h"
#include "Core/TextureManager.h"
#include "Core/Cesium3DTilesetSystem.h"
#include "Core/CesiumComponents.h"
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/Ellipsoid.h>


#if defined(_MSC_VER)
extern "C" {
    #include <string.h>
    #include <stdlib.h>
    int (__cdecl *__imp_stricmp)(const char *, const char *) = _stricmp;
    errno_t (__cdecl *__imp__putenv_s)(const char *, const char *) = _putenv_s;
}
#endif

using namespace Nexus;
using namespace Nexus::Core;

std::string g_sceneOverridePath;

namespace Nexus {
    extern std::atomic<float> g_RenderStats_FPS;
    extern std::atomic<float> g_RenderStats_FrameTime;
    extern std::atomic<float> g_RenderStats_LogicTime;
    extern std::atomic<float> g_RenderStats_RenderSyncTime;
    extern std::atomic<float> g_RenderStats_RenderPrepTime;
}

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
#include <array>
#include <atomic>
std::unique_ptr<RosBridgeSystem> g_rosBridge;

template<typename T, size_t Capacity>
class SPSCQueue {
public:
    bool push(const T& item) {
        size_t h = m_head.load(std::memory_order_relaxed);
        size_t next = (h + 1) % Capacity;
        if (next == m_tail.load(std::memory_order_acquire)) return false;
        m_buffer[h] = item;
        m_head.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& item) {
        size_t t = m_tail.load(std::memory_order_relaxed);
        if (t == m_head.load(std::memory_order_acquire)) return false;
        item = m_buffer[t];
        m_tail.store((t + 1) % Capacity, std::memory_order_release);
        return true;
    }
private:
    std::array<T, Capacity> m_buffer;
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};
};

SPSCQueue<SDL_Event, 1024> g_eventQueue;

static constexpr size_t TEXT_SIDE_BUF_COUNT = 64;
static char g_textSideBuf[TEXT_SIDE_BUF_COUNT][64];
static std::atomic<size_t> g_textSideBufIdx{0};

struct Position { float x, y; };

struct InputState {
    std::atomic<bool> w{false}, a{false}, s{false}, d{false}, q{false}, e{false};
    std::atomic<bool> mouseRightDown{false};
    std::atomic<float> mouseDeltaX{0.0f}, mouseDeltaY{0.0f};
    float yaw{0.0f}, pitch{0.0f};
    float cameraSpeedMultiplier{1.0f};
} g_input;

void OnWindowEvent(const void* event) {
    const SDL_Event* sdlEvent = static_cast<const SDL_Event*>(event);

    SDL_Event copy = *sdlEvent;

    if (copy.type == SDL_EVENT_TEXT_INPUT && copy.text.text) {
        size_t idx = g_textSideBufIdx.fetch_add(1, std::memory_order_relaxed) % TEXT_SIDE_BUF_COUNT;
        strncpy(g_textSideBuf[idx], copy.text.text, 63);
        g_textSideBuf[idx][63] = '\0';
        copy.text.text = g_textSideBuf[idx];
    }

    g_eventQueue.push(copy);
}

void ProcessEventSync(const SDL_Event& sdlEvent) {
#ifdef ENABLE_RMLUI
    if (g_renderer && g_renderer->getBridgeRenderer()->getUIBridge()) {
        g_renderer->getBridgeRenderer()->getUIBridge()->processSdlEvent(sdlEvent);
    }
#endif

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
        case SDL_EVENT_MOUSE_WHEEL: {
            float scrollAmt = sdlEvent.wheel.y;
            g_input.cameraSpeedMultiplier *= (1.0f + scrollAmt * 0.1f);
            if (g_input.cameraSpeedMultiplier < 0.01f) g_input.cameraSpeedMultiplier = 0.01f;
            if (g_input.cameraSpeedMultiplier > 10000000.0f) g_input.cameraSpeedMultiplier = 10000000.0f;
            break;
        }
        default: break;
    }
}

Status InitializeEngine(const EngineConfig& config, bool onlineMode) {
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

    NX_CORE_INFO("Main: Initializing TextureManager");
    g_textureManager = std::make_unique<TextureManager>(vkContext);
    g_textureManager->setRenderer(g_renderer.get());
#endif

    std::string sceneRelPath = g_sceneOverridePath.empty() ? "Data/Scenes/default_scene.json" : g_sceneOverridePath;
    std::string scenePath = ResourceLoader::getBasePath() + sceneRelPath;
    auto configResult = SceneLoader::parseSceneFile(scenePath);
    if (!configResult.ok()) {
        NX_CORE_WARN("场景文件加载失败: {}，使用默认配置", configResult.status().message());
    }
    auto sceneConfig = configResult.ok() ? configResult.value() : SceneLoader::SceneConfig{};

    g_scene = std::make_unique<Scene>(sceneConfig.sceneName);

#if ENABLE_VULKAN
    NX_CORE_INFO("Main: Calling SceneLoader::createEntities");
    (void)SceneLoader::createEntities(sceneConfig, g_scene.get(), g_renderer.get(), g_textureManager.get());

    NX_CORE_INFO("Main: Calling Cesium3DTilesetSystem::initialize");
    std::string cesiumCachePath = ResourceLoader::getBasePath() + ".cache/cesium";
    Cesium3DTilesetSystem::initialize(g_scene.get(), g_context, g_textureManager.get(), cesiumCachePath, onlineMode);

    NX_CORE_INFO("Main: Creating Cesium Test Entity");
    Entity cesiumEnt = g_scene->createEntity("Cesium_Test_Tileset");
    auto& georef = cesiumEnt.addComponent<CesiumGeoreference>();
    georef.m_longitude = 121.5;
    georef.m_latitude = 25.0;
    georef.m_height = 50.0;

    auto cameraView = g_scene->getRegistry().view<CameraComponent, TransformComponent>();
    for (auto c : cameraView) {
        auto& cam = cameraView.get<CameraComponent>(c);
    }

    auto& tileset = cesiumEnt.addComponent<Cesium3DTileset>();
    tileset.m_ionAssetId = 2275207;
    const char* ionToken = getenv("CESIUM_ION_TOKEN");
    if (ionToken) {
        tileset.m_ionAccessToken = ionToken;
    } else {
        tileset.m_ionAccessToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiI4YzFmM2RhZC0wZTM1LTQwNTgtOGJhNy00YzQ3MDAyN2IxNjQiLCJpZCI6NDEyMDc2LCJpYXQiOjE3NzUwMTI2ODl9.JFwAvTfQCe8wvmczvyfqbP8BvwxjZ09WLjZpF7U4rhU";
        NX_CORE_WARN("CESIUM_ION_TOKEN not set in environment, using fallback token.");
    }

    entt::entity vsEnt = entt::null;
    auto view = g_scene->getRegistry().view<TagComponent>();
    for (auto e : view) {
        if (view.get<TagComponent>(e).name == "front_camera" || view.get<TagComponent>(e).name == "front_camera_link") {
            vsEnt = e;
            break;
        }
    }
    if (vsEnt == entt::null) {
        NX_CORE_WARN("Vision Sensor: 'front_camera' entity not found! Creating fallback entity.");
        Entity fallbackEnt = g_scene->createEntity("VisionSensor");
        fallbackEnt.addComponent<RigidBodyComponent>("front_camera");
        vsEnt = (entt::entity)fallbackEnt;
    }

    if (!g_scene->getRegistry().has<TransformComponent>(vsEnt)) {
        g_scene->getRegistry().emplace<TransformComponent>(vsEnt);
    }

    if (!g_scene->getRegistry().has<CameraComponent>(vsEnt)) {
        g_scene->getRegistry().emplace<CameraComponent>(vsEnt);
    }
    auto& vsCam = g_scene->getRegistry().get<CameraComponent>(vsEnt);
    vsCam.fov = 60.0f;

    g_renderer->getBridgeRenderer()->setVisionSensorCamera(vsEnt);

#else
    SceneLoader::createEntities(sceneConfig, g_scene.get(), nullptr, nullptr);
#endif

    NX_CORE_INFO("Main: Init Physics System");
    std::string physicsPath;
    g_physicsSystem = new PhysicsSystem();
    auto physicsStatus = g_physicsSystem->initialize();
    if (!physicsStatus.ok()) {
        NX_CORE_WARN("Physics system unavailable: {}", physicsStatus.message());
        delete g_physicsSystem;
        g_physicsSystem = nullptr;
    } else {
        physicsPath = sceneConfig.robotPhysics.empty() ? "Data/Scenes/go2_mujoco/scene.xml" : "Data/" + sceneConfig.robotPhysics;
        NX_CORE_INFO("Main: Loading Physics model {}", physicsPath);
        auto loadStatus = g_physicsSystem->loadModel(physicsPath);
        if (!loadStatus.ok()) {
            NX_CORE_WARN("Failed to load drone model: {}", loadStatus.message());
        }
        g_physicsThread = std::make_unique<PhysicsThread>(g_physicsSystem, 1000.0f);
        NX_CORE_INFO("Main: Starting PhysicsThread");
        g_physicsThread->startThread();
    }

    NX_CORE_INFO("Main: Starting ROS Bridge");
    g_rosBridge = std::make_unique<RosBridgeSystem>();
    if (auto status = g_rosBridge->initialize(); !status.ok()) {
        NX_CORE_WARN("Failed to start ROS Bridge: {}", status.message());
    }

    if (g_rosBridge && g_physicsSystem) {
        std::string modelDir = physicsPath;
        auto slashPos = modelDir.find_last_of("/\\");
        if (slashPos != std::string::npos) modelDir = modelDir.substr(0, slashPos);
        auto dirSlash = modelDir.find_last_of("/\\");
        std::string folderName = (dirSlash != std::string::npos) ? modelDir.substr(dirSlash + 1) : modelDir;

        std::string robotName = folderName;
        auto underscorePos = robotName.find('_');
        if (underscorePos != std::string::npos) {
            robotName = "unitree_" + robotName.substr(0, underscorePos);
        }

        g_rosBridge->setRobotInfo(folderName + "_0", robotName);
        g_rosBridge->setPhysicsSystem(g_physicsSystem);

        auto* mjPhys = dynamic_cast<MuJoCo_PhysicsSystem*>(g_physicsSystem);
        if (mjPhys) {
            auto* bridge = g_rosBridge.get();
            mjPhys->setStateCallback([bridge](mjModel* m, mjData* d) {
                bridge->publishPhysicsState(m, d);
            }, 7);
            NX_CORE_INFO("已注册物理线程高频状态发布回调 (decimation=7, ~{:.0f}Hz)", 1.0 / (7 * 0.003));
        }
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

void buildSnapshotFromRegistry(Registry& registry, RenderSnapshot* snapshot) {
    auto cameraView = registry.view<CameraComponent, TransformComponent>();

    std::array<float, 16> viewProj = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };

    uint32_t width = 1280, height = 720;
    if (g_window) {
        width = static_cast<SDL_Window_Wrapper*>(g_window)->getWidth();
        height = static_cast<SDL_Window_Wrapper*>(g_window)->getHeight();
    }

    bool mainCameraProcessed = false;
    for (auto entity : cameraView) {
        auto& camera = cameraView.get<CameraComponent>(entity);
        auto& transform = cameraView.get<TransformComponent>(entity);

        bool isVisionSensor = false;
        if (registry.has<TagComponent>(entity)) {
            std::string name = registry.get<TagComponent>(entity).name;
            if (name == "VisionSensor" || name == "front_camera" || name == "front_camera_link") {
                isVisionSensor = true;
            }
        }

        if (!isVisionSensor && mainCameraProcessed) {
            continue;
        }

        if (isVisionSensor) {
            camera.aspect = 640.0f / 480.0f;
        } else {
            camera.aspect = (float)width / ((float)height + 0.0001f);
        }
        auto proj = camera.computeProjectionMatrix();

        float pos[3];
        if (isVisionSensor) {
            pos[0] = transform.worldMatrix[12];
            pos[1] = transform.worldMatrix[13];
            pos[2] = transform.worldMatrix[14];
        } else {
            pos[0] = transform.worldMatrix[12];
            pos[1] = transform.worldMatrix[13];
            pos[2] = transform.worldMatrix[14];
        }

        float target[3] = { camera.target[0], camera.target[1], camera.target[2] };
        float upVector[3] = { 0.0f, 0.0f, 1.0f };
        if (!isVisionSensor) {
            upVector[0] = camera.up[0]; upVector[1] = camera.up[1]; upVector[2] = camera.up[2];
        } else {
             float fwd_x = transform.worldMatrix[0];
             float fwd_y = transform.worldMatrix[1];
             float fwd_z = transform.worldMatrix[2];

             target[0] = pos[0] + fwd_x;
             target[1] = pos[1] + fwd_y;
             target[2] = pos[2] + fwd_z;

             upVector[0] = transform.worldMatrix[8];
             upVector[1] = transform.worldMatrix[9];
             upVector[2] = transform.worldMatrix[10];
        }

        float fwd[3] = { target[0] - pos[0], target[1] - pos[1], target[2] - pos[2] };
        float fLen = std::sqrt(fwd[0]*fwd[0] + fwd[1]*fwd[1] + fwd[2]*fwd[2]);
        if (fLen > 1e-5f) { fwd[0]/=fLen; fwd[1]/=fLen; fwd[2]/=fLen; }
        else { fwd[0]=0; fwd[1]=0; fwd[2]=-1; }

        float right[3] = {
            fwd[1]*upVector[2] - fwd[2]*upVector[1],
            fwd[2]*upVector[0] - fwd[0]*upVector[2],
            fwd[0]*upVector[1] - fwd[1]*upVector[0]
        };
        float rLen = std::sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
        if (rLen > 1e-5f) { right[0]/=rLen; right[1]/=rLen; right[2]/=rLen; }
        else { right[0]=1; right[1]=0; right[2]=0; }

        float up[3] = {
            right[1]*fwd[2] - right[2]*fwd[1],
            right[2]*fwd[0] - right[0]*fwd[2],
            right[0]*fwd[1] - right[1]*fwd[0]
        };

        std::array<float, 16> view = {
            right[0], up[0], -fwd[0], 0.0f,
            right[1], up[1], -fwd[1], 0.0f,
            right[2], up[2], -fwd[2], 0.0f,
            -(right[0]*pos[0] + right[1]*pos[1] + right[2]*pos[2]),
            -(up[0]*pos[0] + up[1]*pos[1] + up[2]*pos[2]),
            fwd[0]*pos[0] + fwd[1]*pos[1] + fwd[2]*pos[2],
            1.0f
        };

        if (isVisionSensor) {
            snapshot->visionSensorViewProj = multiplyMat4(proj, view);
            snapshot->visionSensorValid = true;
        } else {
            viewProj = multiplyMat4(proj, view);
            mainCameraProcessed = true;
        }
    }

    auto meshView = registry.view<MeshComponent, TransformComponent>();

    std::map<std::pair<IBuffer*, IBuffer*>, size_t> batchMap;

    IBuffer* globalVb = g_context->getGlobalVertexBuffer();
    IBuffer* globalIb = g_context->getGlobalIndexBuffer();

    uint32_t selectedId = 0xFFFFFFFF;
#if ENABLE_VULKAN
    if (g_renderer) {
        selectedId = g_renderer->getBridgeRenderer()->m_selectedEntityId.load(std::memory_order_relaxed);
    }
#endif

    for (auto entity : meshView) {
        auto& mesh = meshView.get<MeshComponent>(entity);
        auto& transform = meshView.get<TransformComponent>(entity);

        if (transform.worldMatrix[0] == 0.0f && transform.worldMatrix[5] == 0.0f && transform.worldMatrix[10] == 0.0f) {
            continue;
        }

        std::array<float, 16> mvp = multiplyMat4(viewProj, transform.worldMatrix);

        IBuffer* currentVb = mesh.vertexBuffer ? mesh.vertexBuffer : globalVb;
        IBuffer* currentIb = mesh.indexBuffer ? mesh.indexBuffer : globalIb;
        if (!currentVb || !currentIb) continue;

        ObjectData obj = {};
        obj.textureIndex = mesh.albedoTexture;
        obj.samplerIndex = mesh.samplerIndex;
        obj.albedoFactor = mesh.albedoFactor;
        obj.metallicFactor = mesh.metallicFactor;
        obj.roughnessFactor = mesh.roughnessFactor;
        obj.mvp = mvp;
        obj.worldMatrix = transform.worldMatrix;

        bool isSelected = ((uint32_t)entity == selectedId);
        if (isSelected) {
            obj.highlightColor = {1.0f, 0.6f, 0.1f, 0.35f};
        } else {
            obj.highlightColor = {0.0f, 0.0f, 0.0f, 0.0f};
        }

        uint32_t entityIdx = (uint32_t)snapshot->frameObjects.size();
        snapshot->frameObjects.push_back(obj);

        DrawIndexedIndirectCommand cmd;
        cmd.indexCount = mesh.indexCount;
        cmd.instanceCount = 1;
        cmd.firstIndex = mesh.indexOffset;
        cmd.vertexOffset = mesh.vertexOffset;
        cmd.firstInstance = entityIdx;

        RenderDrawBatch* targetBatch = nullptr;
        auto key = std::make_pair(currentVb, currentIb);
        auto it = batchMap.find(key);
        if (it != batchMap.end()) {
            targetBatch = &snapshot->batches[it->second];
        } else {
            size_t newIdx = snapshot->batches.size();
            snapshot->batches.push_back({currentVb, currentIb, {}});
            targetBatch = &snapshot->batches.back();
            batchMap[key] = newIdx;
        }
        targetBatch->commands.push_back(cmd);

        snapshot->totalTriangles += mesh.indexCount / 3;
        snapshot->meshCount++;
    }
}

void RunMainLoop() {
    uint32_t lastWidth = 1280;
    uint32_t lastHeight = 720;

    double accumLogicTime = 0.0;
    double accumSyncTime = 0.0;
    double accumPrepTime = 0.0;

    std::vector<SDL_Event> localEvents;
    while (!g_quit) {
        auto logicStartTime = std::chrono::high_resolution_clock::now();

        localEvents.clear();
        {
            SDL_Event ev;
            while (g_eventQueue.pop(ev)) {
                localEvents.push_back(ev);
            }
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

        static auto lastFrameTime = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        if (g_scene) {
            static auto fpsStartTime = now;
            static int frameCount = 0;
            frameCount++;
            float elapsedFpsTime = std::chrono::duration<float>(now - fpsStartTime).count();
            if (elapsedFpsTime >= 0.5f) {
                float fps = frameCount / elapsedFpsTime;
                float frameTime = (elapsedFpsTime * 1000.0f) / frameCount;
                float avgLogic = (float)(accumLogicTime / frameCount);
                float avgSync = (float)(accumSyncTime / frameCount);
                float avgPrep = (float)(accumPrepTime / frameCount);

                g_RenderStats_FPS.store(fps, std::memory_order_relaxed);
                g_RenderStats_FrameTime.store(frameTime, std::memory_order_relaxed);
                g_RenderStats_LogicTime.store(avgLogic, std::memory_order_relaxed);
                g_RenderStats_RenderSyncTime.store(avgSync, std::memory_order_relaxed);
                g_RenderStats_RenderPrepTime.store(avgPrep, std::memory_order_relaxed);

                fpsStartTime = now;
                frameCount = 0;
                accumLogicTime = 0.0;
                accumSyncTime = 0.0;
                accumPrepTime = 0.0;
            }

            if (deltaTime > 0.1f) deltaTime = 0.1f;

            auto& registry = g_scene->getRegistry();
            auto view = registry.view<CameraComponent, TransformComponent>();
            float sensitivity = 0.5f * deltaTime;

            for (auto entity : view) {
                if (registry.has<TagComponent>(entity)) {
                    std::string name = registry.get<TagComponent>(entity).name;
                    if (name == "VisionSensor" || name == "front_camera" || name == "front_camera_link") {
                        continue;
                    }
                }

                auto& transform = registry.get<TransformComponent>(entity);
                auto& camera = registry.get<CameraComponent>(entity);

                double altitude = 5.0;
                auto tilesetView = registry.view<Cesium3DTileset, CesiumGeoreference>();
                for (auto tsEntity : tilesetView) {
                    auto& geoRef = tilesetView.get<CesiumGeoreference>(tsEntity);
                    if (geoRef.m_localCoordinateSystem) {
                        glm::dvec3 camEnuPosition(transform.position[0], -transform.position[2], transform.position[1]);
                        glm::dvec3 ecefPosition = geoRef.m_localCoordinateSystem->localPositionToEcef(camEnuPosition);
                        auto carto = CesiumGeospatial::Ellipsoid::WGS84.cartesianToCartographic(ecefPosition);
                        if (carto) {
                            altitude = carto->height;
                        }

                        break;
                    }
                }

                float safeAltitude = std::max(0.0f, static_cast<float>(altitude));
                float baseSpeed = std::clamp((safeAltitude * safeAltitude) * 0.005f, 1.0f, 150000.0f);
                float speed = baseSpeed * deltaTime * g_input.cameraSpeedMultiplier;

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

                float rightX = -forwardZ;
                float rightY = 0.0f;
                float rightZ = forwardX;

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
            auto logicEndTime = std::chrono::high_resolution_clock::now();
            accumLogicTime += std::chrono::duration<double, std::milli>(logicEndTime - logicStartTime).count();

            auto syncStartTime = std::chrono::high_resolution_clock::now();
            while (g_rhiThread->getQueueSize() >= 2) {
                std::this_thread::yield();
            }
            auto syncEndTime = std::chrono::high_resolution_clock::now();
            accumSyncTime += std::chrono::duration<double, std::milli>(syncEndTime - syncStartTime).count();

            auto prepStartTime = std::chrono::high_resolution_clock::now();
            static float lh = 0, lc = 0, lpu = 0, lu = 0;
            if (g_scene) {
                auto t1 = std::chrono::high_resolution_clock::now();
                HierarchySystem::update(g_scene->getRegistry());

                auto t2 = std::chrono::high_resolution_clock::now();
                Cesium3DTilesetSystem::update(g_scene->getRegistry(), deltaTime);

                auto t3 = std::chrono::high_resolution_clock::now();
                RoboticsDynamicsSystem::update(g_scene->getRegistry(), g_physicsSystem);
                if (g_rosBridge) {
                    if (g_physicsSystem) g_rosBridge->publishModelInfo(g_physicsSystem);
                }
                auto t4 = std::chrono::high_resolution_clock::now();

                lh += std::chrono::duration<float, std::milli>(t2 - t1).count();
                lc += std::chrono::duration<float, std::milli>(t3 - t2).count();
                lpu += std::chrono::duration<float, std::milli>(t4 - t3).count();
            }

            auto t5 = std::chrono::high_resolution_clock::now();
            auto* uiBridge = (g_renderer) ? g_renderer->getBridgeRenderer()->getUIBridge() : nullptr;
            if (uiBridge) {
                if (uiBridge->tryLockUI()) {
                    if (g_editorUIManager) {
                        g_editorUIManager->update(g_scene.get());
                    }
                    uiBridge->updateUI();
                    uiBridge->unlockUI();
                }
            }
            auto prepEndTime = std::chrono::high_resolution_clock::now();
            lu += std::chrono::duration<float, std::milli>(prepEndTime - t5).count();

            static int p_prep = 0;
            if (++p_prep >= 60) {
                NX_CORE_INFO("Logic Profile: Hier={:.2f}ms, Cesium={:.2f}ms, Phys={:.2f}ms, UIMgr={:.2f}ms", lh/60.0f, lc/60.0f, lpu/60.0f, lu/60.0f);
                lh = 0; lc = 0; lpu = 0; lu = 0; p_prep = 0;
            }

            if (g_textureManager) {
                g_textureManager->performGarbageCollection();
            }

            accumPrepTime += std::chrono::duration<double, std::milli>(prepEndTime - prepStartTime).count();

            static RenderSnapshot sm_snapshots[4];
            static uint32_t sm_currentSnapshot = 0;

            RenderSnapshot* activeSnapshot = &sm_snapshots[sm_currentSnapshot];
            activeSnapshot->clear();
            if (g_scene) {
                buildSnapshotFromRegistry(g_scene->getRegistry(), activeSnapshot);
            }

            RenderCommand cmd;
            cmd.type = RenderCommandType::Draw;
            cmd.snapshot = activeSnapshot;
            g_rhiThread->pushCommand(cmd);

            sm_currentSnapshot = (sm_currentSnapshot + 1) % 4;

            std::vector<uint8_t> pixels;
            if (g_renderer && g_renderer->getBridgeRenderer()->getOffscreenPixels(pixels)) {
                if (g_rosBridge) g_rosBridge->publishImage(pixels, 640, 480);
            }
        }
#else
        g_context->sync();
#endif

        std::this_thread::yield();
    }
}
} // namespace

int main(int argc, char* argv[]) {
    Log::init();
    Log::info("Nexus Engine Starting...");

    EngineConfig config;
    std::string sceneArg;
    bool onlineMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--validation") {
            config.enableValidationLayers = true;
            config.forceValidation = true;
        } else if (arg == "--no-validation") {
            config.enableValidationLayers = false;
        } else if (arg == "--online") {
            onlineMode = true;
        } else if (arg == "--scene" && i + 1 < argc) {
            sceneArg = argv[++i];
        }
    }

    if (auto status = ResourceLoader::initialize(); !status.ok()) {
        Log::warn("ResourceLoader failed to detect base path: {}", status.message());
    }

    {
        std::string scenesDir = ResourceLoader::getBasePath() + "Data/Scenes";
        Log::info("可用场景:");
        try {
            for (auto& entry : std::filesystem::directory_iterator(scenesDir)) {
                if (entry.path().extension() == ".json") {
                    std::string stem = entry.path().stem().string();
                    std::string displayName = stem;
                    auto pos = displayName.find("_scene");
                    if (pos != std::string::npos) displayName = displayName.substr(0, pos);
                    Log::info("  --scene {}  ({})", displayName, entry.path().filename().string());
                }
            }
        } catch (...) {}
    }

    if (!sceneArg.empty()) {
        std::string candidatePath = "Data/Scenes/" + sceneArg + "_scene.json";
        std::string fullPath = ResourceLoader::getBasePath() + candidatePath;
        if (std::filesystem::exists(fullPath)) {
            g_sceneOverridePath = candidatePath;
            Log::info("选择场景: {} ({})", sceneArg, candidatePath);
        } else {
            Log::warn("场景 '{}' 不存在 (查找: {})", sceneArg, fullPath);
        }
    }

    if (auto status = InitializeEngine(config, onlineMode); !status.ok()) {
        Log::critical("Engine init failed: {}", status.message());
        return -1;
    }

    RunMainLoop();
    Log::info("Nexus Engine Shutting down...");
    ShutdownEngine();

    return 0;
}
