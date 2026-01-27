#include "EditorUIManager.h"
#include "../Bridge/Vk/VK_UIBridge.h"
#include "../Bridge/Log.h"
#include "../Bridge/ResourceLoader.h"
#include "../Core/Scene.h"
#include "../Core/Components.h"
#include "../Bridge/Entity.h"
#include <nlohmann/json.hpp>
#include <fstream>

#ifdef ENABLE_RMLUI

namespace Nexus {

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

    Rml::Element* body = m_editorDoc->GetFirstChild();
    if (body) {
        body->AppendChild(panelEl->GetParentNode()->RemoveChild(panelEl));
    }

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

    if (panelEl->GetParentNode()) {
        panelEl->GetParentNode()->RemoveChild(panelEl);
    }
    it->second->AppendChild(Rml::ElementPtr(panelEl));

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
            if (!panelId.empty()) {
                floatPanel(panelId, m_dragStartX - 100.0f, m_dragStartY - 10.0f);
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

            if (targetZone && !panelId.empty()) {
                dockPanel(panelId, targetZone->GetId());
            }

            for (auto& [zoneId, zoneEl] : m_dockZones) {
                if (zoneEl) zoneEl->SetClass("highlight", false);
            }
            m_draggedElement = nullptr;
        }
    }
    else if (event.GetId() == Rml::EventId::Click) {
        if (target->IsClassSet("tree-node")) {
            auto entityIdStr = target->GetAttribute<Rml::String>("entity-id", "");
            if (!entityIdStr.empty() && m_currentScene) {
                uint32_t rawId = std::stoul(entityIdStr);
                m_selectedEntity = Entity(static_cast<entt::entity>(rawId), &m_currentScene->getRegistry());
            }
        }
    }
    else if (event.GetId() == Rml::EventId::Change) {
        if (m_selectedEntity.isValid() && m_selectedEntity.hasComponent<TransformComponent>()) {
            auto& transform = m_selectedEntity.getComponent<TransformComponent>();
            auto valStr = event.GetParameter<Rml::String>("value", "0.0");
            try {
                float val = std::stof(valStr);
                std::string targetId = target->GetId();
                if (targetId == "prop-pos-x") transform.position[0] = val;
                else if (targetId == "prop-pos-y") transform.position[1] = val;
                else if (targetId == "prop-pos-z") transform.position[2] = val;
            } catch (...) {}
        }
    }
    else if (event.GetId() == Rml::EventId::Focus) {
        target->SetClass("focused", true);
    }
    else if (event.GetId() == Rml::EventId::Blur) {
        target->SetClass("focused", false);
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

void EditorUIManager::update(Scene* scene) {
    if (!scene || !m_editorDoc) return;
    m_currentScene = scene;

    static int tickCount = 0;
    if (tickCount++ % 10 == 0) {
        Rml::Element* treeParent = m_editorDoc->GetElementById("hierarchy-tree");
        if (treeParent) {
            bool foundSelected = false;
            treeParent->SetInnerRML("");
            auto& registry = scene->getRegistry();
            auto view = registry.view<TagComponent>();

            for (auto ent : view) {
                Entity entity(ent, &registry);
                auto& tag = entity.getComponent<TagComponent>();

                auto nodePtr = m_editorDoc->CreateElement("div");
                nodePtr->SetClass("tree-node", true);
                if (m_selectedEntity == entity) {
                    nodePtr->SetClass("selected", true);
                    foundSelected = true;
                }

                nodePtr->SetInnerRML(tag.name);
                nodePtr->SetAttribute("entity-id", std::to_string(static_cast<uint32_t>(ent)));
                nodePtr->AddEventListener(Rml::EventId::Click, this);
                treeParent->AppendChild(std::move(nodePtr));
            }
            if (!foundSelected) {
                m_selectedEntity = {};
            }
        }
    }

    if (m_selectedEntity.isValid() && m_selectedEntity.hasComponent<TransformComponent>()) {
        auto& transform = m_selectedEntity.getComponent<TransformComponent>();
        auto* px = m_editorDoc->GetElementById("prop-pos-x");
        auto* py = m_editorDoc->GetElementById("prop-pos-y");
        auto* pz = m_editorDoc->GetElementById("prop-pos-z");

        if (px && py && pz && !px->IsClassSet("focused") && !py->IsClassSet("focused") && !pz->IsClassSet("focused")) {
            px->SetAttribute("value", std::to_string(transform.position[0]));
            py->SetAttribute("value", std::to_string(transform.position[1]));
            pz->SetAttribute("value", std::to_string(transform.position[2]));
        }
    }
}

} // namespace Nexus

#endif
