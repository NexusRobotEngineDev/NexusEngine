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

static EditorUIManager* s_instance = nullptr;

EditorUIManager* EditorUIManager::Get() {
    return s_instance;
}

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
    s_instance = this;

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
    if (s_instance == this) s_instance = nullptr;
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
    }
    else if (event.GetId() == Rml::EventId::Change) {
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

void EditorUIManager::update(Scene* scene, float dt) {
    if (!scene || !m_editorDoc) return;
    m_currentScene = scene;
    processUICommands();

    static int s_frameCount = 0;
    static float s_timeAcc = 0.9f;

    s_frameCount++;
    s_timeAcc += dt;
    if (s_timeAcc >= 1.0f) {
        g_RenderStats_FPS.store(s_frameCount / s_timeAcc, std::memory_order_relaxed);
        s_frameCount = 0;
        s_timeAcc = 0.0f;
    }

}

} // namespace Nexus

#endif
