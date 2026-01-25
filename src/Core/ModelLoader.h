#pragma once

#include "Base.h"
#include "../Bridge/Entity.h"
#include <string>

namespace Nexus {

class Scene;
namespace Core {

class MeshManager;
class TextureManager;

/**
 * @brief Assimp based Model Loader
 */
class ModelLoader {
public:
    /**
     * @brief Loads a 3D model into the scene hierarchy
     * @param scene The engine scene
     * @param meshManager The mesh manager to allocate vertex/index buffers
     * @param path The relative path to the model file
     * @return The root entity of the loaded model
     */
    static Entity loadModel(TextureManager* textureManager, Scene* scene, MeshManager* meshManager, const std::string& path);
};

} // namespace Core
} // namespace Nexus
