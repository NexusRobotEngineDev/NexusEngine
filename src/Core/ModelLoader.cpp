#include "ModelLoader.h"
#include "MeshManager.h"
#include "Scene.h"
#include "Components.h"
#include "../Bridge/ResourceLoader.h"
#include "../Bridge/Log.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include "TextureManager.h"
#include "URDFLoader.h"
#include "../Bridge/thirdparty.h"
#include <optional>
#include <cassert>

namespace Nexus {
namespace Core {

static void processNode(TextureManager* textureManager, aiNode* node, const aiScene* aScene, Scene* engineScene, MeshManager* meshManager, Entity parentEntity, const std::string& directory, std::optional<std::array<float, 4>> fallbackColor = std::nullopt) {
    std::string nodeName = node->mName.C_Str();
    if (node->mNumMeshes == 0 && node->mNumChildren == 0 &&
        (nodeName.find("Camera") != std::string::npos ||
         nodeName.find("Light") != std::string::npos)) {
        return;
    }

    std::string prefix = "";
    if (parentEntity.isValid() && engineScene->getRegistry().has<TagComponent>(parentEntity.getHandle())) {
        prefix = engineScene->getRegistry().get<TagComponent>(parentEntity.getHandle()).name + "_";
    }

    Entity nodeEntity = engineScene->createEntity(prefix + node->mName.C_Str());

    auto& transform = nodeEntity.getComponent<TransformComponent>();

    aiVector3D position, rotationEuler, scale;
    node->mTransformation.Decompose(scale, rotationEuler, position);

    transform.position = {position.x, position.y, position.z};

    aiQuaternion rotationQuat = aiQuaternion(rotationEuler.y, rotationEuler.z, rotationEuler.x);
    transform.rotation = {rotationQuat.x, rotationQuat.y, rotationQuat.z, rotationQuat.w};

    transform.scale = {scale.x, scale.y, scale.z};

    NX_CORE_INFO("[NodeDebug] Node: {}, Pos: ({},{},{}), Rot: ({},{},{},{})",
                 nodeEntity.getComponent<TagComponent>().name, position.x, position.y, position.z,
                 rotationQuat.x, rotationQuat.y, rotationQuat.z, rotationQuat.w);

    if (parentEntity.isValid()) {
        engineScene->setParent(nodeEntity, parentEntity);
    }

    if (node->mNumMeshes > 0) {
        for (unsigned int m = 0; m < node->mNumMeshes; m++) {
            aiMesh* mesh = aScene->mMeshes[node->mMeshes[m]];
            Entity subMeshEntity;
            if (node->mNumMeshes == 1) {
                subMeshEntity = nodeEntity;
            } else {
                subMeshEntity = engineScene->createEntity(prefix + node->mName.C_Str() + "_mesh" + std::to_string(m));
                engineScene->setParent(subMeshEntity, nodeEntity);
            }

            std::vector<float> vertices;
            std::vector<uint32_t> indices;

            for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
                vertices.push_back(mesh->mVertices[i].x);
                vertices.push_back(mesh->mVertices[i].y);
                vertices.push_back(mesh->mVertices[i].z);

                if (mesh->mTextureCoords[0]) {
                    vertices.push_back(mesh->mTextureCoords[0][i].x);
                    vertices.push_back(mesh->mTextureCoords[0][i].y);
                } else {
                    vertices.push_back(0.0f);
                    vertices.push_back(0.0f);
                }

                if (mesh->mNormals) {
                    vertices.push_back(mesh->mNormals[i].x);
                    vertices.push_back(mesh->mNormals[i].y);
                    vertices.push_back(mesh->mNormals[i].z);
                } else {
                    vertices.push_back(0.0f);
                    vertices.push_back(1.0f);
                    vertices.push_back(0.0f);
                }
            }

            for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
                aiFace face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; j++) {
                    indices.push_back(face.mIndices[j]);
                }
            }

            uint32_t albedoIndex = 0;
            uint32_t samplerIndex = 0;

            if (mesh->mMaterialIndex >= 0) {
                aiMaterial* material = aScene->mMaterials[mesh->mMaterialIndex];
                aiString texPath;
                bool found = false;

                aiString matName;
                if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
                    NX_CORE_INFO("[TextureDebug] Submesh Material: {}, Name: {}", mesh->mMaterialIndex, matName.C_Str());
                }

