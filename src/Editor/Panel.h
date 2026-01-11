#pragma once

#include <string>

namespace Nexus {

/**
 * @brief 编辑器面板基类，每个面板对应一个 RmlUi DOM 元素
 */
class Panel {
public:
    Panel(const std::string& id, const std::string& title)
        : m_id(id), m_title(title) {}

    virtual ~Panel() = default;

    const std::string& getId() const { return m_id; }
    const std::string& getTitle() const { return m_title; }

    void setDocked(bool docked) { m_isDocked = docked; }
    bool isDocked() const { return m_isDocked; }

    void setDockZone(const std::string& zone) { m_dockZone = zone; }
    const std::string& getDockZone() const { return m_dockZone; }

protected:
    std::string m_id;
    std::string m_title;
    bool m_isDocked = true;
    std::string m_dockZone;
};

} // namespace Nexus
