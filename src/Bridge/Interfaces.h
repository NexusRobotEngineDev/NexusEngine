#pragma once

#include "Base.h"
#include "CommonTypes.h"
#include <string>
#include <vector>

namespace Nexus {

class IBuffer;
class ICommandBuffer;
class ITexture;

/**
 * @brief 渲染上下文接口
 */
class IContext {
public:
    virtual ~IContext() = default;
    virtual Status initialize() = 0;
    virtual Status initializeWindowSurface(void* windowNativeHandle) = 0;
    virtual Status initializeHeadless() = 0;
    virtual void sync() = 0;
    virtual void shutdown() = 0;
    virtual uint32_t getGraphicsQueueFamilyIndex() const = 0;
    virtual std::unique_ptr<IBuffer> createBuffer(uint64_t size, uint32_t usage, uint32_t properties) = 0;
    virtual std::unique_ptr<ITexture> createTexture(const ImageData& imageData, TextureUsage usage) = 0;
    virtual std::unique_ptr<ITexture> createTexture(uint32_t width, uint32_t height, TextureFormat format, TextureUsage usage) = 0;

    virtual IBuffer* getGlobalVertexBuffer() const { return nullptr; }
    virtual IBuffer* getGlobalIndexBuffer() const { return nullptr; }
};

/**
 * @brief 缓冲区接口
 */
class IBuffer {
public:
    virtual ~IBuffer() = default;
    virtual void* map() = 0;
    virtual void unmap() = 0;
    virtual uint64_t getSize() const = 0;
    virtual void* getNativeHandle() const = 0;
    virtual Status uploadData(const void* data, uint64_t size, uint64_t offset = 0) = 0;
};

/**
 * @brief 命令记录接口
 */
class ICommandBuffer {
public:
    virtual ~ICommandBuffer() = default;
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void transitionImageLayout(ITexture* texture, ImageLayout oldLayout, ImageLayout newLayout) = 0;
    virtual void beginRendering(const RenderingInfo& info) = 0;
    virtual void endRendering() = 0;
    virtual void setViewport(const Viewport& viewport) = 0;
    virtual void setScissor(const Rect2D& scissor) = 0;
    virtual void bindPipeline(PipelineBindPoint bindPoint, void* pipeline) = 0;
    virtual void bindVertexBuffers(uint32_t firstBinding, IBuffer* buffer, uint64_t offset) = 0;
    virtual void bindIndexBuffer(IBuffer* buffer, uint64_t offset, IndexType indexType) = 0;
    virtual void bindDescriptorSets(PipelineBindPoint bindPoint, void* layout, uint32_t firstSet, void* descriptorSet) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) = 0;
    virtual void drawIndexedIndirect(IBuffer* buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
    virtual void drawIndirect(IBuffer* buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
    virtual void drawMeshTasksIndirectEXT(IBuffer* buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
    virtual void copyTextureToBuffer(ITexture* texture, IBuffer* buffer) = 0;
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
    virtual void setEventCallback(std::function<void(const void*)> callback) = 0;
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
    virtual Status loadModel(const std::string& path) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void shutdown() = 0;
    virtual void forceStartPhysics() {}

    struct ContactData {
        std::array<float, 3> position;
        std::array<float, 3> normal;
        float depth;
        int geomIdx1{-1};
        int geomIdx2{-1};
    };

    struct GeomSyncData {
        int geomId;
        int type;
        float size[3];
        float pos[3];
        float rot[4];
    };

    /**
     * @brief 提取动物理核心（MuJoCo）侧活动的几何体信息
     */
    virtual void getActiveGeoms(std::vector<GeomSyncData>& outGeoms) {}

    /**
     * @brief 同步这些几何体到外部碰撞检测核心（Jolt）作为代理体
     */
    virtual void syncKinematicProxies(const std::vector<GeomSyncData>& geoms) {}

    /**
     * @brief 注入接触点数据，用于与外部碰撞引擎（如 Jolt）进行混合物理模拟同步
     */
    virtual void injectContacts(const std::vector<ContactData>& contacts) {}

    /**
     * @brief 获取指定名称刚体的世界坐标与旋转
     */
    virtual bool getBodyTransform(const std::string& name, std::array<float, 3>& outPos, std::array<float, 4>& outRot) = 0;

    /**
     * @brief 获取指定名称几何体的世界坐标与旋转及ID
     */
    virtual bool getGeomTransform(const std::string& name, std::array<float, 3>& outPos, std::array<float, 4>& outRot, int& outGeomId) { return false; }


    /**
     * @brief 为指定名称的关节施加 PD 控制与前馈力矩
     */
    virtual void setJointControl(const std::string& jointName, float q, float dq, float kp, float kd, float tau) = 0;

    /**
     * @brief 返回所有 actuator 名称列表（按 ID 顺序）
     */
    virtual std::vector<std::string> getActuatorNames() const = 0;
};

/**
 * @brief 纹理接口
 */
class ITexture {
public:
    virtual ~ITexture() = default;
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual TextureFormat getFormat() const = 0;
    virtual uint32_t getBindlessTextureIndex() const { return 0; }
    virtual uint32_t getBindlessSamplerIndex() const { return 0; }
};

class Registry;

/**
 * @brief 渲染器接口
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual Status initialize() = 0;
    virtual Status renderFrame(Registry* registry = nullptr) = 0;
    virtual void processEvent(const void* event) = 0;
    virtual Status onResize(uint32_t width, uint32_t height) = 0;
    virtual ICommandBuffer* getCurrentCommandBuffer() = 0;
    virtual ITexture* getSwapchainTexture(uint32_t index) = 0;
    virtual uint32_t acquireNextImage() = 0;
    virtual void present(uint32_t imageIndex) = 0;
    virtual void shutdown() = 0;
};

} // namespace Nexus
