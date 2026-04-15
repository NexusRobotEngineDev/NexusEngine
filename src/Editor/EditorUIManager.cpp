#include "EditorUIManager.h"
#include "../Bridge/Vk/VK_UIBridge.h"
#include "../Bridge/Log.h"
#include "../Bridge/ResourceLoader.h"
#include "../Core/Scene.h"
#include "../Core/Components.h"
#include "../Bridge/Entity.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <mutex>

#ifdef ENABLE_RMLUI

namespace Nexus {



extern std::atomic<uint32_t> g_RenderStats_DrawCalls;
extern std::atomic<uint32_t> g_RenderStats_Triangles;
extern std::atomic<float> g_RenderStats_FPS;
extern std::atomic<float> g_RenderStats_FrameTime;
extern std::atomic<float> g_RenderStats_UITime;
extern std::atomic<float> g_RenderStats_LogicTime;
extern std::atomic<float> g_RenderStats_RenderSyncTime;
extern std::atomic<float> g_RenderStats_RenderPrepTime;
extern std::atomic<float> g_RenderStats_RenderDrawTime;

bool EditorUIManager::initialize(VK_UIBridge* uiBridge) {
    m_uiBridge = uiBridge;

    std::string docPath = ResourceLoader::getBasePath() + "Data/UI/editor_layout.rml";
    m_editorDoc = m_uiBridge->loadDocument(docPath);
    if (!m_editorDoc) {
        NX_CORE_ERROR("EditorUIManager: 无法加载编辑器布局: {}", docPath);
        return false;
    }

    m_dockZones["dock-left"] = m_editorDoc->GetElementById("dock-left");
    m_dockZones["dock-right"] = m_editorDoc->GetElementById("dock-right");

    setupEventListeners();

    NX_CORE_INFO("EditorUIManager 初始化成功");
    return true;
}

void EditorUIManager::shutdown() {
    m_panels.clear();
    m_dockZones.clear();
    m_editorDoc = nullptr;
}

void EditorUIManager::registerPanel(std::unique_ptr<Panel> panel, const std::string& defaultDockZone) {
    panel->setDockZone(defaultDockZone);
    panel->setDocked(true);
    m_panels.push_back(std::move(panel));
}

void EditorUIManager::floatPanel(const std::string& panelId, float x, float y) {
    if (!m_editorDoc) return;

    Rml::Element* panelEl = m_editorDoc->GetElementById(panelId);
    if (!panelEl) return;

    panelEl->SetClass("floating", true);
    panelEl->SetProperty("left", std::to_string((int)x) + "dp");
    panelEl->SetProperty("top", std::to_string((int)y) + "dp");

    for (auto& p : m_panels) {
        if (p->getId() == panelId) {
            p->setDocked(false);
            p->setDockZone("");
            break;
        }
    }
}

void EditorUIManager::dockPanel(const std::string& panelId, const std::string& dockZoneId) {
    if (!m_editorDoc) return;

    Rml::Element* panelEl = m_editorDoc->GetElementById(panelId);
    if (!panelEl) return;

    auto it = m_dockZones.find(dockZoneId);
    if (it == m_dockZones.end() || !it->second) return;

    panelEl->SetClass("floating", false);
    panelEl->RemoveProperty("left");
    panelEl->RemoveProperty("top");
    panelEl->RemoveProperty("position");

    Rml::Element* parent = panelEl->GetParentNode();
    if (parent && parent != it->second) {
        auto detached = parent->RemoveChild(panelEl);
        it->second->AppendChild(std::move(detached));
    }

    for (auto& p : m_panels) {
        if (p->getId() == panelId) {
            p->setDocked(true);
            p->setDockZone(dockZoneId);
            break;
        }
    }
}

void EditorUIManager::ProcessEvent(Rml::Event& event) {
    Rml::Element* target = event.GetTargetElement();
    if (!target) return;

    if (event.GetId() == Rml::EventId::Dragstart) {
        Rml::Element* panel = target->GetParentNode();
        if (panel && panel->IsClassSet("panel")) {
            m_draggedElement = panel;
            m_dragStartX = event.GetParameter("mouse_x", 0.0f);
            m_dragStartY = event.GetParameter("mouse_y", 0.0f);

            std::string panelId = panel->GetId();
            if (!panelId.empty() && panelId.size() < 32) {
                UICommand cmd{};
                cmd.type = UICommandType::DragFloat;
                strncpy(cmd.panelId, panelId.c_str(), 31);
                cmd.x = m_dragStartX - 100.0f;
                cmd.y = m_dragStartY - 10.0f;
                m_uiCommandQueue.push(cmd);
            }
        }
    }
    else if (event.GetId() == Rml::EventId::Drag) {
        if (m_draggedElement) {
            float mouseX = event.GetParameter("mouse_x", 0.0f);
            float mouseY = event.GetParameter("mouse_y", 0.0f);
            m_draggedElement->SetProperty("left", std::to_string((int)(mouseX - 100.0f)) + "dp");
            m_draggedElement->SetProperty("top", std::to_string((int)(mouseY - 10.0f)) + "dp");

            for (auto& [zoneId, zoneEl] : m_dockZones) {
                if (zoneEl) {
                    auto box = zoneEl->GetAbsoluteOffset(Rml::BoxArea::Border);
                    auto size = zoneEl->GetBox().GetSize(Rml::BoxArea::Border);
                    bool inside = mouseX >= box.x && mouseX <= box.x + size.x &&
                                  mouseY >= box.y && mouseY <= box.y + size.y;
                    zoneEl->SetClass("highlight", inside);
                }
            }
        }
    }
    else if (event.GetId() == Rml::EventId::Dragend) {
        if (m_draggedElement) {
            float mouseX = event.GetParameter("mouse_x", 0.0f);
            float mouseY = event.GetParameter("mouse_y", 0.0f);

            Rml::Element* targetZone = findDockZoneAtPosition(mouseX, mouseY);
            std::string panelId = m_draggedElement->GetId();

            if (targetZone && !panelId.empty() && panelId.size() < 32) {
                std::string targetId = targetZone->GetId();
                if (targetId.size() < 32) {
                    UICommand cmd{};
                    cmd.type = UICommandType::DragDock;
                    strncpy(cmd.panelId, panelId.c_str(), 31);
                    strncpy(cmd.targetZoneId, targetId.c_str(), 31);
                    m_uiCommandQueue.push(cmd);
                }
            }

            for (auto& [zoneId, zoneEl] : m_dockZones) {
                if (zoneEl) zoneEl->SetClass("highlight", false);
            }
            m_draggedElement = nullptr;
        }
    }
    else if (event.GetId() == Rml::EventId::Click) {
        float mouseX = event.GetParameter("mouse_x", 0.0f);
        float mouseY = event.GetParameter("mouse_y", 0.0f);

        Rml::Element* clickedTreeNode = nullptr;

        Rml::Element* node = target;
        while (node) {
            if (node->IsClassSet("tree-node")) {
                clickedTreeNode = node;
                break;
            }
            node = node->GetParentNode();
        }

        if (!clickedTreeNode && target->GetTagName() == "slidertrack") {
            Rml::ElementList treeNodes;
            m_editorDoc->GetElementsByClassName(treeNodes, "tree-node");
            for (auto* tn : treeNodes) {
                auto offset = tn->GetAbsoluteOffset(Rml::BoxArea::Border);
                auto size = tn->GetBox().GetSize(Rml::BoxArea::Border);
                if (mouseY >= offset.y && mouseY <= offset.y + size.y &&
                    mouseX >= offset.x && mouseX <= offset.x + size.x) {
                    clickedTreeNode = tn;
                    break;
                }
            }
        }

        if (clickedTreeNode) {
            auto entityIdStr = clickedTreeNode->GetAttribute<Rml::String>("entity-id", "");
            if (!entityIdStr.empty()) {
                uint32_t rawId = std::stoul(entityIdStr);
                m_uiCommandQueue.push({UICommandType::Select, rawId});
                m_uiCommandQueue.push({UICommandType::ToggleExpand, rawId});
            }
        }
    }
    else if (event.GetId() == Rml::EventId::Change) {
        std::string targetId = target->GetId();
        auto valStr = event.GetParameter<Rml::String>("value", "0.0");
        NX_CORE_INFO("EditorUIManager: Change event fired for '{}' with value '{}'", targetId, valStr);

        if (m_selectedEntity.isValid() && m_selectedEntity.hasComponent<TransformComponent>()) {
            auto& transform = m_selectedEntity.getComponent<TransformComponent>();
            try {
                float val = std::stof(valStr);
                if (targetId == "prop-pos-x") transform.position[0] = val;
                else if (targetId == "prop-pos-y") transform.position[1] = val;
                else if (targetId == "prop-pos-z") transform.position[2] = val;
            } catch (...) {
                NX_CORE_WARN("EditorUIManager: Failed to parse float from '{}'", valStr);
            }
        } else {
            NX_CORE_WARN("EditorUIManager: Change event but no valid selected entity!");
        }
    }
    else if (event.GetId() == Rml::EventId::Focus) {
        target->SetClass("focused", true);
    }
    else if (event.GetId() == Rml::EventId::Blur) {
        target->SetClass("focused", false);
    }
    else if (event.GetId() == Rml::EventId::Keydown) {
        if (target->GetTagName() == "input") {
            auto key = (Rml::Input::KeyIdentifier)event.GetParameter<int>("key_identifier", 0);
            if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER) {
                target->Blur();
            }
        }
    }
}

