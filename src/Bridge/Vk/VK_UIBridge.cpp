#include "VK_UIBridge.h"
#include "../Log.h"
#include "../ResourceLoader.h"
#include <RmlUi/Debugger.h>

#ifdef ENABLE_RMLUI

namespace Nexus {

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

    Rml::Debugger::Initialise(m_rmlContext);
    Rml::Debugger::SetVisible(true);

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

void VK_UIBridge::injectTextInput(const std::string& text) {
    if (text.empty()) return;
    std::lock_guard<std::mutex> lock(m_textInputMutex);
    m_textInputQueue.push_back(text);
}

void VK_UIBridge::flushThreadSafeEvents() {
    std::vector<std::string> localQueue;
    {
        std::lock_guard<std::mutex> lock(m_textInputMutex);
        if (m_textInputQueue.empty()) return;
        localQueue = std::move(m_textInputQueue);
        m_textInputQueue.clear();
    }

    if (m_rmlContext) {
        for (const auto& text : localQueue) {
            if (auto focus = m_rmlContext->GetFocusElement()) {
                NX_CORE_INFO("VK_UIBridge - Injecting TextInput: '{}'", text);
                m_rmlContext->ProcessTextInput(text);
            }
        }
    }
}

void VK_UIBridge::update() {
    if (m_rmlContext) {
        flushThreadSafeEvents();
        m_rmlContext->Update();
    }
    if (m_renderInterface) {
        m_renderInterface->pumpDeferredDestruction();
    }
}

void VK_UIBridge::render() {
    if (m_rmlContext) {
        m_rmlContext->Render();
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

#ifdef ENABLE_SDL
#include <RmlUi/Core/Input.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>

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

void VK_UIBridge::processSdlEvent(const SDL_Event& event) {
    if (!m_rmlContext) return;

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
            break;
        }
    }
}
#endif

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
