#pragma once

#include "../thirdparty.h"
#include "../Interfaces.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

#ifdef ENABLE_RMLUI

namespace Nexus {

/**
 * @brief RmlUi 的渲染接口实现
 */
class VK_RmlUi_Renderer : public Rml::RenderInterface {
public:
    VK_RmlUi_Renderer(IContext* context, IRenderer* renderer);
    virtual ~VK_RmlUi_Renderer();

    virtual Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
    virtual void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    virtual void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    virtual Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
    virtual Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
    virtual void ReleaseTexture(Rml::TextureHandle texture) override;

    virtual void EnableScissorRegion(bool enable) override;
    virtual void SetScissorRegion(Rml::Rectanglei region) override;

    virtual void SetTransform(const Rml::Matrix4f* transform) override;

    Status createPipeline(uint32_t width, uint32_t height);
    void updateWindowSize(int width, int height);
    void pumpDeferredDestruction();
    void beginRender();

private:
    Status createPipeline();

    IContext* m_context;
    IRenderer* m_renderer;

    int m_windowWidth = 0;
    int m_windowHeight = 0;

    vk::PipelineLayout m_pipelineLayout;
    vk::Pipeline m_pipeline;

    std::unique_ptr<ITexture> m_whiteTexture;

    bool m_scissorEnabled = false;
    vk::Rect2D m_scissorRect;

    std::vector<struct RmlGeometry*> m_geometryDeletionQueue[3];
    std::vector<std::unique_ptr<IBuffer>> m_bufferDeletionQueue[3];
    std::vector<ITexture*> m_textureDeletionQueue[3];
    int m_currentFrameIndex = 0;
    bool m_pipelineBound = false;
};

} // namespace Nexus

#endif
