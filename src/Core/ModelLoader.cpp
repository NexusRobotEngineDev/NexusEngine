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
#include "../Bridge/thirdparty.h"

namespace Nexus {
namespace Core {

static void processNode(TextureManager* textureManager, aiNode* node, const aiScene* aScene, Scene* engineScene, MeshManager* meshManager, Entity parentEntity, const std::string& directory) {
    Entity nodeEntity = engineScene->createEntity(node->mName.C_Str());

    aiVector3D scale;
    aiQuaternion rotation;
    aiVector3D position;
    node->mTransformation.Decompose(scale, rotation, position);

    auto& transform = nodeEntity.getComponent<TransformComponent>();
    transform.position = {position.x, position.y, position.z};
    transform.rotation = {rotation.x, rotation.y, rotation.z, rotation.w};
    transform.scale = {scale.x, scale.y, scale.z};

    NX_CORE_INFO("[NodeDebug] Node: {}, Pos: ({},{},{}), Rot: ({},{},{},{})",
                 node->mName.C_Str(), position.x, position.y, position.z,
                 rotation.x, rotation.y, rotation.z, rotation.w);

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
                subMeshEntity = engineScene->createEntity(std::string(node->mName.C_Str()) + "_mesh" + std::to_string(m));
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
            if (meshManager->addMesh(vertices, indices, vOffset, iOffset).ok()) {
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

                NX_CORE_INFO("[TextureDebug] Submesh entity assigned: Albedo={}, Sampler={}", albedoIndex, samplerIndex);
            }
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(textureManager, node->mChildren[i], aScene, engineScene, meshManager, nodeEntity, directory);
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

    NX_CORE_INFO("Successfully loaded model via Assimp: {}", fullPath);
    return rootEntity;
}

} // namespace Core
} // namespace Nexus
