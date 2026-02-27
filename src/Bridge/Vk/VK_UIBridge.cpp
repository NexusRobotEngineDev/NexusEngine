#include "VK_UIBridge.h"
#include "../Log.h"
#include "../ResourceLoader.h"

#ifdef ENABLE_RMLUI
#include <RmlUi/Debugger.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>

namespace Nexus {

extern std::atomic<float> g_RenderStats_UITime;

VK_UIBridge::VK_UIBridge(IContext* context, IRenderer* renderer)
    : m_context(context), m_renderer(renderer) {
}

VK_UIBridge::~VK_UIBridge() {
    shutdown();
}

bool VK_UIBridge::initialize(int windowWidth, int windowHeight) {
    m_systemInterface = std::make_unique<VK_RmlUi_System>();
    m_renderInterface = std::make_unique<VK_RmlUi_Renderer>(m_context, m_renderer);

    Rml::SetSystemInterface(m_systemInterface.get());
    Rml::SetRenderInterface(m_renderInterface.get());

    if (!Rml::Initialise()) {
        NX_CORE_ERROR("Failed to initialize RmlUi!");
        return false;
    }

    m_renderInterface->updateWindowSize(windowWidth, windowHeight);

    std::string fontPath = ResourceLoader::getBasePath() + "Data/UI/arial.ttf";
    if (!Rml::LoadFontFace(fontPath)) {
        NX_CORE_ERROR("Failed to load RmlUi font: {}", fontPath);
    }

    m_rmlContext = Rml::CreateContext("NexusEditor", Rml::Vector2i(windowWidth, windowHeight));
    if (!m_rmlContext) {
        NX_CORE_ERROR("Failed to create RmlUi Context!");
        return false;
    }

    NX_CORE_INFO("RmlUi Bridge Initialized.");
    return true;
}

void VK_UIBridge::shutdown() {
    if (m_rmlContext) {
        Rml::RemoveContext(m_rmlContext->GetName());
        m_rmlContext = nullptr;
    }

    Rml::Shutdown();

    m_renderInterface.reset();
    m_systemInterface.reset();
}

static Rml::Input::KeyIdentifier translateKey(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_BACKSPACE: return Rml::Input::KI_BACK;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER: return Rml::Input::KI_RETURN;
        case SDL_SCANCODE_LEFT: return Rml::Input::KI_LEFT;
        case SDL_SCANCODE_RIGHT: return Rml::Input::KI_RIGHT;
        case SDL_SCANCODE_UP: return Rml::Input::KI_UP;
        case SDL_SCANCODE_DOWN: return Rml::Input::KI_DOWN;
        case SDL_SCANCODE_DELETE: return Rml::Input::KI_DELETE;
        case SDL_SCANCODE_HOME: return Rml::Input::KI_HOME;
        case SDL_SCANCODE_END: return Rml::Input::KI_END;
        case SDL_SCANCODE_ESCAPE: return Rml::Input::KI_ESCAPE;
        case SDL_SCANCODE_TAB: return Rml::Input::KI_TAB;
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT: return Rml::Input::KI_LSHIFT;
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL: return Rml::Input::KI_LCONTROL;
        default: return Rml::Input::KI_UNKNOWN;
    }
}