                for (int i = 0; i <= 21; ++i) {
                    aiTextureType type = static_cast<aiTextureType>(i);
                    uint32_t count = material->GetTextureCount(type);
                    if (count > 0) {
                        material->GetTexture(type, 0, &texPath);
                        NX_CORE_INFO("[TextureDebug] Found texture in type {}: {}", i, texPath.C_Str());
                        found = true;
                        if (i == aiTextureType_BASE_COLOR || i == aiTextureType_DIFFUSE || albedoIndex == 0) {
                             const aiTexture* embeddedTexture = aScene->GetEmbeddedTexture(texPath.C_Str());
                             ITexture* tex = nullptr;
                             if (embeddedTexture) {
                                 std::string key = directory + "#embedded#" + std::string(texPath.C_Str());
                                 if (embeddedTexture->mHeight == 0) {
                                     auto texRes = ResourceLoader::loadImageFromMemory(reinterpret_cast<const uint8_t*>(embeddedTexture->pcData), embeddedTexture->mWidth);
                                     if (texRes.ok()) {
                                         tex = textureManager->createTextureFromMemory(key, texRes.value());
                                     }
                                 }
                             } else {
                                 std::string rawPath = texPath.C_Str();
                                 std::string fullTexPath;
                                 if (rawPath.find(":") != std::string::npos || rawPath.front() == '/' || rawPath.front() == '\\') {
                                     fullTexPath = rawPath;
                                 } else {
                                     fullTexPath = directory + "/" + rawPath;
                                 }
                                 tex = textureManager->getOrCreateTexture(fullTexPath);
                             }

                             if (tex) {
                                 albedoIndex = tex->getBindlessTextureIndex();
                                 samplerIndex = tex->getBindlessSamplerIndex();
                             }
                        }
                    }
                }

                if (found) {
                    if (albedoIndex != 0) {
                        NX_CORE_INFO("[TextureDebug] ModelLoader: Successfully mapped texture: {} -> bindless index {}, sampler {}", texPath.C_Str(), albedoIndex, samplerIndex);
                    } else {
                        NX_CORE_WARN("[TextureDebug] ModelLoader: Failed to map texture: {}", texPath.C_Str());
                    }
                    aiColor4D diffuse(1,1,1,1);
                    if (material->Get(AI_MATKEY_BASE_COLOR, diffuse) == AI_SUCCESS ||
                        material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
                         NX_CORE_INFO("[TextureDebug] Material {} color applied: ({},{},{},{})",
                                      mesh->mMaterialIndex, diffuse.r, diffuse.g, diffuse.b, diffuse.a);
                    }
                }
            }

            uint32_t vOffset, iOffset;
            auto addMeshStatus = meshManager->addMesh(vertices, indices, vOffset, iOffset);
            if (addMeshStatus.ok()) {
                auto& meshComp = subMeshEntity.addComponent<MeshComponent>();
                meshComp.vertexOffset = vOffset;
                meshComp.indexOffset = iOffset;
                meshComp.indexCount = (uint32_t)indices.size();
                meshComp.albedoTexture = albedoIndex;
                meshComp.samplerIndex = samplerIndex;

                if (mesh->mMaterialIndex >= 0) {
                    aiMaterial* material = aScene->mMaterials[mesh->mMaterialIndex];
                    aiColor4D diffuse(1.0f, 1.0f, 1.0f, 1.0f);
                    if (material->Get(AI_MATKEY_BASE_COLOR, diffuse) == AI_SUCCESS ||
                        material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
                        meshComp.albedoFactor = {diffuse.r, diffuse.g, diffuse.b, diffuse.a};
                    }
                }
                if (fallbackColor && albedoIndex == 0) {
                    meshComp.albedoFactor = *fallbackColor;
                }

                NX_CORE_INFO("[TextureDebug] Submesh entity assigned: Albedo={}, Sampler={}", albedoIndex, samplerIndex);
            } else {
                NX_CORE_ERROR("MeshManager::addMesh Failed: {}", addMeshStatus.message());
            }
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(textureManager, node->mChildren[i], aScene, engineScene, meshManager, nodeEntity, directory, fallbackColor);
    }
}

