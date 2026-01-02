#pragma once

#include "Base.h"
#include <string>

namespace Nexus {

/**
 * @brief 渲染上下文接口
 */
class IContext {
public:
    virtual ~IContext() = default;
    virtual Status initialize() = 0;
    virtual Status initializeWindowSurface(void* windowNativeHandle) = 0;
    virtual void sync() = 0;
    virtual void shutdown() = 0;
    virtual uint32_t getGraphicsQueueFamilyIndex() const = 0;
};

/**
 * @brief 窗口接口
 */
class IWindow {
public:
    virtual ~IWindow() = default;
    virtual Status initialize() = 0;
    virtual Status createWindow(const std::string& title, uint32_t width, uint32_t height) = 0;
    virtual void* getNativeHandle() const = 0;
    virtual void onEvent(const void* event) = 0;
    virtual uint32_t getWindowID() const = 0;
    virtual void shutdown() = 0;
    virtual bool shouldClose() const = 0;
};

/**
 * @brief 物理系统接口
 */
class IPhysicsSystem {
public:
    virtual ~IPhysicsSystem() = default;
    virtual Status initialize() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void shutdown() = 0;
};

} // namespace Nexus
