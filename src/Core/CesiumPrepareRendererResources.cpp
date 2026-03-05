#include "CesiumPrepareRendererResources.h"
#include "CesiumTileRenderResources.h"
#include "TextureManager.h"
#include "Scene.h"
#include "Components.h"
#include "CesiumComponents.h"
#include "RenderSystem.h"
#include "MeshManager.h"
#include "../Bridge/Log.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/ExtensionKhrMaterialsUnlit.h>
#include <CesiumGltfContent/GltfUtilities.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Nexus;
using namespace CesiumGltf;

namespace {

struct ParsedPrimitive {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    std::string textureKey;
    std::array<float, 4> baseColorFactor = {1, 1, 1, 1};
    glm::dmat4 nodeTransform = glm::dmat4(1.0);
};

struct ParsedImage {
    std::string textureKey;
    Nexus::ImageData data;
};

struct ParsedTile {
    std::vector<ParsedPrimitive> primitives;
    std::vector<ParsedImage> images;
    glm::dmat4 tileTransform;
};

static std::atomic<uint64_t> s_cesiumModelCounter{1};

void parsePrimitive(const CesiumGltf::Model& model, const CesiumGltf::MeshPrimitive& primitive, ParsedPrimitive& out, const glm::dmat4& nodeTransform, uint64_t modelInstanceId) {
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end()) return;

    CesiumGltf::AccessorView<glm::vec3> posView(model, posIt->second);
    if (posView.status() != CesiumGltf::AccessorViewStatus::Valid) {
        NX_CORE_ERROR("Cesium: Failed to parse primitive POSITION, status={}", (int)posView.status());
        return;
    }

    int64_t vertexCount = posView.size();


    auto normIt = primitive.attributes.find("NORMAL");
    CesiumGltf::AccessorView<glm::vec3> normView;
    if (normIt != primitive.attributes.end()) {
        normView = CesiumGltf::AccessorView<glm::vec3>(model, normIt->second);
        if (normView.status() != CesiumGltf::AccessorViewStatus::Valid) {
            NX_CORE_WARN("Cesium: NORMAL invalid format, status={}", (int)normView.status());
        }
    }

    int32_t uvIndex = 0;
    if (primitive.material >= 0 && primitive.material < (int32_t)model.materials.size()) {
        const CesiumGltf::Material& material = model.materials[primitive.material];
        if (material.pbrMetallicRoughness) {
            auto& factor = material.pbrMetallicRoughness->baseColorFactor;
            out.baseColorFactor = {(float)factor[0], (float)factor[1], (float)factor[2], (float)factor[3]};

            if (material.pbrMetallicRoughness->baseColorTexture) {
                uvIndex = material.pbrMetallicRoughness->baseColorTexture->texCoord;
                int32_t texIdx = material.pbrMetallicRoughness->baseColorTexture->index;
                if (texIdx >= 0 && texIdx < (int32_t)model.textures.size()) {
                    int32_t imgIdx = model.textures[texIdx].source;
                    if (imgIdx >= 0 && imgIdx < (int32_t)model.images.size()) {
                        out.textureKey = fmt::format("C3DT_{}_{}", modelInstanceId, imgIdx);
                    }
                }
            }
        }
    }

    auto uvIt = primitive.attributes.find("TEXCOORD_" + std::to_string(uvIndex));

    CesiumGltf::AccessorView<glm::vec2> uvViewF;
    CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<uint16_t>> uvViewU16;
    CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<uint8_t>> uvViewU8;

    bool hasUvF = false, hasUvU16 = false, hasUvU8 = false;

    if (uvIt != primitive.attributes.end()) {
        uvViewF = CesiumGltf::AccessorView<glm::vec2>(model, uvIt->second);
        hasUvF = (uvViewF.status() == CesiumGltf::AccessorViewStatus::Valid);
        if (!hasUvF) {
            uvViewU16 = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<uint16_t>>(model, uvIt->second);
            hasUvU16 = (uvViewU16.status() == CesiumGltf::AccessorViewStatus::Valid);
            if (!hasUvU16) {
                uvViewU8 = CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::VEC2<uint8_t>>(model, uvIt->second);
                hasUvU8 = (uvViewU8.status() == CesiumGltf::AccessorViewStatus::Valid);
            }
        }
    }

