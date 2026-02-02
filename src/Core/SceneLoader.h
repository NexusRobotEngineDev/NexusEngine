#pragma once

#include "Base.h"
#include <string>
#include <vector>
#include <array>

namespace Nexus {

class Scene;

namespace Core {

class RenderSystem;
class TextureManager;

/**
 * 从 JSON 场景描述文件加载整个场景：相机、机器人（URDF）、地面、静态物体。
 * 物理系统的创建和线程管理仍由 Main.cpp 负责，SceneLoader 只提供路径配置。
 */
class SceneLoader {
public:
    struct ObjectDef {
        std::string type;
        std::array<float, 3> position = {0,0,0};
        std::array<float, 3> size = {1,1,1};
        std::array<float, 4> color = {1,1,1,1};
    };

    struct SceneConfig {
        std::string sceneName;
        std::array<float, 3> cameraPosition = {0.f, 0.5f, 3.f};
        std::string robotUrdf;
        std::string robotPhysics;
        bool hasGround = true;
        std::array<float, 2> groundSize = {20.f, 20.f};
        std::array<float, 4> groundColor = {0.6f, 0.6f, 0.6f, 1.f};
        std::vector<ObjectDef> objects;
    };

    static StatusOr<SceneConfig> parseSceneFile(const std::string& jsonPath);

    static Status createEntities(
        const SceneConfig& config,
        Scene* scene,
        RenderSystem* renderer,
        TextureManager* textureManager
    );
};

} // namespace Core
} // namespace Nexus
