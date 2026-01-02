#pragma once
#include "Config.h"
#include "Interfaces.h"

#if ENABLE_VULKAN
    #include "Vk/VK_Context.h"
#endif

#if ENABLE_SDL
    #include "Sdl/SDL_Window.h"
#endif

#if ENABLE_MUJOCO
    #include "MuJoCo/MuJoCo_PhysicsSystem.h"
#endif

namespace Nexus {

#if GRAPHICS_BACKEND_COUNT == 1
    #if ENABLE_VULKAN
        using Context = ::Nexus::VK_Context;
        using ContextPtr = ::Nexus::VK_Context*;
    #elif ENABLE_DX12
    #endif
#else
    using Context = ::Nexus::IContext;
    using ContextPtr = ::Nexus::IContext*;
#endif

#if WINDOW_BACKEND_COUNT == 1
    #if ENABLE_SDL
        using Window = ::Nexus::SDL_Window_Wrapper;
        using WindowPtr = ::Nexus::SDL_Window_Wrapper*;
    #endif
#else
    using Window = ::Nexus::IWindow;
    using WindowPtr = ::Nexus::IWindow*;
#endif

#if PHYSICS_BACKEND_COUNT == 1
    #if ENABLE_MUJOCO
        using PhysicsSystem = ::Nexus::MuJoCo_PhysicsSystem;
        using PhysicsSystemPtr = ::Nexus::MuJoCo_PhysicsSystem*;
    #endif
#else
    using PhysicsSystem = ::Nexus::IPhysicsSystem;
    using PhysicsSystemPtr = ::Nexus::IPhysicsSystem*;
#endif

ContextPtr CreateContext();
WindowPtr CreateNativeWindow();

} // namespace Nexus