    out.vertices.reserve(vertexCount * 8);
    for (int64_t i = 0; i < vertexCount; ++i) {
        glm::vec3 rawPos = posView[i];
        out.vertices.push_back(rawPos.x);
        out.vertices.push_back(rawPos.y);
        out.vertices.push_back(rawPos.z);

        if (hasUvF) {
            glm::vec2 uv = uvViewF[i];
            out.vertices.push_back(uv.x);
            out.vertices.push_back(uv.y);
        } else if (hasUvU16) {
            float u = (float)uvViewU16[i].value[0] / 65535.0f;
            float v = (float)uvViewU16[i].value[1] / 65535.0f;
            out.vertices.push_back(u);
            out.vertices.push_back(v);
        } else if (hasUvU8) {
            float u = (float)uvViewU8[i].value[0] / 255.0f;
            float v = (float)uvViewU8[i].value[1] / 255.0f;
            out.vertices.push_back(u);
            out.vertices.push_back(v);
        } else {
            out.vertices.push_back(0.0f);
            out.vertices.push_back(0.0f);
        }

        if (normView.status() == CesiumGltf::AccessorViewStatus::Valid) {
            glm::vec3 rawN = normView[i];
            out.vertices.push_back(rawN.x);
            out.vertices.push_back(rawN.y);
            out.vertices.push_back(rawN.z);
        } else {
            out.vertices.push_back(0.0f);
            out.vertices.push_back(1.0f);
            out.vertices.push_back(0.0f);
        }
    }

    if (primitive.indices >= 0) {
        CesiumGltf::AccessorView<uint32_t> indexView32(model, primitive.indices);
        CesiumGltf::AccessorView<uint16_t> indexView16(model, primitive.indices);
        CesiumGltf::AccessorView<uint8_t> indexView8(model, primitive.indices);

        if (indexView32.status() == CesiumGltf::AccessorViewStatus::Valid) {
            out.indices.reserve(indexView32.size());
            for (int64_t i = 0; i < indexView32.size(); ++i) out.indices.push_back(indexView32[i]);
        } else if (indexView16.status() == CesiumGltf::AccessorViewStatus::Valid) {
            out.indices.reserve(indexView16.size());
            for (int64_t i = 0; i < indexView16.size(); ++i) out.indices.push_back(indexView16[i]);
        } else if (indexView8.status() == CesiumGltf::AccessorViewStatus::Valid) {
            out.indices.reserve(indexView8.size());
            for (int64_t i = 0; i < indexView8.size(); ++i) out.indices.push_back(indexView8[i]);
        }
    } else {
        out.indices.reserve(vertexCount);
        for (uint32_t i = 0; i < vertexCount; ++i) out.indices.push_back(i);
    }

    if (primitive.mode == 5) {
        if (out.indices.size() >= 3) {
            std::vector<uint32_t> stripIndices;
            stripIndices.reserve((out.indices.size() - 2) * 3);
            int wind = 0;
            for (size_t i = 0; i < out.indices.size() - 2; ++i) {
                uint32_t i0 = out.indices[i];
                uint32_t i1 = out.indices[i + 1];
                uint32_t i2 = out.indices[i + 2];

                if (i0 == 0xFFFFFFFF || i0 == 65535 ||
                    i1 == 0xFFFFFFFF || i1 == 65535 ||
                    i2 == 0xFFFFFFFF || i2 == 65535) {
                    wind = 0;
                    continue;
                }

                if (i0 == i1 || i1 == i2 || i0 == i2) {
                    wind++;
                    continue;
                }

                if (wind % 2 == 0) {
                    stripIndices.push_back(i0);
                    stripIndices.push_back(i1);
                    stripIndices.push_back(i2);
                } else {
                    stripIndices.push_back(i1);
                    stripIndices.push_back(i0);
                    stripIndices.push_back(i2);
                }
                wind++;
            }
            out.indices = std::move(stripIndices);
        }
    } else if (primitive.mode == 6) {
        if (out.indices.size() >= 3) {
            std::vector<uint32_t> fanIndices;
            fanIndices.reserve((out.indices.size() - 2) * 3);
            for (size_t i = 1; i < out.indices.size() - 1; ++i) {
                fanIndices.push_back(out.indices[0]);
                fanIndices.push_back(out.indices[i]);
                fanIndices.push_back(out.indices[i + 1]);
            }
            out.indices = std::move(fanIndices);
        }
    }
}

