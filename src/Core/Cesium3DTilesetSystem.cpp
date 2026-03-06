#include "Cesium3DTilesetSystem.h"
#include "CesiumComponents.h"
#include "Components.h"
#include "../Bridge/Log.h"
#include "RenderSystem.h"
#include "../Bridge/Vk/VK_Renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <Cesium3DTilesSelection/ViewState.h>
#include <CesiumGeometry/Transforms.h>
#include <CesiumGeospatial/Cartographic.h>

#include <spdlog/spdlog.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include "CesiumTaskProcessor.h"
#include "CesiumAssetAccessor.h"
#include "CesiumPrepareRendererResources.h"
#include <Cesium3DTilesContent/registerAllTileContentTypes.h>

using namespace Nexus;
using namespace Nexus::Core;

namespace Nexus {
namespace Core {

static Scene* g_tilesetScene = nullptr;
static Nexus::IContext* g_tilesetContext = nullptr;
static TextureManager* g_tilesetTextureManager = nullptr;
static Core::RenderSystem* g_tilesetRenderSystem = nullptr;
static std::shared_ptr<CesiumAsync::IAssetAccessor> g_assetAccessor = nullptr;
static std::shared_ptr<CesiumPrepareRendererResources> g_prepareRes = nullptr;

void Cesium3DTilesetSystem::initialize(Scene* scene, Nexus::IContext* context, TextureManager* textureManager, Core::RenderSystem* renderSystem, const std::string& cachePath, bool onlineMode) {
    g_tilesetScene = scene;
    g_tilesetContext = context;
    g_tilesetTextureManager = textureManager;
    g_tilesetRenderSystem = renderSystem;
    auto accessor = std::make_shared<CesiumAssetAccessor>();
    if (!cachePath.empty()) {
        accessor->setCachePath(cachePath);
    }
    accessor->setOfflineMode(!onlineMode);
    g_assetAccessor = accessor;

    Cesium3DTilesContent::registerAllTileContentTypes();
}

void Cesium3DTilesetSystem::update(Nexus::Registry& registry, float dt) {
    if (g_prepareRes) {
        g_prepareRes->pumpDeferredDeletion();
    }

    auto cameraView = registry.view<CameraComponent, TransformComponent>();

    struct CameraState {
        glm::dvec3 pos;
        glm::dvec3 dir;
        glm::dvec3 up;
        double aspect;
        double fovY;
    };
    std::vector<CameraState> activeCameras;

    bool mainCameraFound = false;
    for (auto entity : cameraView) {
        auto& camera = cameraView.get<CameraComponent>(entity);
        auto& camTransform = cameraView.get<TransformComponent>(entity);

        bool isVisionSensor = false;
        if (registry.has<TagComponent>(entity)) {
            std::string name = registry.get<TagComponent>(entity).name;
            if (name == "VisionSensor" || name == "front_camera" || name == "front_camera_link") {
                isVisionSensor = true;
            }
        }

        if (!isVisionSensor && mainCameraFound) {
            continue;
        }

        CameraState state;
        if (isVisionSensor) {
            state.pos = glm::dvec3(camTransform.worldMatrix[12], camTransform.worldMatrix[13], camTransform.worldMatrix[14]);

            state.dir = glm::dvec3(camTransform.worldMatrix[0], camTransform.worldMatrix[1], camTransform.worldMatrix[2]);
            if (glm::length(state.dir) > 0.0001) state.dir = glm::normalize(state.dir);
            else state.dir = glm::dvec3(1.0, 0.0, 0.0);

            state.up = glm::dvec3(camTransform.worldMatrix[8], camTransform.worldMatrix[9], camTransform.worldMatrix[10]);
            if (glm::length(state.up) > 0.0001) state.up = glm::normalize(state.up);
            else state.up = glm::dvec3(0.0, 0.0, 1.0);

            state.aspect = 640.0 / 480.0;
            state.fovY = 60.0f;
        } else {
            state.pos = glm::dvec3(camTransform.worldMatrix[12], camTransform.worldMatrix[13], camTransform.worldMatrix[14]);

            state.dir = glm::dvec3(camera.target[0] - state.pos.x,
                                   camera.target[1] - state.pos.y,
                                   camera.target[2] - state.pos.z);
            if (glm::length(state.dir) > 0.0001) state.dir = glm::normalize(state.dir);
            else state.dir = glm::dvec3(0.0, 0.0, -1.0);

            state.up = glm::dvec3(camera.up[0], camera.up[1], camera.up[2]);
            if (glm::length(state.up) > 0.0001) state.up = glm::normalize(state.up);
            else state.up = glm::dvec3(0.0, 0.0, 1.0);

            state.aspect = camera.aspect;
            if (state.aspect <= 0.01) state.aspect = 1920.0 / 1080.0;
            state.fovY = camera.fov;

            mainCameraFound = true;
        }

        activeCameras.push_back(state);
    }

    if (activeCameras.empty()) {
        return;
    }

    auto tilesetView = registry.view<Cesium3DTileset, CesiumGeoreference>();

    for (auto entity : tilesetView) {
        auto& tilesetComponent = tilesetView.get<Cesium3DTileset>(entity);
        auto& geoRef = tilesetView.get<CesiumGeoreference>(entity);

        if (!tilesetComponent.m_tileset) {
            if (tilesetComponent.m_url.empty() && tilesetComponent.m_ionAssetId <= 0) {
                continue;
            }

            if (!g_tilesetScene || !g_tilesetContext || !g_tilesetTextureManager) {
                NX_CORE_ERROR("Cesium3DTilesetSystem: Cannot initialize Tileset. Call initialize() first.");
                continue;
            }

            NX_CORE_INFO("Cesium3DTilesetSystem: Initializing Tileset...");

            g_prepareRes = std::make_shared<CesiumPrepareRendererResources>(g_tilesetScene, g_tilesetContext, g_tilesetTextureManager, g_tilesetRenderSystem);

            Cesium3DTilesSelection::TilesetExternals externals{
                g_assetAccessor,
                g_prepareRes,
                CesiumAsync::AsyncSystem(std::make_shared<CesiumTaskProcessor>()),
                nullptr,
                spdlog::default_logger()
            };

            Cesium3DTilesSelection::TilesetOptions options;
            options.maximumCachedBytes = 4ULL * 1024 * 1024 * 1024;
            options.maximumSimultaneousTileLoads = 64;
            options.preloadAncestors = true;
            options.preloadSiblings = true;

            if (!tilesetComponent.m_url.empty()) {
                tilesetComponent.m_tileset = std::make_unique<Cesium3DTilesSelection::Tileset>(
                    externals, tilesetComponent.m_url, options);
            } else {
                tilesetComponent.m_tileset = std::make_unique<Cesium3DTilesSelection::Tileset>(
                    externals, tilesetComponent.m_ionAssetId, tilesetComponent.m_ionAccessToken, options);
            }
        }

        if (!geoRef.m_localCoordinateSystem) {
            geoRef.m_localCoordinateSystem.emplace(
                CesiumGeospatial::LocalHorizontalCoordinateSystem(
                    CesiumGeospatial::Cartographic::fromDegrees(
                        geoRef.m_longitude,
                        geoRef.m_latitude,
                        geoRef.m_height
                    )
                )
            );
        }

        std::vector<Cesium3DTilesSelection::ViewState> frustums;
        for (const auto& cam : activeCameras) {
            glm::dvec3 camEnuPosition(cam.pos.x, -cam.pos.z, cam.pos.y);
            glm::dvec3 camEnuDirection(cam.dir.x, -cam.dir.z, cam.dir.y);
            glm::dvec3 camEnuUp(cam.up.x, -cam.up.z, cam.up.y);

            glm::dvec3 ecefPosition = geoRef.m_localCoordinateSystem->localPositionToEcef(camEnuPosition);
            glm::dvec3 ecefDirection = geoRef.m_localCoordinateSystem->localDirectionToEcef(camEnuDirection);
            glm::dvec3 ecefUp = geoRef.m_localCoordinateSystem->localDirectionToEcef(camEnuUp);

            double baseHeight = 1080.0;
            double baseWidth = baseHeight * cam.aspect;

            Cesium3DTilesSelection::ViewState viewState = Cesium3DTilesSelection::ViewState::create(
                ecefPosition,
                ecefDirection,
                ecefUp,
                glm::dvec2(baseWidth, baseHeight),
                2.0 * std::atan(cam.aspect * std::tan(glm::radians(static_cast<double>(cam.fovY)) / 2.0)),
                glm::radians(static_cast<double>(cam.fovY))
            );
            frustums.push_back(viewState);
        }

        if (!tilesetComponent.m_suspendUpdate && !frustums.empty()) {
            const auto& result = tilesetComponent.m_tileset->updateView(frustums, dt);

            static float debugPrintTimer = 0.0f;
            debugPrintTimer += dt;
            bool shouldLogCesium = (debugPrintTimer > 2.0f);
            if (shouldLogCesium) {
                debugPrintTimer = 0.0f;
            }

            size_t visibleCount = result.tilesToRenderThisFrame.size();
            size_t tilesNotDone = 0, tilesNoContent = 0, tilesNoResources = 0, tilesRendered = 0;


            glm::dmat4 ecefToEnu = geoRef.m_localCoordinateSystem->getEcefToLocalTransformation();
            glm::dmat4 enuToYUp(
                glm::dvec4(1.0,  0.0,  0.0, 0.0),
                glm::dvec4(0.0,  0.0, -1.0, 0.0),
                glm::dvec4(0.0,  1.0,  0.0, 0.0),
                glm::dvec4(0.0,  0.0,  0.0, 1.0)
            );
            glm::dmat4 ecefToLocalYUp = enuToYUp * ecefToEnu;

            if (g_prepareRes) {
                g_prepareRes->setEcefToLocalYUp(ecefToLocalYUp);
            }

            auto bridge = g_tilesetRenderSystem ? g_tilesetRenderSystem->getBridgeRenderer() : nullptr;

            if (bridge) bridge->lockPersistentData();

            auto viewMeshes = registry.view<CesiumGltfComponent, MeshComponent>();
            size_t cesiumEntityCount = 0;
            for (auto e : viewMeshes) {
                auto& mesh = viewMeshes.get<MeshComponent>(e);
                if (bridge && mesh.persistentSlot != 0xFFFFFFFF) {
                    bridge->setPersistentSlotVisibility(mesh.persistentSlot, false);
                }
                cesiumEntityCount++;
            }

            for (const auto* pTile : result.tilesToRenderThisFrame) {
                if (pTile->getState() != Cesium3DTilesSelection::TileLoadState::Done) {
                    tilesNotDone++;
                    continue;
                }

                const auto* pRenderContent = pTile->getContent().getRenderContent();
                if (!pRenderContent) {
                    tilesNoContent++;
                    continue;
                }

                auto pMainResult = pRenderContent->getRenderResources();
                if (!pMainResult) {
                    tilesNoResources++;
                    continue;
                }

                auto pRenderRes = static_cast<CesiumTileRenderResources*>(pMainResult);

                for (auto e : pRenderRes->entities) {
                    if (registry.valid(e) && registry.has<MeshComponent>(e)) {
                        auto& mesh = registry.get<MeshComponent>(e);
                        if (bridge && mesh.persistentSlot != 0xFFFFFFFF) {
                            bridge->setPersistentSlotVisibility(mesh.persistentSlot, true);
                            tilesRendered++;
                        }
                    }
                }
            }

            if (bridge) bridge->unlockPersistentData();

            if (shouldLogCesium) {
                NX_CORE_INFO("Cesium3DTilesetSystem: visible={}, cesiumEntities={}, rendered={}, notDone={}, noContent={}, noResources={}",
                    visibleCount, cesiumEntityCount, tilesRendered, tilesNotDone, tilesNoContent, tilesNoResources);
                FILE* diagFile = fopen("cesium_diag.txt", "w");
                if (diagFile) {
                    fprintf(diagFile, "visible=%zu\ncesiumEntities=%zu\nrendered=%zu\nnotDone=%zu\nnoContent=%zu\nnoResources=%zu\n",
                        visibleCount, cesiumEntityCount, tilesRendered, tilesNotDone, tilesNoContent, tilesNoResources);

                    fprintf(diagFile, "\necefToEnu col3 (translation): (%.1f, %.1f, %.1f)\n",
                        ecefToEnu[3][0], ecefToEnu[3][1], ecefToEnu[3][2]);
                    fprintf(diagFile, "camera count: %zu, main pos: (%.2f, %.2f, %.2f)\n",
                        activeCameras.size(), activeCameras[0].pos.x, activeCameras[0].pos.y, activeCameras[0].pos.z);

                    bool dumpedTile = false;
                    for (const auto* pTile2 : result.tilesToRenderThisFrame) {
                        if (dumpedTile) break;
                        if (pTile2->getState() != Cesium3DTilesSelection::TileLoadState::Done) continue;
                        const auto* pRC = pTile2->getContent().getRenderContent();
                        if (!pRC) continue;
                        auto pMR = pRC->getRenderResources();
                        if (!pMR) continue;

                        const glm::dmat4& tileXform = pTile2->getTransform();
                        fprintf(diagFile, "\nFirst visible tile getTransform():\n");
                        for (int r = 0; r < 4; r++)
                            fprintf(diagFile, "  [%.2f, %.2f, %.2f, %.2f]\n", tileXform[0][r], tileXform[1][r], tileXform[2][r], tileXform[3][r]);

                        glm::dmat4 ft = enuToYUp * ecefToEnu * tileXform;
                        fprintf(diagFile, "\nfinalTransform (enuToYUp * ecefToEnu * tileTransform):\n");
                        for (int r = 0; r < 4; r++)
                            fprintf(diagFile, "  [%.4f, %.4f, %.4f, %.4f]\n", ft[0][r], ft[1][r], ft[2][r], ft[3][r]);
                        fprintf(diagFile, "  => pos = (%.2f, %.2f, %.2f)\n", ft[3][0], ft[3][1], ft[3][2]);

                        dumpedTile = true;
                    }

                    auto diagView = registry.view<CesiumGltfComponent, TransformComponent, MeshComponent>();
                    int printed = 0;
                    for (auto e : diagView) {
                        auto& t = diagView.get<TransformComponent>(e);
                        auto& m = diagView.get<MeshComponent>(e);
                        if (t.worldMatrix[12] < 999999.0f) {
                            fprintf(diagFile, "\nENTITY entity=%u worldPos=(%.2f, %.2f, %.2f) indexCount=%u\n",
                                (unsigned)e, t.worldMatrix[12], t.worldMatrix[13], t.worldMatrix[14], m.indexCount);
                            if (++printed >= 3) break;
                        }
                    }
                    fclose(diagFile);
                }
            }
        }
    }
}

} // namespace Core
} // namespace Nexus