Entity ModelLoader::loadModel(TextureManager* textureManager, Scene* scene, MeshManager* meshManager, const std::string& path) {
    Assimp::Importer importer;
    std::string fullPath = ResourceLoader::getBasePath() + path;

    const aiScene* aScene = importer.ReadFile(fullPath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    if (!aScene || aScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !aScene->mRootNode) {
        NX_CORE_ERROR("Assimp Error loading model: {} ({})", fullPath, importer.GetErrorString());
        return Entity();
    }

    std::string directory = fullPath.substr(0, fullPath.find_last_of("/\\"));

    Entity rootEntity = scene->createEntity(path + " Root");

    processNode(textureManager, aScene->mRootNode, aScene, scene, meshManager, rootEntity, directory);

    aiVector3D bboxMin(1e10f, 1e10f, 1e10f), bboxMax(-1e10f, -1e10f, -1e10f);
    for (unsigned int m = 0; m < aScene->mNumMeshes; m++) {
        const aiMesh* mesh = aScene->mMeshes[m];
        for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
            const auto& p = mesh->mVertices[v];
            bboxMin.x = std::min(bboxMin.x, p.x); bboxMin.y = std::min(bboxMin.y, p.y); bboxMin.z = std::min(bboxMin.z, p.z);
            bboxMax.x = std::max(bboxMax.x, p.x); bboxMax.y = std::max(bboxMax.y, p.y); bboxMax.z = std::max(bboxMax.z, p.z);
        }
    }
    NX_CORE_INFO("BBOX [{}]: size=({:.3f}, {:.3f}, {:.3f}) min=({:.3f},{:.3f},{:.3f}) max=({:.3f},{:.3f},{:.3f})",
        path,
        bboxMax.x - bboxMin.x, bboxMax.y - bboxMin.y, bboxMax.z - bboxMin.z,
        bboxMin.x, bboxMin.y, bboxMin.z, bboxMax.x, bboxMax.y, bboxMax.z);

    NX_CORE_INFO("Successfully loaded model via Assimp: {}", fullPath);
    return rootEntity;
}

