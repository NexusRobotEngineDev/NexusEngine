#include "VK_RmlUi_System.h"
#include "../Log.h"

#ifdef ENABLE_RMLUI

namespace Nexus {

VK_RmlUi_System::VK_RmlUi_System() {}

VK_RmlUi_System::~VK_RmlUi_System() {}

double VK_RmlUi_System::GetElapsedTime() {
#ifdef ENABLE_SDL
    return static_cast<double>(SDL_GetTicks()) / 1000.0;
#else
    return 0.0;
#endif
}

bool VK_RmlUi_System::LogMessage(Rml::Log::Type type, const Rml::String& message) {
    switch (type) {
        case Rml::Log::LT_ERROR:
            NX_CORE_ERROR("RmlUi: {}", message);
            break;
        case Rml::Log::LT_ASSERT:
            NX_CORE_CRITICAL("RmlUi Assert: {}", message);
            break;
        case Rml::Log::LT_WARNING:
            NX_CORE_WARN("RmlUi: {}", message);
            break;
        case Rml::Log::LT_INFO:
            NX_CORE_INFO("RmlUi: {}", message);
            break;
        case Rml::Log::LT_DEBUG:
            NX_CORE_DEBUG("RmlUi: {}", message);
            break;
        case Rml::Log::LT_ALWAYS:
            NX_CORE_INFO("RmlUi: {}", message);
            break;
        default:
            NX_CORE_TRACE("RmlUi: {}", message);
            break;
    }
    return true;
}

} // namespace Nexus

#endif