glm::dmat4 getNodeLocalTransform(const CesiumGltf::Model& model, int32_t nodeIdx) {
    const auto& node = model.nodes[nodeIdx];
    glm::dmat4 local(1.0);
    if (node.matrix.size() == 16) {
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                local[c][r] = node.matrix[c * 4 + r];
    } else {
        glm::dvec3 t(0), s(1);
        glm::dquat rot(1, 0, 0, 0);
        if (node.translation.size() == 3) t = {node.translation[0], node.translation[1], node.translation[2]};
        if (node.rotation.size() == 4) rot = glm::dquat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        if (node.scale.size() == 3) s = {node.scale[0], node.scale[1], node.scale[2]};
        glm::dmat4 T = glm::translate(glm::dmat4(1.0), t);
        glm::dmat4 R = glm::mat4_cast(rot);
        glm::dmat4 S = glm::scale(glm::dmat4(1.0), s);
        local = T * R * S;
    }
    return local;
}

void traverseNodes(const CesiumGltf::Model& model, int32_t nodeIdx, const glm::dmat4& parentTransform, ParsedTile& tile, int& meshIdx, uint64_t modelInstanceId) {
    if (nodeIdx < 0 || nodeIdx >= (int32_t)model.nodes.size()) return;
    const auto& node = model.nodes[nodeIdx];
    glm::dmat4 globalTransform = parentTransform * getNodeLocalTransform(model, nodeIdx);

    if (node.mesh >= 0 && node.mesh < (int32_t)model.meshes.size()) {
        const auto& mesh = model.meshes[node.mesh];
        for (const auto& prim : mesh.primitives) {
            ParsedPrimitive pp;
            parsePrimitive(model, prim, pp, globalTransform, modelInstanceId);
            pp.nodeTransform = globalTransform;
            if (!pp.vertices.empty()) {
                tile.primitives.push_back(std::move(pp));
            }
            meshIdx++;
        }
    }
    for (int32_t child : node.children) {
        traverseNodes(model, child, globalTransform, tile, meshIdx, modelInstanceId);
    }
}

} // namespace

CesiumPrepareRendererResources::CesiumPrepareRendererResources(Scene* scene, Nexus::IContext* context, Core::TextureManager* textureManager, Core::MeshManager* meshManager)
    : m_scene(scene), m_context(context), m_textureManager(textureManager), m_meshManager(meshManager) {}

