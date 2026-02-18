#pragma once

#include "Base.h"
#include "../Bridge/Interfaces.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace Nexus {
namespace Core {

/**
 * @brief 纹理管理器，负责纹理的生命周期管理和缓存
 */
class TextureManager {
public:
    TextureManager(IContext* context);
    ~TextureManager();

    /**
     * @brief 获取或创建纹理
     * @param path 纹理路径
     * @return 纹理指针（由管理器持有）
     */
    ITexture* getOrCreateTexture(const std::string& path);

    /**
     * @brief 从内存创建纹理并缓存
     * @param key 缓存键（如 modelPath + "#" + index）
     * @param data 图像数据
     * @return 纹理指针
     */
    ITexture* createTextureFromMemory(const std::string& key, const ImageData& data);

    /**
     * @brief 手动添加外部创建的纹理
     */
    void addTexture(const std::string& key, std::unique_ptr<ITexture> texture);

    /**
     * @brief 移除特定的纹理缓存（用于流式瓦片销毁）
     * @param key 缓存键
     */
    void removeTexture(const std::string& key);

    /**
     * @brief 获取默认纹理（fallback）
     */
    ITexture* getDefaultTexture();

    /**
     * @brief 获取纯白纹理（适用于没有贴图但只有颜色的材质）
     */
    ITexture* getWhiteTexture();

private:
    IContext* m_context;
    std::mutex m_mutex;
    std::unordered_map<std::string, std::unique_ptr<ITexture>> m_textures;
    std::unique_ptr<ITexture> m_defaultTexture;
    std::unique_ptr<ITexture> m_whiteTexture;
};

} // namespace Core
} // namespace Nexus
