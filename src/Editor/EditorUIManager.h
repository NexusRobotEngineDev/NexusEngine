#pragma once

#include "Panel.h"
#include "../Bridge/Entity.h"
#include <RmlUi/Core.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <set>
#include <array>
#include <atomic>

namespace Nexus {

class VK_UIBridge;
class Scene;

enum class UICommandType : uint8_t {
    Select,
    ToggleExpand,
};

struct UICommand {
    UICommandType type;
    uint32_t entityId;
};

template<typename T, size_t Cap>
class UICommandQueue {
public:
    bool push(const T& item) {
        size_t h = m_head.load(std::memory_order_relaxed);
        size_t next = (h + 1) % Cap;
        if (next == m_tail.load(std::memory_order_acquire)) return false;
        m_buf[h] = item;
        m_head.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& item) {
        size_t t = m_tail.load(std::memory_order_relaxed);
        if (t == m_head.load(std::memory_order_acquire)) return false;
        item = m_buf[t];
        m_tail.store((t + 1) % Cap, std::memory_order_release);
        return true;
    }
private:
    std::array<T, Cap> m_buf;
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};
};

/**
 * @brief 编辑器 UI 管理器，负责面板的拖拽、吸附与布局持久化
 */
class EditorUIManager : public Rml::EventListener {
public:
    EditorUIManager() = default;
    ~EditorUIManager() = default;

    bool initialize(VK_UIBridge* uiBridge);
    void shutdown();
    void registerPanel(std::unique_ptr<Panel> panel, const std::string& defaultDockZone);
    void floatPanel(const std::string& panelId, float x, float y);
    void dockPanel(const std::string& panelId, const std::string& dockZoneId);
    void update(Scene* scene);
    bool saveLayout(const std::string& filePath);
    bool loadLayout(const std::string& filePath);
    void ProcessEvent(Rml::Event& event) override;

private:
    void setupEventListeners();
    void processUICommands();
    Rml::Element* findDockZoneAtPosition(float x, float y);

    VK_UIBridge* m_uiBridge = nullptr;
    Rml::ElementDocument* m_editorDoc = nullptr;
    std::vector<std::unique_ptr<Panel>> m_panels;
    std::unordered_map<std::string, Rml::Element*> m_dockZones;

    Rml::Element* m_draggedElement = nullptr;
    float m_dragStartX = 0;
    float m_dragStartY = 0;

    Entity m_selectedEntity{};
    Scene* m_currentScene = nullptr;
    double m_lastUpdateTime = 0.0;
    std::set<uint32_t> m_expandedEntities;
    bool m_hierarchyDirty = true;

    UICommandQueue<UICommand, 128> m_uiCommandQueue;
};

} // namespace Nexus