void EditorUIManager::setupEventListeners() {
    if (!m_editorDoc) return;

    Rml::ElementList titleBars;
    m_editorDoc->GetElementsByClassName(titleBars, "title-bar");

    for (auto* titleBar : titleBars) {
        titleBar->AddEventListener(Rml::EventId::Dragstart, this);
        titleBar->AddEventListener(Rml::EventId::Drag, this);
        titleBar->AddEventListener(Rml::EventId::Dragend, this);
    }

    m_editorDoc->AddEventListener(Rml::EventId::Click, this, true);
    m_editorDoc->AddEventListener(Rml::EventId::Change, this, true);
    m_editorDoc->AddEventListener(Rml::EventId::Focus, this, true);
    m_editorDoc->AddEventListener(Rml::EventId::Blur, this, true);
    m_editorDoc->AddEventListener(Rml::EventId::Keydown, this, true);
}

Rml::Element* EditorUIManager::findDockZoneAtPosition(float x, float y) {
    for (auto& [zoneId, zoneEl] : m_dockZones) {
        if (!zoneEl) continue;
        auto box = zoneEl->GetAbsoluteOffset(Rml::BoxArea::Border);
        auto size = zoneEl->GetBox().GetSize(Rml::BoxArea::Border);
        if (x >= box.x && x <= box.x + size.x && y >= box.y && y <= box.y + size.y) {
            return zoneEl;
        }
    }
    return nullptr;
}

