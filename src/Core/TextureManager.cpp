#include "TextureManager.h"
#include "ResourceLoader.h"
#include "Log.h"
#include "RenderSystem.h"

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

    ImageData whiteData;
    whiteData.width = 1;
    whiteData.height = 1;
    whiteData.channels = 4;
    whiteData.pixels = { 255, 255, 255, 255 };
    m_whiteTexture = m_context->createTexture(whiteData, TextureUsage::Sampled);
}

TextureManager::~TextureManager() {
    m_textures.clear();
    m_gcQueue.clear();
    m_defaultTexture.reset();
    m_whiteTexture.reset();
}

ITexture* TextureManager::getOrCreateTexture(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_textures.find(path);
        if (it != m_textures.end()) {
            it->second.refCount++;
            NX_CORE_INFO("[TextureDebug] Cache Hit: {} (Ref: {})", path, it->second.refCount);
            return it->second.texture.get();
        }
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

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_textures[path] = {std::move(texture), 1};
    }

    return ptr;
}

ITexture* TextureManager::createTextureFromMemory(const std::string& key, const ImageData& data) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_textures.find(key);
        if (it != m_textures.end()) {
            it->second.refCount++;
            NX_CORE_INFO("[TextureDebug] Cache Hit for Memory Texture: {} (Ref: {})", key, it->second.refCount);
            return it->second.texture.get();
        }
    }

    NX_CORE_INFO("[TextureDebug] Cache Miss for Memory Texture: {}", key);

    auto texture = m_context->createTexture(data, TextureUsage::Sampled);
    if (!texture) return nullptr;

    ITexture* ptr = texture.get();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_textures[key] = {std::move(texture), 1};
    }

    return ptr;
}

void TextureManager::addTexture(const std::string& key, std::unique_ptr<ITexture> texture) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_textures[key] = {std::move(texture), 1};
}

void TextureManager::removeTexture(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_textures.find(key);
    if (it != m_textures.end()) {
        it->second.refCount--;
        if (it->second.refCount <= 0) {
            uint64_t currentFrame = m_renderer ? m_renderer->getFrameCount() : 0;
            m_gcQueue.push_back({std::move(it->second.texture), currentFrame + 3});
            m_textures.erase(it);
        }
    }
}

void TextureManager::performGarbageCollection() {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t currentFrame = m_renderer ? m_renderer->getFrameCount() : 0;

    for (auto it = m_gcQueue.begin(); it != m_gcQueue.end();) {
        if (it->texture->isUploading()) {
            it->targetFrame = currentFrame + 3;
            ++it;
        } else if (currentFrame >= it->targetFrame) {
            it = m_gcQueue.erase(it);
        } else {
            ++it;
        }
    }
}

ITexture* TextureManager::getDefaultTexture() {
    return m_defaultTexture.get();
}

ITexture* TextureManager::getWhiteTexture() {
    return m_whiteTexture.get();
}

} // namespace Core
} // namespace Nexus
