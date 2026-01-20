#include "SceneSerializer.h"
#include "Components.h"
#include "../Bridge/ResourceLoader.h"
#include "../Bridge/Log.h"

#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <fstream>
#include <unordered_map>

namespace Nexus {

struct SerializedEntity {
    uint32_t id;
    bool hasTag = false;
    TagComponent tag;

    bool hasTransform = false;
    TransformComponent transform;

    bool hasHierarchy = false;
    uint32_t parent = 0;
    std::vector<uint32_t> children;

    template<class Archive>
    void serialize(Archive& ar) {
        ar(id, hasTag, tag, hasTransform, transform, hasHierarchy, parent, children);
    }
};

SceneSerializer::SceneSerializer(Scene& scene) : m_scene(scene) {}

bool SceneSerializer::serialize(const std::string& filePath) {
    std::string fullPath = ResourceLoader::getBasePath() + filePath;
    std::ofstream os(fullPath, std::ios::binary);
    if (!os.is_open()) {
        NX_CORE_ERROR("SceneSerializer: 无法打开文件进行写入: {}", fullPath);
        return false;
    }

    try {
        cereal::BinaryOutputArchive archive(os);

        std::vector<SerializedEntity> entities;
        auto& reg = m_scene.getRegistry().getInternal();

        for (auto entityHandle : reg.storage<entt::entity>()) {
            SerializedEntity se;
            se.id = static_cast<uint32_t>(entityHandle);

            if (reg.all_of<TagComponent>(entityHandle)) {
                se.hasTag = true;
                se.tag = reg.get<TagComponent>(entityHandle);
            }
            if (reg.all_of<TransformComponent>(entityHandle)) {
                se.hasTransform = true;
                se.transform = reg.get<TransformComponent>(entityHandle);
            }
            if (reg.all_of<HierarchyComponent>(entityHandle)) {
                se.hasHierarchy = true;
                auto& hier = reg.get<HierarchyComponent>(entityHandle);
                se.parent = static_cast<uint32_t>(hier.parent);
                for (auto child : hier.children) {
                    se.children.push_back(static_cast<uint32_t>(child));
                }
            }
            entities.push_back(se);
        }

        uint32_t count = static_cast<uint32_t>(entities.size());
        archive(count);
        for (auto& se : entities) {
            archive(se);
        }

        NX_CORE_INFO("SceneSerializer: 场景已成功保存到 {}", fullPath);
        return true;
    } catch (const std::exception& e) {
        NX_CORE_ERROR("SceneSerializer: 序列化失败: {}", e.what());
        return false;
    }
}

bool SceneSerializer::deserialize(const std::string& filePath) {
    std::string fullPath = ResourceLoader::getBasePath() + filePath;
    std::ifstream is(fullPath, std::ios::binary);
    if (!is.is_open()) {
        NX_CORE_ERROR("SceneSerializer: 无法打开文件进行读取: {}", fullPath);
        return false;
    }

    try {
        cereal::BinaryInputArchive archive(is);
        uint32_t count = 0;
        archive(count);

        std::vector<SerializedEntity> entities(count);
        for (uint32_t i = 0; i < count; ++i) {
            archive(entities[i]);
        }

        m_scene.getRegistry().getInternal().clear();

        std::unordered_map<uint32_t, entt::entity> idMap;
        idMap[static_cast<uint32_t>(entt::null)] = entt::null;

        auto& reg = m_scene.getRegistry().getInternal();

        for (auto& se : entities) {
            auto handle = reg.create();
            idMap[se.id] = handle;

            if (se.hasTag) {
                reg.emplace<TagComponent>(handle, se.tag);
            }
            if (se.hasTransform) {
                reg.emplace<TransformComponent>(handle, se.transform);
            }
        }

        for (auto& se : entities) {
            if (se.hasHierarchy) {
                auto newHandle = idMap[se.id];
                auto& hier = reg.emplace<HierarchyComponent>(newHandle);

                auto parentIt = idMap.find(se.parent);
                hier.parent = (parentIt != idMap.end()) ? parentIt->second : entt::null;

                for (auto oldChildId : se.children) {
                    auto childIt = idMap.find(oldChildId);
                    if (childIt != idMap.end() && childIt->second != entt::null) {
                        hier.children.push_back(childIt->second);
                    }
                }
            }
        }

        NX_CORE_INFO("SceneSerializer: 场景已从 {} 恢复，共 {} 个实体", fullPath, count);
        return true;
    } catch (const std::exception& e) {
        NX_CORE_ERROR("SceneSerializer: 反序列化失败: {}", e.what());
        return false;
    }
}

} // namespace Nexus