CesiumPrepareRendererResources::~CesiumPrepareRendererResources() = default;

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources> CesiumPrepareRendererResources::prepareInLoadThread(
    const CesiumAsync::AsyncSystem& asyncSystem,
    Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
    const glm::dmat4& transform,
    const std::any& rendererOptions) {

    CesiumGltf::Model* pModel = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);
    if (!pModel) {
        return asyncSystem.createResolvedFuture(
            Cesium3DTilesSelection::TileLoadResultAndRenderResources{
                std::move(tileLoadResult),
                nullptr
            }
        );
    }

    auto pParsedTile = new ParsedTile();

    uint64_t modelInstanceId = s_cesiumModelCounter.fetch_add(1);

    if (!pModel->images.empty()) {
        for (int32_t i = 0; i < (int32_t)pModel->images.size(); ++i) {
            const auto& cesiumImage = pModel->images[i];
            if (cesiumImage.cesium.pixelData.empty()) continue;

            ParsedImage pImg;
            pImg.textureKey = fmt::format("C3DT_{}_{}", modelInstanceId, i);
            pImg.data.width = cesiumImage.cesium.width;
            pImg.data.height = cesiumImage.cesium.height;
            pImg.data.channels = 4;

            size_t numPixels = cesiumImage.cesium.width * cesiumImage.cesium.height;
            pImg.data.pixels.resize(numPixels * 4);
            const uint8_t* src = reinterpret_cast<const uint8_t*>(cesiumImage.cesium.pixelData.data());
            uint8_t* dst = pImg.data.pixels.data();

            int32_t srcChannels = cesiumImage.cesium.channels;
            if (srcChannels == 4) {
                std::memcpy(dst, src, numPixels * 4);
            } else if (srcChannels == 3) {
                for (size_t p = 0; p < numPixels; ++p) {
                    dst[p*4+0] = src[p*3+0];
                    dst[p*4+1] = src[p*3+1];
                    dst[p*4+2] = src[p*3+2];
                    dst[p*4+3] = 255;
                }
            } else if (srcChannels == 2) {
                for (size_t p = 0; p < numPixels; ++p) {
                    dst[p*4+0] = src[p*2+0];
                    dst[p*4+1] = src[p*2+0];
                    dst[p*4+2] = src[p*2+0];
                    dst[p*4+3] = src[p*2+1];
                }
            } else if (srcChannels == 1) {
                for (size_t p = 0; p < numPixels; ++p) {
                    dst[p*4+0] = src[p];
                    dst[p*4+1] = src[p];
                    dst[p*4+2] = src[p];
                    dst[p*4+3] = 255;
                }
            }

            pParsedTile->images.push_back(std::move(pImg));
        }
    }

    glm::dmat4 rootTransform = transform;
    rootTransform = CesiumGltfContent::GltfUtilities::applyRtcCenter(*pModel, rootTransform);
    rootTransform = CesiumGltfContent::GltfUtilities::applyGltfUpAxisTransform(*pModel, rootTransform);

    pParsedTile->tileTransform = rootTransform;

    int meshIdx = 0;
    if (!pModel->scenes.empty()) {
        int sceneIdx = std::max(0, pModel->scene);
        const auto& scene = pModel->scenes[sceneIdx];
        for (int32_t rootNode : scene.nodes) {
            traverseNodes(*pModel, rootNode, glm::dmat4(1.0), *pParsedTile, meshIdx, modelInstanceId);
        }
    } else {
        for (int32_t ni = 0; ni < (int32_t)pModel->nodes.size(); ni++) {
            traverseNodes(*pModel, ni, glm::dmat4(1.0), *pParsedTile, meshIdx, modelInstanceId);
        }
    }

    return asyncSystem.createResolvedFuture(Cesium3DTilesSelection::TileLoadResultAndRenderResources{
        std::move(tileLoadResult), pParsedTile
    });
}

void CesiumPrepareRendererResources::setEcefToLocalYUp(const glm::dmat4& ecefToLocalYUp) {
    std::lock_guard<std::mutex> lock(m_transformMutex);
    m_ecefToLocalYUp = ecefToLocalYUp;
}

void CesiumPrepareRendererResources::ensureCesiumRootEntity() {
    if (m_cesiumRootEntity == entt::null || !m_scene->getRegistry().has<TagComponent>(m_cesiumRootEntity)) {
        Entity root = m_scene->createEntity("CesiumTiles");
        m_cesiumRootEntity = root.getHandle();
    }
}

