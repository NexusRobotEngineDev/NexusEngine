#pragma once

#include "Base.h"
#include "../Bridge/Interfaces.h"
#include <string>
#include <unordered_map>
#include <memory>

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
     * @brief 获取默认纹理（fallback）
     */
    ITexture* getDefaultTexture();

private:
    IContext* m_context;
    std::unordered_map<std::string, std::unique_ptr<ITexture>> m_textures;
    std::unique_ptr<ITexture> m_defaultTexture;
};

} // namespace Core
} // namespace Nexus
