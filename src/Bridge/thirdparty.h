#pragma once

#ifdef ENABLE_SDL
#include <SDL3/SDL.h>
#endif

#ifdef ENABLE_VULKAN
#include <vulkan/vulkan.hpp>
#endif

#include <mujoco/mujoco.h>

#ifdef ENABLE_RMLUI
#include <RmlUi/Core.h>
#endif
