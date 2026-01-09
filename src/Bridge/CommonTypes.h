#pragma once
#include <cstdint>
#include <array>
#include <vector>

namespace Nexus {

class ITexture;

struct ImageData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    std::vector<uint8_t> pixels;
};

enum class TextureUsage {
    Sampled,
    Storage,
    Attachment
};

struct Offset2D {
    int32_t x;
    int32_t y;
};

struct Extent2D {
    uint32_t width;
    uint32_t height;
};

struct Rect2D {
    Offset2D offset;
    Extent2D extent;
};

struct Viewport {
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
};
enum class TextureFormat {
    Unknown,
    RGBA8_UNORM,
    BGRA8_UNORM,
    D32_SFLOAT,
    BC7_UNORM_BLOCK,
    R8G8B8A8_UNORM,
    R8G8B8A8_SRGB
};

enum class ImageLayout {
    Undefined,
    ColorAttachmentOptimal,
    ShaderReadOnlyOptimal,
    TransferSrcOptimal,
    TransferDstOptimal,
    PresentSrc
};

enum class AttachmentLoadOp {
    Load,
    Clear,
    DontCare
};

enum class AttachmentStoreOp {
    Store,
    DontCare
};

struct ClearColorValue {
    std::array<float, 4> float32;
};

struct ClearValue {
    ClearColorValue color;
};

struct RenderingAttachment {
    ITexture* texture;
    ImageLayout layout;
    AttachmentLoadOp loadOp;
    AttachmentStoreOp storeOp;
    ClearValue clearValue;
};

struct RenderingInfo {
    Rect2D renderArea;
    uint32_t layerCount = 1;
    std::vector<RenderingAttachment> colorAttachments;
};

enum class PipelineBindPoint {
    Graphics,
    Compute
};

enum class IndexType {
    Uint16,
    Uint32
};

/**
 * @brief 间接绘制指令
 */
struct DrawIndexedIndirectCommand {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  vertexOffset;
    uint32_t firstInstance;
};

/**
 * @brief 非索引间接绘制指令
 */
struct DrawIndirectCommand {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

/**
 * @brief Mesh Shader 间接绘制指令
 */
struct DrawMeshTasksIndirectCommand {
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
};

} // namespace Nexus
