#pragma once
#include "../Interfaces.h"
#include <SDL3/SDL.h>
#include <string>
#include <functional>

namespace Nexus {

/**
 * @brief SDL 窗口包装类
 */
class SDL_Window_Wrapper : public IWindow {
public:
    SDL_Window_Wrapper();
    virtual ~SDL_Window_Wrapper() override;

    /**
     * @brief 初始化SDL视频子系统
     */
    virtual Status initialize() override;

    /**
     * @brief 创建窗口
     */
    virtual Status createWindow(const std::string& title, uint32_t width, uint32_t height) override;

    /**
     * @brief 获取原生窗口句柄 (SDL_Window*)
     */
    virtual void* getNativeHandle() const override;

    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

    /**
     * @brief 处理单个 SDL 事件
     */
    virtual void onEvent(const void* event) override;

    /**
     * @brief 获取 SDL 窗口 ID
     */
    virtual uint32_t getWindowID() const override;

    /**
     * @brief 关闭窗口并清理资源
     */
    virtual void shutdown() override;

    /**
     * @brief 检查窗口是否请求关闭
     */
    virtual bool shouldClose() const override;

    virtual void setEventCallback(std::function<void(const void*)> callback) override {
        m_eventCallback = callback;
    }

private:
    SDL_Window* m_window = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_shouldClose = false;
    std::function<void(const void*)> m_eventCallback;
};

} // namespace Nexus
