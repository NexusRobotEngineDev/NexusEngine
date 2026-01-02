#include "Context.h"

namespace Nexus {

#if GRAPHICS_BACKEND_COUNT == 1 && ENABLE_VULKAN
/**
 * @brief 创建上下文
 * @return 返回上下文指针
 */
ContextPtr CreateContext() {
    return new VK_Context();
}
#else
/**
 * @brief 创建上下文
 * @return 返回上下文指针
 */
ContextPtr CreateContext() {
    return nullptr;
}
#endif

#if WINDOW_BACKEND_COUNT == 1 && ENABLE_SDL
/**
 * @brief 创建窗口
 * @return 返回窗口指针
 */
WindowPtr CreateNativeWindow() {
    return new SDL_Window_Wrapper();
}
#else
/**
 * @brief 创建窗口
 * @return 返回窗口指针
 */
WindowPtr CreateNativeWindow() {
    return nullptr;
}
#endif

} // namespace Nexus