Entity ModelLoader::loadURDF(TextureManager* textureManager, Scene* scene, MeshManager* meshManager, const std::string& urdfPath) {
    std::string fullPath = ResourceLoader::getBasePath() + urdfPath;
    auto result = NxURDF::parseFile(fullPath);
    if (!result) {
        NX_CORE_ERROR("NxURDF: 加载 URDF 失败: {}", fullPath);
        return Entity();
    }

    const auto& model = *result;
    std::string urdfDir = urdfPath.substr(0, urdfPath.find_last_of("/\\"));
    NX_CORE_INFO("NxURDF: 解析成功 '{}' — {} links, {} joints",
                 model.name, model.links.size(), model.joints.size());

    auto rpyToQuat = [](double r, double p, double y) -> std::array<float, 4> {
        double cr = std::cos(r * 0.5), sr = std::sin(r * 0.5);
        double cp = std::cos(p * 0.5), sp = std::sin(p * 0.5);
        double cy = std::cos(y * 0.5), sy = std::sin(y * 0.5);

        return {
            static_cast<float>(sr * cp * cy - cr * sp * sy),
            static_cast<float>(cr * sp * cy + sr * cp * sy),
            static_cast<float>(cr * cp * sy - sr * sp * cy),
            static_cast<float>(cr * cp * cy + sr * sp * sy)
        };
    };

    std::string rootName = model.rootLinkName();

    Entity urdfRootEntity = scene->createEntity(model.name + "_root");
    auto& rootTr = urdfRootEntity.getComponent<TransformComponent>();
    rootTr.rotation = {-0.7071068f, 0.0f, 0.0f, 0.7071068f};

    std::unordered_map<std::string, Entity> linkEntities;
    for (const auto& link : model.links) {
        Entity linkEntity = scene->createEntity(link.name);
        linkEntity.addComponent<RigidBodyComponent>().bodyName = link.name;
        linkEntities[link.name] = linkEntity;
    }

    for (const auto& joint : model.joints) {
        auto childIt = linkEntities.find(joint.childLink);
        auto parentIt = linkEntities.find(joint.parentLink);
        if (childIt == linkEntities.end() || parentIt == linkEntities.end()) continue;

        auto& childTr = childIt->second.getComponent<TransformComponent>();
        childTr.position = {
            static_cast<float>(joint.origin.xyz[0]),
            static_cast<float>(joint.origin.xyz[1]),
            static_cast<float>(joint.origin.xyz[2])
        };
        childTr.rotation = rpyToQuat(joint.origin.rpy[0], joint.origin.rpy[1], joint.origin.rpy[2]);
        scene->setParent(childIt->second, parentIt->second);
    }

    auto rootLinkIt = linkEntities.find(rootName);
    if (rootLinkIt != linkEntities.end()) {
        scene->setParent(rootLinkIt->second, urdfRootEntity);

        std::unordered_map<std::string, double> linkZAccum;
        linkZAccum[rootName] = 0.0;
        for (int pass = 0; pass < 10; ++pass) {
            for (const auto& joint : model.joints) {
                auto parentZ = linkZAccum.find(joint.parentLink);
                if (parentZ != linkZAccum.end()) {
                    double childZ = parentZ->second + joint.origin.xyz[2];
                    auto it = linkZAccum.find(joint.childLink);
                    if (it == linkZAccum.end() || childZ < it->second)
                        linkZAccum[joint.childLink] = childZ;
                }
            }
        }
        double minZ = 0.0;
        for (const auto& [name, z] : linkZAccum)
            if (z < minZ) minZ = z;

        float standingHeight = static_cast<float>(-minZ);
        auto& rootLinkTr = rootLinkIt->second.getComponent<TransformComponent>();
        rootLinkTr.position[1] = -standingHeight;
        NX_CORE_INFO("NxURDF: 自动站立高度 = {:.3f}m (最低Z = {:.3f})", standingHeight, minZ);
    }

    for (const auto& link : model.links) {
        for (const auto& visual : link.visuals) {
            if (visual.geometry.type != NxURDF::GeometryType::Mesh) continue;

            std::string meshPath = NxURDF::resolveMeshPath(visual.geometry.meshFilename, urdfDir);
            std::string fullMeshPath = ResourceLoader::getBasePath() + meshPath;

            Assimp::Importer importer;
            importer.SetPropertyInteger(AI_CONFIG_IMPORT_COLLADA_IGNORE_UP_DIRECTION, 1);
            const aiScene* aScene = importer.ReadFile(fullMeshPath,
                aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

            if (!aScene || aScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
                NX_CORE_WARN("NxURDF: 无法加载网格 {}: {}", fullMeshPath, importer.GetErrorString());
                continue;
            }

            auto linkIt = linkEntities.find(link.name);
            if (linkIt == linkEntities.end()) continue;

            Entity visualEntity = scene->createEntity(link.name + "_visual");
            auto& visualTr = visualEntity.getComponent<TransformComponent>();
            visualTr.position = {
                static_cast<float>(visual.origin.xyz[0]),
                static_cast<float>(visual.origin.xyz[1]),
                static_cast<float>(visual.origin.xyz[2])
            };
            visualTr.rotation = rpyToQuat(
                visual.origin.rpy[0], visual.origin.rpy[1], visual.origin.rpy[2]);
            scene->setParent(visualEntity, linkIt->second);

            std::optional<std::array<float, 4>> fallbackColor = std::nullopt;

            std::string meshExt = meshPath.substr(meshPath.find_last_of('.') + 1);
            std::transform(meshExt.begin(), meshExt.end(), meshExt.begin(), ::tolower);
            bool isSTL = (meshExt == "stl");

            if (isSTL && visual.material) {
                if (visual.material->color[0] != 1.0 || visual.material->color[1] != 1.0 || visual.material->color[2] != 1.0 || visual.material->color[3] != 1.0) {
                    fallbackColor = { (float)visual.material->color[0], (float)visual.material->color[1], (float)visual.material->color[2], (float)visual.material->color[3] };
                } else if (!visual.material->name.empty()) {
                    auto matIt = model.materials.find(visual.material->name);
                    if (matIt != model.materials.end()) {
                        fallbackColor = { (float)matIt->second.color[0], (float)matIt->second.color[1], (float)matIt->second.color[2], (float)matIt->second.color[3] };
                    }
                }
            }

            NX_CORE_INFO("NxURDF: processing visual Mesh for Link={} (ext={}, fallback={})", link.name, meshExt, fallbackColor.has_value());
            processNode(textureManager, aScene->mRootNode, aScene, scene, meshManager, visualEntity, "", fallbackColor);
        }
    }

    NX_CORE_INFO("NxURDF: 根 Link='{}', 共创建 {} 个 link 实体", rootName, linkEntities.size());
    return urdfRootEntity;
}

} // namespace Core
} // namespace Nexus