void* CesiumPrepareRendererResources::prepareInMainThread(Cesium3DTilesSelection::Tile& tile, void* pLoadThreadResult) {
    auto pParsedTile = static_cast<ParsedTile*>(pLoadThreadResult);
    if (!pParsedTile) {
        NX_CORE_ERROR("[CesiumMainThread] prepareInMainThread called with NULL pLoadThreadResult!");
        return nullptr;
    }

    glm::dmat4 ecefToLocal;
    {
        std::lock_guard<std::mutex> lock(m_transformMutex);
        ecefToLocal = m_ecefToLocalYUp;
    }

    auto pRenderResources = new CesiumTileRenderResources();
    pRenderResources->tileTransform = pParsedTile->tileTransform;

    auto tStart = std::chrono::high_resolution_clock::now();
    double timeTex = 0, timeVerts = 0, timeBuffers = 0;

    std::unordered_map<std::string, ITexture*> loadedTextures;
    for (auto& img : pParsedTile->images) {
        if (!img.data.pixels.empty() && m_textureManager) {
            ITexture* tex = m_textureManager->createTextureFromMemory(img.textureKey, img.data);
            if (tex) {
                loadedTextures[img.textureKey] = tex;
                pRenderResources->textureKeys.push_back(img.textureKey);
            }
        }
    }

    auto tTexDone = std::chrono::high_resolution_clock::now();
    timeTex = std::chrono::duration<double, std::milli>(tTexDone - tStart).count();

    for (size_t pi = 0; pi < pParsedTile->primitives.size(); ++pi) {
        auto tTexLoopStart = std::chrono::high_resolution_clock::now();
        auto& prim = pParsedTile->primitives[pi];
        CesiumPrimitiveRenderData renderData;
        renderData.indexCount = static_cast<uint32_t>(prim.indices.size());
        renderData.baseColorFactor = prim.baseColorFactor;

        if (m_textureManager && m_textureManager->getWhiteTexture()) {
            renderData.albedoTexture = m_textureManager->getWhiteTexture()->getBindlessTextureIndex();
            renderData.samplerIndex = m_textureManager->getWhiteTexture()->getBindlessSamplerIndex();
        }

        if (!prim.textureKey.empty()) {
            auto it = loadedTextures.find(prim.textureKey);
            if (it != loadedTextures.end()) {
                renderData.albedoTexture = it->second->getBindlessTextureIndex();
                renderData.samplerIndex = it->second->getBindlessSamplerIndex();
            } else if (m_textureManager) {
                ITexture* tex = m_textureManager->getOrCreateTexture(prim.textureKey);
                if (tex) {
                    renderData.albedoTexture = tex->getBindlessTextureIndex();
                    renderData.samplerIndex = tex->getBindlessSamplerIndex();
                    pRenderResources->textureKeys.push_back(prim.textureKey);
                }
            }
        }

        auto tVertsStart = std::chrono::high_resolution_clock::now();
        timeTex += std::chrono::duration<double, std::milli>(tVertsStart - tTexLoopStart).count();

        size_t vertCount = prim.vertices.size() / 8;
        glm::dmat4 combined = ecefToLocal * pParsedTile->tileTransform * prim.nodeTransform;
        for (size_t vi = 0; vi < vertCount; vi++) {
            double px = (double)prim.vertices[vi * 8 + 0];
            double py = (double)prim.vertices[vi * 8 + 1];
            double pz = (double)prim.vertices[vi * 8 + 2];

            glm::dvec4 localPos = combined * glm::dvec4(px, py, pz, 1.0);
            prim.vertices[vi * 8 + 0] = static_cast<float>(localPos.x);
            prim.vertices[vi * 8 + 1] = static_cast<float>(localPos.y);
            prim.vertices[vi * 8 + 2] = static_cast<float>(localPos.z);

            double nx = (double)prim.vertices[vi * 8 + 5];
            double ny = (double)prim.vertices[vi * 8 + 6];
            double nz = (double)prim.vertices[vi * 8 + 7];
            glm::dvec4 localNorm = combined * glm::dvec4(nx, ny, nz, 0.0);
            prim.vertices[vi * 8 + 5] = static_cast<float>(localNorm.x);
            prim.vertices[vi * 8 + 6] = static_cast<float>(localNorm.y);
            prim.vertices[vi * 8 + 7] = static_cast<float>(localNorm.z);
        }

        auto tVertsDone = std::chrono::high_resolution_clock::now();
        timeVerts += std::chrono::duration<double, std::milli>(tVertsDone - tVertsStart).count();

        auto tBuffStart = std::chrono::high_resolution_clock::now();

        size_t vSize = prim.vertices.size() * sizeof(float);
        size_t iSize = prim.indices.size() * sizeof(uint32_t);

        uint32_t vOffset = 0, iOffset = 0;
        if (vSize > 0 || iSize > 0) {
            if (m_meshManager) {
                auto status = m_meshManager->addMesh(prim.vertices, prim.indices, vOffset, iOffset);
                if (!status.ok()) {
                    NX_CORE_ERROR("Failed to allocate mesh space from MeshManager: {}", status.message());
                } else {
                    renderData.vertexBuffer = nullptr;
                    renderData.indexBuffer = nullptr;
                    renderData.vertexOffset = vOffset;
                    renderData.indexOffset = iOffset;
                }
            }
        }
        auto tBuffDone = std::chrono::high_resolution_clock::now();
        timeBuffers += std::chrono::duration<double, std::milli>(tBuffDone - tBuffStart).count();

        pRenderResources->primitives.push_back(std::move(renderData));

        Entity ent = m_scene->createEntity("CesiumTile_Prim");

        auto& mesh = ent.addComponent<MeshComponent>();
        mesh.vertexBuffer = pRenderResources->primitives.back().vertexBuffer.get();
        mesh.indexBuffer = pRenderResources->primitives.back().indexBuffer.get();
        mesh.vertexOffset = pRenderResources->primitives.back().vertexOffset;
        mesh.indexOffset = pRenderResources->primitives.back().indexOffset;
        mesh.indexCount = pRenderResources->primitives.back().indexCount;
        mesh.albedoFactor = pRenderResources->primitives.back().baseColorFactor;
        mesh.albedoTexture = pRenderResources->primitives.back().albedoTexture;
        mesh.samplerIndex = pRenderResources->primitives.back().samplerIndex;

        ent.addComponent<CesiumGltfComponent>().m_tileTransform = pRenderResources->tileTransform;

        if (ent.hasComponent<TransformComponent>()) {
            auto& trans = ent.getComponent<TransformComponent>();
            trans.worldMatrix[12] = 9999999.0f;
            trans.worldMatrix[13] = 9999999.0f;
            trans.worldMatrix[14] = 9999999.0f;
        }

        pRenderResources->entities.push_back(ent.getHandle());

        ensureCesiumRootEntity();
        Entity rootEntity(m_cesiumRootEntity, &m_scene->getRegistry());
        m_scene->setParent(ent, rootEntity);
    }

    auto tTotalDone = std::chrono::high_resolution_clock::now();
    double timeTotal = std::chrono::duration<double, std::milli>(tTotalDone - tStart).count();

    if (timeTotal > 1.0) {
        NX_CORE_INFO("[CesiumMainThread_Profile] Tile {} prims: Total={:.2f}ms (Tex={:.2f}ms, VertTransform={:.2f}ms, VulkanBufferCreate&Upload={:.2f}ms)",
            pParsedTile->primitives.size(), timeTotal, timeTex, timeVerts, timeBuffers);
    }

    delete pParsedTile;
    return pRenderResources;
}