bool EditorUIManager::saveLayout(const std::string& filePath) {
    std::string fullPath = ResourceLoader::getBasePath() + filePath;

    nlohmann::json j;
    nlohmann::json panelsArray = nlohmann::json::array();

    for (auto& p : m_panels) {
        nlohmann::json panelObj;
        panelObj["id"] = p->getId();
        panelObj["docked"] = p->isDocked();
        panelObj["dockZone"] = p->getDockZone();
        panelObj["floatX"] = p->getFloatX();
        panelObj["floatY"] = p->getFloatY();
        panelsArray.push_back(panelObj);
    }
    j["panels"] = panelsArray;

    std::ofstream file(fullPath);
    if (!file.is_open()) {
        NX_CORE_ERROR("EditorUIManager: 无法写入布局文件: {}", fullPath);
        return false;
    }
    file << j.dump(2);
    file.close();

    NX_CORE_INFO("EditorUIManager: 布局已保存: {}", fullPath);
    return true;
}

bool EditorUIManager::loadLayout(const std::string& filePath) {
    std::string fullPath = ResourceLoader::getBasePath() + filePath;

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        NX_CORE_INFO("EditorUIManager: 布局文件不存在, 使用默认布局: {}", fullPath);
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(file);
        file.close();

        if (!j.contains("panels") || !j["panels"].is_array()) {
            NX_CORE_WARN("EditorUIManager: 布局文件格式无效");
            return false;
        }

        for (auto& panelObj : j["panels"]) {
            std::string id = panelObj.value("id", "");
            bool docked = panelObj.value("docked", true);
            std::string dockZone = panelObj.value("dockZone", "");
            float floatX = panelObj.value("floatX", 0.0f);
            float floatY = panelObj.value("floatY", 0.0f);

            if (id.empty()) continue;

            if (docked && !dockZone.empty()) {
                dockPanel(id, dockZone);
            } else {
                floatPanel(id, floatX, floatY);
            }

            for (auto& p : m_panels) {
                if (p->getId() == id) {
                    p->setFloatPosition(floatX, floatY);
                    break;
                }
            }
        }

        NX_CORE_INFO("EditorUIManager: 布局已恢复: {}", fullPath);
        return true;
    } catch (const std::exception& e) {
        NX_CORE_ERROR("EditorUIManager: 布局文件解析失败: {}", e.what());
        return false;
    }
}

