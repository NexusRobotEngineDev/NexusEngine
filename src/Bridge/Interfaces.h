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

    /**
     * @brief 获取指定名称刚体的世界坐标与旋转
     */
    virtual bool getBodyTransform(const std::string& name, std::array<float, 3>& outPos, std::array<float, 4>& outRot) = 0;

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
 * @brief 视觉数据桥接抽象层
 * 负责接收引擎后台提取到的原生离屏像素流或者内存句柄，实现各种与控制算法/外部接口的解耦传输。
 */
class IVisionBridge {
public:
    virtual ~IVisionBridge() = default;

    /**
     * @brief 推流离屏像素 (CPU 同步模式 / PBO 异步抽提的 CPU 版本)
     * @param mappedData 指向 CPU 或者映射后 RAM 数据的原生态字节流
     * @param size 数据长度
     * @param width 像素宽
     * @param height 像素高
     */
    virtual void streamImage(const void* mappedData, size_t size, int width, int height) = 0;
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
    virtual bool isUploading() const { return false; }
    virtual void setUploading(bool uploading) {}
};

class Registry;
class RenderSnapshot;

/**
 * @brief 渲染器接口
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual Status initialize() = 0;
    virtual Status renderFrame(RenderSnapshot* snapshot = nullptr) = 0;
    virtual void processEvent(const void* event) = 0;
    virtual Status onResize(uint32_t width, uint32_t height) = 0;
    virtual ICommandBuffer* getCurrentCommandBuffer() = 0;
    virtual ITexture* getSwapchainTexture(uint32_t index) = 0;
    virtual uint32_t acquireNextImage() = 0;
    virtual void present(uint32_t imageIndex) = 0;
    virtual void shutdown() = 0;
    virtual uint64_t getFrameCount() const = 0;
    virtual void updateMeshletBuffers(
        const void* meshletsData, size_t meshletsSize,
        const void* boundsData, size_t boundsSize,
        const void* verticesData, size_t verticesSize,
        const void* trianglesData, size_t trianglesSize) {};
    virtual bool isMeshletPipelineReady() const { return false; }
};

} // namespace Nexus