void VK_UIBridge::drainEventQueue() {
    if (!m_rmlContext) return;

    size_t tail = m_eventTail.load(std::memory_order_relaxed);
    size_t head = m_eventHead.load(std::memory_order_acquire);

    while (tail != head) {
        const SDL_Event& event = m_eventBuf[tail % EVENT_QUEUE_CAP];

        int keyModifier = 0;
        SDL_Keymod mod = SDL_GetModState();
        if (mod & SDL_KMOD_SHIFT) keyModifier |= Rml::Input::KM_SHIFT;
        if (mod & SDL_KMOD_CTRL)  keyModifier |= Rml::Input::KM_CTRL;
        if (mod & SDL_KMOD_ALT)   keyModifier |= Rml::Input::KM_ALT;

        switch(event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                m_rmlContext->ProcessMouseMove((int)event.motion.x, (int)event.motion.y, keyModifier);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    if (auto focus = m_rmlContext->GetFocusElement()) {
                        focus->Blur();
                    }
                }
                m_rmlContext->ProcessMouseButtonDown(event.button.button - 1, keyModifier);
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                m_rmlContext->ProcessMouseButtonUp(event.button.button - 1, keyModifier);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                m_rmlContext->ProcessMouseWheel(-event.wheel.y, keyModifier);
                break;
            case SDL_EVENT_KEY_DOWN: {
                Rml::Input::KeyIdentifier key = translateKey(event.key.scancode);
                if (key != Rml::Input::KI_UNKNOWN) {
                    m_rmlContext->ProcessKeyDown(key, keyModifier);
                }
                break;
            }
            case SDL_EVENT_KEY_UP: {
                Rml::Input::KeyIdentifier key = translateKey(event.key.scancode);
                if (key != Rml::Input::KI_UNKNOWN) {
                    m_rmlContext->ProcessKeyUp(key, keyModifier);
                }
                break;
            }
            case SDL_EVENT_TEXT_INPUT: {
                if (event.text.text) {
                    m_rmlContext->ProcessTextInput(event.text.text);
                }
                break;
            }
        }

        tail++;
        m_eventTail.store(tail, std::memory_order_release);
    }
}

#ifdef ENABLE_SDL
void VK_UIBridge::processSdlEvent(const SDL_Event& event) {
    size_t head = m_eventHead.load(std::memory_order_relaxed);
    size_t tail = m_eventTail.load(std::memory_order_acquire);

    if (head - tail >= EVENT_QUEUE_CAP) {
        return;
    }

    m_eventBuf[head % EVENT_QUEUE_CAP] = event;
    m_eventHead.store(head + 1, std::memory_order_release);
}
#endif

void VK_UIBridge::lockUI() { m_uiMutex.lock(); }
bool VK_UIBridge::tryLockUI() { return m_uiMutex.try_lock(); }
void VK_UIBridge::unlockUI() { m_uiMutex.unlock(); }

void VK_UIBridge::updateUI() {
    drainEventQueue();

    if (m_rmlContext) {
        m_rmlContext->Update();
    }
}

void VK_UIBridge::renderUI() {
    std::lock_guard<std::mutex> lock(m_uiMutex);

    if (m_renderInterface) {
        m_renderInterface->pumpDeferredDestruction();
    }

    auto tStart = std::chrono::high_resolution_clock::now();

    if (m_rmlContext) {
        if (m_renderInterface) {
            m_renderInterface->beginRender();
        }
        m_rmlContext->Render();
    }

    auto tEnd = std::chrono::high_resolution_clock::now();
    float elapsed = std::chrono::duration<float, std::milli>(tEnd - tStart).count();

    static float accum = 0;
    static int frames = 0;
    accum += elapsed;
    if (++frames >= 60) {
        g_RenderStats_UITime.store(accum / frames, std::memory_order_relaxed);
        accum = 0;
        frames = 0;
    }
}

Rml::ElementDocument* VK_UIBridge::loadDocument(const std::string& documentPath) {
    if (!m_rmlContext) return nullptr;
    auto* doc = m_rmlContext->LoadDocument(documentPath);
    if (doc) {
        doc->Show();
        NX_CORE_INFO("RmlUi Loaded document: {}", documentPath);
    } else {
        NX_CORE_ERROR("RmlUi Failed to load document: {}", documentPath);
    }
    return doc;
}

void VK_UIBridge::onResize(int width, int height) {
    if(m_rmlContext) {
        m_rmlContext->SetDimensions(Rml::Vector2i(width, height));
    }
    if (m_renderInterface) {
        m_renderInterface->updateWindowSize(width, height);
    }
}

} // namespace Nexus

#endif