void EditorUIManager::processUICommands() {
    if (!m_currentScene) return;
    auto& reg = m_currentScene->getRegistry();

    UICommand cmd;
    while (m_uiCommandQueue.pop(cmd)) {
        try {
            switch (cmd.type) {
                case UICommandType::Select: {
                    entt::entity ent = static_cast<entt::entity>(cmd.entityId);
                    if (reg.getInternal().valid(ent)) {
                        m_selectedEntity = Entity(ent, &reg);
                    }
                    break;
                }
                case UICommandType::ToggleExpand: {
                    if (m_expandedEntities.count(cmd.entityId)) {
                        m_expandedEntities.erase(cmd.entityId);
                    } else {
                        m_expandedEntities.insert(cmd.entityId);
                    }
                    m_hierarchyDirty = true;
                    break;
                }
                case UICommandType::DragFloat: {
                    floatPanel(cmd.panelId, cmd.x, cmd.y);
                    break;
                }
                case UICommandType::DragDock: {
                    dockPanel(cmd.panelId, cmd.targetZoneId);
                    break;
                }
            }
        } catch (const std::exception& e) {
            NX_CORE_ERROR("MAIN UI THREAD EXCEPTION: {}", e.what());
        } catch (...) {
            NX_CORE_ERROR("MAIN UI THREAD UNKNOWN EXCEPTION");
        }
    }
}

void EditorUIManager::initCachedElements() {
    if (m_cache.initialized || !m_editorDoc) return;
    m_cache.hierarchyTree = m_editorDoc->GetElementById("hierarchy-tree");
    m_cache.entityId = m_editorDoc->GetElementById("prop-entity-id");
    m_cache.entityName = m_editorDoc->GetElementById("prop-entity-name");
    m_cache.posX = m_editorDoc->GetElementById("prop-pos-x");
    m_cache.posY = m_editorDoc->GetElementById("prop-pos-y");
    m_cache.posZ = m_editorDoc->GetElementById("prop-pos-z");
    m_cache.rot = m_editorDoc->GetElementById("prop-rot");
    m_cache.scale = m_editorDoc->GetElementById("prop-scale");
    m_cache.worldX = m_editorDoc->GetElementById("prop-world-x");
    m_cache.worldY = m_editorDoc->GetElementById("prop-world-y");
    m_cache.worldZ = m_editorDoc->GetElementById("prop-world-z");
    m_cache.parentName = m_editorDoc->GetElementById("prop-parent-name");
    m_cache.drawCalls = m_editorDoc->GetElementById("prop-draw-calls");
    m_cache.triangles = m_editorDoc->GetElementById("prop-triangles");
    m_cache.fps = m_editorDoc->GetElementById("prop-fps");
    m_cache.frameTime = m_editorDoc->GetElementById("prop-frame-time");
    m_cache.uiTime = m_editorDoc->GetElementById("prop-ui-time");
    m_cache.logicTime = m_editorDoc->GetElementById("prop-logic-time");
    m_cache.renderSyncTime = m_editorDoc->GetElementById("prop-render-sync-time");
    m_cache.renderPrepTime = m_editorDoc->GetElementById("prop-render-prep-time");
    m_cache.renderDrawTime = m_editorDoc->GetElementById("prop-render-draw-time");
    m_cache.initialized = true;
}