void CesiumPrepareRendererResources::free(
    Cesium3DTilesSelection::Tile& tile,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept {

    if (pLoadThreadResult) {
        delete static_cast<ParsedTile*>(pLoadThreadResult);
    }

    if (pMainThreadResult) {
        auto pRenderResources = static_cast<CesiumTileRenderResources*>(pMainThreadResult);
        for (auto e : pRenderResources->entities) {
            if (m_scene->getRegistry().has<CesiumGltfComponent>(e)) {
                m_scene->destroyEntity(Entity(e, &m_scene->getRegistry()));
            }
        }
        for (const auto& key : pRenderResources->textureKeys) {
            if (m_textureManager) {
                m_textureManager->removeTexture(key);
            }
        }
        m_deferredDeletions.push_back({DEFERRED_FRAMES, pRenderResources});
    }
}

void CesiumPrepareRendererResources::pumpDeferredDeletion() {
    for (auto& d : m_deferredDeletions) {
        d.framesRemaining--;
    }
    while (!m_deferredDeletions.empty() && m_deferredDeletions.front().framesRemaining <= 0) {
        auto* res = m_deferredDeletions.front().resources;
        if (m_meshManager) {
            for (const auto& prim : res->primitives) {
                if (prim.indexCount > 0) {
                    m_meshManager->removeMesh(prim.vertexOffset, prim.indexOffset);
                }
            }
        }
        delete res;
        m_deferredDeletions.pop_front();
    }
}

void CesiumPrepareRendererResources::attachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pMainThreadRendererResources,
    const glm::dvec2& translation,
    const glm::dvec2& scale) {
}

void CesiumPrepareRendererResources::detachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pMainThreadRendererResources) noexcept {
}

void* CesiumPrepareRendererResources::prepareRasterInLoadThread(
    CesiumGltf::ImageCesium& image,
    const std::any& rendererOptions) {
    return nullptr;
}

void* CesiumPrepareRendererResources::prepareRasterInMainThread(
    CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pLoadThreadResult) {
    return nullptr;
}

void CesiumPrepareRendererResources::freeRaster(
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept {
}
