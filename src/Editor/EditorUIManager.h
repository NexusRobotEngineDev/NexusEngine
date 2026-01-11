#pragma once

#include "Panel.h"
#include <RmlUi/Core.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace Nexus {

class VK_UIBridge;

/**
 * @brief 编辑器 UI 管理器，负责面板的拖拽、吸附与布局持久化
 */
class EditorUIManager : public Rml::EventListener {
public:
    EditorUIManager() = default;
    ~EditorUIManager() = default;

    /**
     * @brief 初始化编辑器 UI，加载布局文档
     */
    bool initialize(VK_UIBridge* uiBridge);

    /**
     * @brief 关闭编辑器并清理资源
     */
    void shutdown();

    /**
     * @brief 注册一个面板到管理器
     */
    void registerPanel(std::unique_ptr<Panel> panel, const std::string& defaultDockZone);

    /**
     * @brief 将面板设为浮动状态
     */
    void floatPanel(const std::string& panelId, float x, float y);

    /**
     * @brief 将面板吸附到指定 Dock 区域
     */
    void dockPanel(const std::string& panelId, const std::string& dockZoneId);

    /**
     * @brief 保存当前布局到 JSON 文件
     */
    bool saveLayout(const std::string& filePath);

    /**
     * @brief 从 JSON 文件中恢复布局
     */
    bool loadLayout(const std::string& filePath);

    void ProcessEvent(Rml::Event& event) override;

private:
    void setupDragListeners();
    Rml::Element* findDockZoneAtPosition(float x, float y);

    VK_UIBridge* m_uiBridge = nullptr;
    Rml::ElementDocument* m_editorDoc = nullptr;
    std::vector<std::unique_ptr<Panel>> m_panels;
    std::unordered_map<std::string, Rml::Element*> m_dockZones;

    Rml::Element* m_draggedElement = nullptr;
    float m_dragStartX = 0;
    float m_dragStartY = 0;
};

} // namespace Nexus