void EditorUIManager::update(Scene* scene) {
    if (!scene) {
        NX_CORE_WARN("EditorUIManager::update called with null scene!");
        return;
    }
    if (!m_editorDoc) {
        NX_CORE_WARN("EditorUIManager::update called but m_editorDoc is null!");
        return;
    }
    m_currentScene = scene;
    initCachedElements();


    processUICommands();

    if (++m_updateThrottle < 2) {
        return;
    }
    m_updateThrottle = 0;

    Rml::Element* treeParent = m_cache.hierarchyTree;
    if (treeParent) {
        auto& registry = scene->getRegistry();
        auto view = registry.view<TagComponent>();

        int currentCount = 0;
        for ([[maybe_unused]] auto ent : view) ++currentCount;

        static int lastCount = -1;
        static uint32_t lastSelectedId = 0xFFFFFFFF;
        uint32_t currentSelectedId = m_selectedEntity.isValid() ? (uint32_t)m_selectedEntity.getHandle() : 0xFFFFFFFF;

        static auto lastRebuildTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        bool timeToRebuild = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRebuildTime).count() > 500;

        if (timeToRebuild && currentCount != lastCount) {
            m_hierarchyDirty = true;
            lastCount = currentCount;
            lastRebuildTime = now;
        }

        if (m_hierarchyDirty) {
            m_hierarchyDirty = false;
            bool foundSelected = false;
            treeParent->SetInnerRML("");

            auto appendEntity = [&](auto& self, entt::entity ent, int depth) -> void {
                Entity entity(ent, &registry);
                if (!entity.hasComponent<TagComponent>()) return;
                auto& tag = entity.getComponent<TagComponent>();

                bool hasChildren = false;
                int childCount = 0;
                if (entity.hasComponent<HierarchyComponent>()) {
                    auto& hc = entity.getComponent<HierarchyComponent>();
                    hasChildren = !hc.children.empty();
                    childCount = (int)hc.children.size();
                }

                bool isCesiumRoot = (tag.name == "CesiumTiles");

                uint32_t entId = static_cast<uint32_t>(ent);
                bool isExpanded = m_expandedEntities.count(entId) > 0;

                auto nodePtr = m_editorDoc->CreateElement("div");
                nodePtr->SetClass("tree-node", true);
                if (m_selectedEntity == entity) {
                    nodePtr->SetClass("selected", true);
                    foundSelected = true;
                }

                if (depth > 0) {
                    nodePtr->SetProperty("padding-left", std::to_string(10 + depth * 15) + "dp");
                }

                std::string prefix;
                if (isCesiumRoot && hasChildren) {
                    prefix = isExpanded ? "[-] " : "[+] ";
                    nodePtr->SetInnerRML(prefix + tag.name + " (" + std::to_string(childCount) + " tiles)");
                } else if (hasChildren) {
                    prefix = isExpanded ? "[-] " : "[+] ";
                    nodePtr->SetInnerRML(prefix + tag.name);
                } else {
                    prefix = "    ";
                    nodePtr->SetInnerRML(prefix + tag.name);
                }
                nodePtr->SetAttribute("entity-id", std::to_string(entId));
                treeParent->AppendChild(std::move(nodePtr));

                if (hasChildren && isExpanded && !isCesiumRoot) {
                    auto& hc = entity.getComponent<HierarchyComponent>();
                    for (auto child : hc.children) {
                        if (registry.getInternal().valid(child)) {
                            self(self, child, depth + 1);
                        }
                    }
                }
            };

            for (auto ent : view) {
                Entity entity(ent, &registry);
                bool isRoot = true;
                if (entity.hasComponent<HierarchyComponent>()) {
                    isRoot = (entity.getComponent<HierarchyComponent>().parent == entt::null);
                }
                if (isRoot) {
                    appendEntity(appendEntity, ent, 0);
                }
            }
            if (!foundSelected) {
                m_selectedEntity = {};
                currentSelectedId = 0xFFFFFFFF;
            }
        }

        if (currentSelectedId != lastSelectedId) {
            Rml::ElementList nodes;
            m_editorDoc->GetElementsByClassName(nodes, "tree-node");
            for (auto* node : nodes) {
                auto entityIdStr = node->GetAttribute<Rml::String>("entity-id", "");
                if (!entityIdStr.empty()) {
                    uint32_t rawId = std::stoul(entityIdStr);
                    node->SetClass("selected", rawId == currentSelectedId);
                }
            }
            lastSelectedId = currentSelectedId;
            NX_CORE_INFO("EditorUIManager: Selection changed to entity {}", currentSelectedId);
        }
    } else {
        static int debugLogTick = 0;
        if (debugLogTick++ % 100 == 0) {
            NX_CORE_WARN("EditorUIManager: Failed to locate 'hierarchy-tree' element in RML document!");
        }
    }

    auto* lId = m_cache.entityId;
    auto* lName = m_cache.entityName;

    auto setCachedRML = [](Rml::Element* el, const std::string& text, std::string& cache) {
        if (el && cache != text) { el->SetInnerRML(text); cache = text; }
    };
    static std::string cache_lId, cache_lName, cache_rot, cache_scl, cache_wx, cache_wy, cache_wz, cache_pn;

    if (m_selectedEntity.isValid()) {
        setCachedRML(lId, std::to_string(static_cast<uint32_t>(m_selectedEntity.getHandle())), cache_lId);
        if (m_selectedEntity.hasComponent<TagComponent>()) setCachedRML(lName, m_selectedEntity.getComponent<TagComponent>().name, cache_lName);
    } else {
        setCachedRML(lId, "-", cache_lId);
        setCachedRML(lName, "None", cache_lName);

        auto* px = m_cache.posX;
        auto* py = m_cache.posY;
        auto* pz = m_cache.posZ;
        if (px && !px->IsClassSet("focused")) px->SetAttribute("value", "");
        if (py && !py->IsClassSet("focused")) py->SetAttribute("value", "");
        if (pz && !pz->IsClassSet("focused")) pz->SetAttribute("value", "");

        setCachedRML(m_cache.rot, "-", cache_rot);
        setCachedRML(m_cache.scale, "-", cache_scl);
        setCachedRML(m_cache.worldX, "-", cache_wx);
        setCachedRML(m_cache.worldY, "-", cache_wy);
        setCachedRML(m_cache.worldZ, "-", cache_wz);
        setCachedRML(m_cache.parentName, "-", cache_pn);
    }

    if (m_selectedEntity.isValid() && m_selectedEntity.hasComponent<TransformComponent>()) {
        auto& transform = m_selectedEntity.getComponent<TransformComponent>();
        auto* px = m_cache.posX;
        auto* py = m_cache.posY;
        auto* pz = m_cache.posZ;

        if (px && py && pz && !px->IsClassSet("focused") && !py->IsClassSet("focused") && !pz->IsClassSet("focused")) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.4f", transform.position[0]); px->SetAttribute("value", std::string(buf));
            snprintf(buf, sizeof(buf), "%.4f", transform.position[1]); py->SetAttribute("value", std::string(buf));
            snprintf(buf, sizeof(buf), "%.4f", transform.position[2]); pz->SetAttribute("value", std::string(buf));
        }

        if (m_cache.rot) {
            char buf[96];
            snprintf(buf, sizeof(buf), "(%.3f, %.3f, %.3f, %.3f)", transform.rotation[0], transform.rotation[1], transform.rotation[2], transform.rotation[3]);
            setCachedRML(m_cache.rot, buf, cache_rot);
        }

        if (m_cache.scale) {
            char buf[64];
            snprintf(buf, sizeof(buf), "(%.3f, %.3f, %.3f)", transform.scale[0], transform.scale[1], transform.scale[2]);
            setCachedRML(m_cache.scale, buf, cache_scl);
        }

        if (m_cache.worldX && m_cache.worldY && m_cache.worldZ) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.4f", transform.worldMatrix[12]); setCachedRML(m_cache.worldX, buf, cache_wx);
            snprintf(buf, sizeof(buf), "%.4f", transform.worldMatrix[13]); setCachedRML(m_cache.worldY, buf, cache_wy);
            snprintf(buf, sizeof(buf), "%.4f", transform.worldMatrix[14]); setCachedRML(m_cache.worldZ, buf, cache_wz);
        }

        if (m_cache.parentName) {
            if (m_selectedEntity.hasComponent<HierarchyComponent>()) {
                auto& hc = m_selectedEntity.getComponent<HierarchyComponent>();
                if (hc.parent != entt::null && m_currentScene) {
                    Entity parentEnt(hc.parent, &m_currentScene->getRegistry());
                    if (parentEnt.hasComponent<TagComponent>()) {
                        setCachedRML(m_cache.parentName, parentEnt.getComponent<TagComponent>().name, cache_pn);
                    } else {
                        setCachedRML(m_cache.parentName, "(id: " + std::to_string((uint32_t)hc.parent) + ")", cache_pn);
                    }
                } else {
                    setCachedRML(m_cache.parentName, "(root)", cache_pn);
                }
            } else {
                setCachedRML(m_cache.parentName, "(none)", cache_pn);
            }
        }
    }

    auto* drawCallsEl = m_cache.drawCalls;
    auto* trianglesEl = m_cache.triangles;
    if (drawCallsEl) {
        std::string s = std::to_string(g_RenderStats_DrawCalls.load(std::memory_order_relaxed));
        if (drawCallsEl->GetInnerRML() != s) drawCallsEl->SetInnerRML(s);
    }
    if (trianglesEl) {
        std::string s = std::to_string(g_RenderStats_Triangles.load(std::memory_order_relaxed));
        if (trianglesEl->GetInnerRML() != s) trianglesEl->SetInnerRML(s);
    }

    auto* fpsEl = m_cache.fps;
    auto* frameTimeEl = m_cache.frameTime;
    if (fpsEl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", g_RenderStats_FPS.load(std::memory_order_relaxed));
        if (fpsEl->GetInnerRML() != buf) fpsEl->SetInnerRML(buf);
    }
    if (frameTimeEl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f ms", g_RenderStats_FrameTime.load(std::memory_order_relaxed));
        if (frameTimeEl->GetInnerRML() != buf) frameTimeEl->SetInnerRML(buf);
    }

    auto* uiTimeEl = m_cache.uiTime;
    if (uiTimeEl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f ms", g_RenderStats_UITime.load(std::memory_order_relaxed));
        if (uiTimeEl->GetInnerRML() != buf) uiTimeEl->SetInnerRML(buf);
    }

    auto* logicTimeEl = m_cache.logicTime;
    auto* rsTimeEl = m_cache.renderSyncTime;
    auto* prepTimeEl = m_cache.renderPrepTime;
    auto* drawTimeEl = m_cache.renderDrawTime;

    if (logicTimeEl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f ms", g_RenderStats_LogicTime.load(std::memory_order_relaxed));
        if (logicTimeEl->GetInnerRML() != buf) logicTimeEl->SetInnerRML(buf);
    }
    if (rsTimeEl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f ms", g_RenderStats_RenderSyncTime.load(std::memory_order_relaxed));
        if (rsTimeEl->GetInnerRML() != buf) rsTimeEl->SetInnerRML(buf);
    }
    if (prepTimeEl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f ms", g_RenderStats_RenderPrepTime.load(std::memory_order_relaxed));
        if (prepTimeEl->GetInnerRML() != buf) prepTimeEl->SetInnerRML(buf);
    }
    if (drawTimeEl) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f ms", g_RenderStats_RenderDrawTime.load(std::memory_order_relaxed));
        if (drawTimeEl->GetInnerRML() != buf) drawTimeEl->SetInnerRML(buf);
    }
}

} // namespace Nexus

#endif
