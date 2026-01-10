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

void VK_UIBridge::update() {
    if (m_rmlContext) {
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
void VK_UIBridge::processSdlEvent(const SDL_Event& event) {
    if (!m_rmlContext) return;

    switch(event.type) {
        case SDL_EVENT_MOUSE_MOTION:
            m_rmlContext->ProcessMouseMove((int)event.motion.x, (int)event.motion.y, 0);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            m_rmlContext->ProcessMouseButtonDown(event.button.button - 1, 0);
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            m_rmlContext->ProcessMouseButtonUp(event.button.button - 1, 0);
            break;
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
