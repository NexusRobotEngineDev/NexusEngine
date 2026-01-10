#include "SDL_Window.h"
#include "Log.h"

namespace Nexus {

SDL_Window_Wrapper::SDL_Window_Wrapper() : m_window(nullptr), m_width(0), m_height(0), m_shouldClose(false) {}

SDL_Window_Wrapper::~SDL_Window_Wrapper() {
    shutdown();
}

/**
 * @brief SDL 日志回调函数
 */
static void SDLCALL SDLLogCallback(void* userdata, int category, SDL_LogPriority priority, const char* message) {
    switch (priority) {
        case SDL_LOG_PRIORITY_VERBOSE:
        case SDL_LOG_PRIORITY_DEBUG:
            NX_CORE_DEBUG("SDL: {}", message);
            break;
        case SDL_LOG_PRIORITY_INFO:
            NX_CORE_INFO("SDL: {}", message);
            break;
        case SDL_LOG_PRIORITY_WARN:
            NX_CORE_WARN("SDL: {}", message);
            break;
        case SDL_LOG_PRIORITY_ERROR:
        case SDL_LOG_PRIORITY_CRITICAL:
            NX_CORE_ERROR("SDL: {}", message);
            break;
        default:
            NX_CORE_TRACE("SDL: {}", message);
            break;
    }
}

Status SDL_Window_Wrapper::initialize() {
    SDL_SetLogOutputFunction(SDLLogCallback, nullptr);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        NX_CORE_ERROR("Failed to initialize SDL: {}", SDL_GetError());
        return InternalError(std::string("Failed to initialize SDL: ") + SDL_GetError());
    }
    NX_CORE_INFO("SDL Initialized successfully");
    return OkStatus();
}

Status SDL_Window_Wrapper::createWindow(const std::string& title, uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    m_window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!m_window) {
        NX_CORE_ERROR("Failed to create SDL window: {}", SDL_GetError());
        return InternalError(std::string("Failed to create SDL window: ") + SDL_GetError());
    }

    NX_CORE_INFO("SDL Window created: {} ({}x{})", title, width, height);
    return OkStatus();
}

void* SDL_Window_Wrapper::getNativeHandle() const {
    return m_window;
}

uint32_t SDL_Window_Wrapper::getWindowID() const {
    return m_window ? SDL_GetWindowID(m_window) : 0;
}

void SDL_Window_Wrapper::onEvent(const void* eventPtr) {
    const SDL_Event* event = static_cast<const SDL_Event*>(eventPtr);
    if (event->type == SDL_EVENT_QUIT) {
        m_shouldClose = true;
    }

    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == getWindowID()) {
        m_shouldClose = true;
    }

    if (event->type == SDL_EVENT_WINDOW_RESIZED && event->window.windowID == getWindowID()) {
        m_width = event->window.data1;
        m_height = event->window.data2;
    }

    if (m_eventCallback) {
        m_eventCallback(eventPtr);
    }
}

void SDL_Window_Wrapper::shutdown() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

bool SDL_Window_Wrapper::shouldClose() const {
    return m_shouldClose;
}

} // namespace Nexus
