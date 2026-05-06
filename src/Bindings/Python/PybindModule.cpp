#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include "PybindUI.h"
#include "PybindECS.h"
#include "../../Bridge/Log.h"
#include <atomic>

namespace py = pybind11;

namespace Nexus {
    extern std::atomic<uint32_t> g_RenderStats_DrawCalls;
    extern std::atomic<uint32_t> g_RenderStats_APIDraws;
    extern std::atomic<uint32_t> g_RenderStats_Triangles;
    extern std::atomic<float> g_RenderStats_FPS;
    extern std::atomic<float> g_RenderStats_FrameTime;
    std::function<void(float)> g_pythonUpdateCallback;
}

PYBIND11_EMBEDDED_MODULE(nexus_engine, m) {
    m.doc() = "Nexus Engine Python Bindings (Embedded)";

    m.def("log_info", [](const std::string& msg) { NX_CORE_INFO("Python: {}", msg); });
    m.def("log_warn", [](const std::string& msg) { NX_CORE_WARN("Python: {}", msg); });
    m.def("log_error", [](const std::string& msg) { NX_CORE_ERROR("Python: {}", msg); });

    m.def("set_update_callback", [](std::function<void(float)> cb) {
        Nexus::g_pythonUpdateCallback = cb;
    });

    m.def("get_fps", []() { return Nexus::g_RenderStats_FPS.load(std::memory_order_relaxed); });
    m.def("get_frame_time", []() { return Nexus::g_RenderStats_FrameTime.load(std::memory_order_relaxed); });
    m.def("get_draw_calls", []() { return Nexus::g_RenderStats_DrawCalls.load(std::memory_order_relaxed); });
    m.def("get_api_draws", []() { return Nexus::g_RenderStats_APIDraws.load(std::memory_order_relaxed); });
    m.def("get_triangles", []() { return Nexus::g_RenderStats_Triangles.load(std::memory_order_relaxed); });

    Nexus::BindUI(m);
    Nexus::BindECS(m);
}
