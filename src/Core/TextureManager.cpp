#include "TextureManager.h"
#include "ResourceLoader.h"
#include "Log.h"

namespace Nexus {
namespace Core {

TextureManager::TextureManager(IContext* context) : m_context(context) {
    ImageData defaultData;
    defaultData.width = 2;
    defaultData.height = 2;
    defaultData.channels = 4;
    defaultData.pixels = {
        255, 0, 255, 255,   0, 0, 0, 255,
        0, 0, 0, 255,       255, 0, 255, 255
    };
    m_defaultTexture = m_context->createTexture(defaultData, TextureUsage::Sampled);
}

TextureManager::~TextureManager() {
    m_textures.clear();
    m_defaultTexture.reset();
}

ITexture* TextureManager::getOrCreateTexture(const std::string& path) {
    auto it = m_textures.find(path);
    if (it != m_textures.end()) {
        NX_CORE_INFO("[TextureDebug] Cache Hit: {}", path);
        return it->second.get();
    }
    NX_CORE_INFO("[TextureDebug] Cache Miss: {}", path);

    auto imgRes = ResourceLoader::loadImage(path);
    if (!imgRes.ok()) {
        NX_CORE_WARN("TextureManager: Failed to load texture from disk: {}", path);
        return nullptr;
    }

    auto texture = m_context->createTexture(imgRes.value(), TextureUsage::Sampled);
    if (!texture) {
        NX_CORE_ERROR("TextureManager: Failed to create RHI texture for: {}", path);
        return nullptr;
    }

    ITexture* ptr = texture.get();
    m_textures[path] = std::move(texture);
    return ptr;
}

ITexture* TextureManager::createTextureFromMemory(const std::string& key, const ImageData& data) {
    auto it = m_textures.find(key);
    if (it != m_textures.end()) {
        NX_CORE_INFO("[TextureDebug] Cache Hit for Memory Texture: {}", key);
        return it->second.get();
    }
    NX_CORE_INFO("[TextureDebug] Cache Miss for Memory Texture: {}", key);

    auto texture = m_context->createTexture(data, TextureUsage::Sampled);
    if (!texture) return nullptr;

    ITexture* ptr = texture.get();
    m_textures[key] = std::move(texture);
    return ptr;
}

void TextureManager::addTexture(const std::string& key, std::unique_ptr<ITexture> texture) {
    m_textures[key] = std::move(texture);
}

ITexture* TextureManager::getDefaultTexture() {
    return m_defaultTexture.get();
}

} // namespace Core
} // namespace Nexus
