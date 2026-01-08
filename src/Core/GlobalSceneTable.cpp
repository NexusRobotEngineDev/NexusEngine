#include "GlobalSceneTable.h"

namespace Nexus {
namespace Core {

void GlobalSceneTable::registerTrunk(SceneTable table) {
    auto id = table.trunkId;
    m_trunks[id] = std::make_unique<SceneTrunk>(std::move(table));
}

void GlobalSceneTable::markLoaded(const std::string& trunkId) {
    auto it = m_trunks.find(trunkId);
    if (it != m_trunks.end()) it->second->markLoaded();
}

void GlobalSceneTable::markUnloaded(const std::string& trunkId) {
    auto it = m_trunks.find(trunkId);
    if (it != m_trunks.end()) it->second->markUnloaded();
}

bool GlobalSceneTable::isLoaded(const std::string& trunkId) const {
    auto it = m_trunks.find(trunkId);
    return it != m_trunks.end() && it->second->isLoaded();
}

std::vector<MeshEntry> GlobalSceneTable::getLoadedEntries() const {
    std::vector<MeshEntry> result;
    for (const auto& [id, trunk] : m_trunks) {
        if (trunk->isLoaded()) {
            for (const auto& entry : trunk->getTable().entries) {
                result.push_back(entry);
            }
        }
    }
    return result;
}

std::vector<std::string> GlobalSceneTable::getAllTrunkIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_trunks.size());
    for (const auto& [id, _] : m_trunks) ids.push_back(id);
    return ids;
}

const SceneTable* GlobalSceneTable::getTable(const std::string& trunkId) const {
    auto it = m_trunks.find(trunkId);
    return it != m_trunks.end() ? &it->second->getTable() : nullptr;
}

} // namespace Core
} // namespace Nexus
