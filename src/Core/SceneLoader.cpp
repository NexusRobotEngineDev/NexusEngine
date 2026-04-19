#include "SceneLoader.h"
#include "Components.h"
#include "Scene.h"
#include "ModelLoader.h"
#include "RenderSystem.h"
#include "TextureManager.h"
#include "../Bridge/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace Nexus {
namespace Core {

static std::array<float, 3> readVec3(const json& j, const std::string& key, std::array<float, 3> def = {0,0,0}) {
    if (j.contains(key) && j[key].is_array() && j[key].size() >= 3)
        return {j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>()};
    return def;
}

static std::array<float, 4> readVec4(const json& j, const std::string& key, std::array<float, 4> def = {1,1,1,1}) {
    if (j.contains(key) && j[key].is_array() && j[key].size() >= 4)
        return {j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>(), j[key][3].get<float>()};
    return def;
}

static std::array<float, 2> readVec2(const json& j, const std::string& key, std::array<float, 2> def = {20,20}) {
    if (j.contains(key) && j[key].is_array() && j[key].size() >= 2)
        return {j[key][0].get<float>(), j[key][1].get<float>()};
    return def;
}

StatusOr<SceneLoader::SceneConfig> SceneLoader::parseSceneFile(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open())
        return NotFoundError("场景文件不存在: " + jsonPath);

    json j;
    try {
        j = json::parse(file);
    } catch (const json::parse_error& e) {
        return InternalError("JSON 解析失败: " + std::string(e.what()));
    }

    SceneConfig config;
    config.sceneName = j.value("name", "UntitledScene");

    if (j.contains("camera"))
        config.cameraPosition = readVec3(j["camera"], "position", {0.f, 0.5f, 3.f});

    if (j.contains("enable_gis")) {
        config.enableGis = j.value("enable_gis", true);
    }

    if (j.contains("robot")) {
        config.robotUrdf    = j["robot"].value("urdf", "");
        config.robotPhysics = j["robot"].value("physics", "");
    }

    if (j.contains("ground")) {
        config.hasGround   = true;
        config.groundSize  = readVec2(j["ground"], "size", {20.f, 20.f});
        config.groundColor = readVec4(j["ground"], "color", {0.6f, 0.6f, 0.6f, 1.f});
    }

    if (j.contains("objects") && j["objects"].is_array()) {
        for (auto& obj : j["objects"]) {
            SceneLoader::ObjectDef def;
            def.modelPath = obj.value("model", obj.value("modelPath", ""));
            def.type     = def.modelPath.empty() ? obj.value("type", "box") : "model";
            def.position = readVec3(obj, "position");
            def.size     = readVec3(obj, "size", {1, 1, 1});
            def.color    = readVec4(obj, "color");
            def.metallic = obj.value("metallic", 0.0f);
            def.roughness = obj.value("roughness", 1.0f);
            config.objects.push_back(def);
        }
    }

    NX_CORE_INFO("场景配置: '{}', robot='{}', 物体数={}", config.sceneName, config.robotUrdf, config.objects.size());
    return config;
}

Status SceneLoader::createEntities(
    const SceneConfig& config,
    Scene* scene,
    RenderSystem* renderer,
    TextureManager* textureManager
) {
    Entity camera = scene->createEntity("MainCamera");
    camera.getComponent<TransformComponent>().position = config.cameraPosition;
    camera.addComponent<CameraComponent>();

    if (!config.robotUrdf.empty() && renderer && textureManager) {
        std::string urdfPath = "Data/" + config.robotUrdf;
        ModelLoader::loadURDF(textureManager, scene, renderer->getMeshManager(), urdfPath);
        NX_CORE_INFO("加载机器人: {}", urdfPath);
    }

    if (config.hasGround && renderer) {
        Entity floor = scene->createEntity("Floor");
        auto& ft = floor.getComponent<TransformComponent>();
        ft.position = {0.0f, 0.0f, 0.0f};
        ft.scale = {config.groundSize[0], 0.01f, config.groundSize[1]};
        floor.addComponent<MeshComponent>(renderer->getCubeMeshComponent());
    }

    for (size_t i = 0; i < config.objects.size(); ++i) {
        const auto& obj = config.objects[i];
        if (obj.type == "model" && !obj.modelPath.empty()) {
            std::string fullModelPath = "Data/" + obj.modelPath;
            if (obj.modelPath.substr(0, 5) == "Data/") {
                fullModelPath = obj.modelPath;
            }
            Entity entity = ModelLoader::loadModel(textureManager, scene, renderer->getMeshManager(), fullModelPath);
            if (entity.isValid()) {
                auto& tr = entity.getComponent<TransformComponent>();
                tr.position = obj.position;
                tr.scale = obj.size;
            }
        } else {
            Entity entity = scene->createEntity("Object_" + std::to_string(i));
            auto& tr = entity.getComponent<TransformComponent>();
            tr.position = obj.position;
            tr.scale = obj.size;
            if (renderer) {
                auto mesh = renderer->getCubeMeshComponent();
                mesh.albedoFactor = obj.color;
                mesh.metallicFactor = obj.metallic;
                mesh.roughnessFactor = obj.roughness;
                entity.addComponent<MeshComponent>(mesh);
            }
        }
    }

    return OkStatus();
}

} // namespace Core
} // namespace Nexus
