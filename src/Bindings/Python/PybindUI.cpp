#include "PybindUI.h"
#include <pybind11/stl.h>
#include "../../Editor/EditorUIManager.h"
#include "../../Bridge/Log.h"

namespace Nexus {

void PybindUIEventListener::ProcessEvent(Rml::Event& event) {
    if (m_callback) {
        std::unordered_map<std::string, std::string> params;
        params["id"] = event.GetTargetElement() ? event.GetTargetElement()->GetId() : "";
        params["type"] = event.GetType();

        if (event.GetType() == "change") {
            params["value"] = event.GetParameter<Rml::String>("value", "");
        } else if (event.GetType() == "click") {
            params["mouse_x"] = std::to_string(event.GetParameter<float>("mouse_x", 0.0f));
            params["mouse_y"] = std::to_string(event.GetParameter<float>("mouse_y", 0.0f));
        }

        try {
            m_callback(params);
        } catch (const pybind11::error_already_set& e) {
            NX_CORE_ERROR("Python UI Event Callback Error: {}", e.what());
        } catch (const std::exception& e) {
            NX_CORE_ERROR("Python UI Event Callback Exception: {}", e.what());
        }
    }
}

void PybindUIEventListener::OnDetach(Rml::Element* element) {

}

bool PyUIWrapper::floatPanel(const std::string& panelId, float x, float y) {
    auto ui = EditorUIManager::Get();
    if (!ui) return false;
    ui->floatPanel(panelId, x, y);
    return true;
}

bool PyUIWrapper::dockPanel(const std::string& panelId, const std::string& dockZoneId) {
    auto ui = EditorUIManager::Get();
    if (!ui) return false;
    ui->dockPanel(panelId, dockZoneId);
    return true;
}

bool PyUIWrapper::loadLayout(const std::string& filePath) {
    auto ui = EditorUIManager::Get();
    if (!ui) return false;
    return ui->loadLayout(filePath);
}

bool PyUIWrapper::saveLayout(const std::string& filePath) {
    auto ui = EditorUIManager::Get();
    if (!ui) return false;
    return ui->saveLayout(filePath);
}

bool PyUIWrapper::setElementRML(const std::string& elementId, const std::string& rml) {
    auto ui = EditorUIManager::Get();
    if (!ui || !ui->getEditorDoc()) return false;
    auto el = ui->getEditorDoc()->GetElementById(elementId);
    if (!el) return false;
    el->SetInnerRML(rml);
    return true;
}

std::string PyUIWrapper::getElementRML(const std::string& elementId) {
    auto ui = EditorUIManager::Get();
    if (!ui || !ui->getEditorDoc()) return "";
    auto el = ui->getEditorDoc()->GetElementById(elementId);
    if (!el) return "";
    return el->GetInnerRML();
}

bool PyUIWrapper::setElementAttribute(const std::string& elementId, const std::string& attr, const std::string& val) {
    auto ui = EditorUIManager::Get();
    if (!ui || !ui->getEditorDoc()) return false;
    auto el = ui->getEditorDoc()->GetElementById(elementId);
    if (!el) return false;
    
    if (attr == "value" && el->IsClassSet("focused")) {
        return true;
    }
    
    el->SetAttribute(attr, val);
    return true;
}

std::string PyUIWrapper::getElementAttribute(const std::string& elementId, const std::string& attr) {
    auto ui = EditorUIManager::Get();
    if (!ui || !ui->getEditorDoc()) return "";
    auto el = ui->getEditorDoc()->GetElementById(elementId);
    if (!el) return "";
    auto a = el->GetAttribute(attr);
    if (!a) return "";
    return a->Get<Rml::String>();
}

bool PyUIWrapper::setElementProperty(const std::string& elementId, const std::string& prop, const std::string& val) {
    auto ui = EditorUIManager::Get();
    if (!ui || !ui->getEditorDoc()) return false;
    auto el = ui->getEditorDoc()->GetElementById(elementId);
    if (!el) return false;
    return el->SetProperty(prop, val);
}

std::string PyUIWrapper::getElementProperty(const std::string& elementId, const std::string& prop) {
    auto ui = EditorUIManager::Get();
    if (!ui || !ui->getEditorDoc()) return "";
    auto el = ui->getEditorDoc()->GetElementById(elementId);
    if (!el) return "";
    const Rml::Property* p = el->GetProperty(prop);
    if (!p) return "";
    return p->ToString();
}

bool PyUIWrapper::addElementClass(const std::string& elementId, const std::string& className) {
    auto ui = EditorUIManager::Get();
    if (!ui || !ui->getEditorDoc()) return false;
    auto el = ui->getEditorDoc()->GetElementById(elementId);
    if (!el) return false;
    el->SetClass(className, true);
    return true;
}

bool PyUIWrapper::removeElementClass(const std::string& elementId, const std::string& className) {
    auto ui = EditorUIManager::Get();
    if (!ui || !ui->getEditorDoc()) return false;
    auto el = ui->getEditorDoc()->GetElementById(elementId);
    if (!el) return false;
    el->SetClass(className, false);
    return true;
}

std::vector<std::unique_ptr<PybindUIEventListener>>& PyUIWrapper::getGlobalListeners() {
    static std::vector<std::unique_ptr<PybindUIEventListener>> s_listeners;
    return s_listeners;
}

bool PyUIWrapper::registerEventCallback(const std::string& elementId, const std::string& eventType, PybindUIEventListener::CallbackFunc cb) {
    auto ui = EditorUIManager::Get();
    if (!ui || !ui->getEditorDoc()) return false;
    auto el = ui->getEditorDoc()->GetElementById(elementId);
    if (!el) return false;
    
    Rml::EventId evId = Rml::EventId::Invalid;
    if (eventType == "click") evId = Rml::EventId::Click;
    else if (eventType == "change") evId = Rml::EventId::Change;
    else if (eventType == "focus") evId = Rml::EventId::Focus;
    else if (eventType == "blur") evId = Rml::EventId::Blur;
    else if (eventType == "keydown") evId = Rml::EventId::Keydown;
    
    if (evId == Rml::EventId::Invalid) {
        NX_CORE_WARN("PyUIWrapper: unsupported event type '{}'", eventType);
        return false;
    }

    auto listener = std::make_unique<PybindUIEventListener>(eventType, cb);
    el->AddEventListener(evId, listener.get());
    getGlobalListeners().push_back(std::move(listener));
    
    return true;
}

void BindUI(py::module& m) {
    py::class_<PyUIWrapper>(m, "UIWrapper")
        .def(py::init<>())
        .def("float_panel", &PyUIWrapper::floatPanel)
        .def("dock_panel", &PyUIWrapper::dockPanel)
        .def("load_layout", &PyUIWrapper::loadLayout)
        .def("save_layout", &PyUIWrapper::saveLayout)
        .def("set_element_rml", &PyUIWrapper::setElementRML)
        .def("get_element_rml", &PyUIWrapper::getElementRML)
        .def("set_element_attribute", &PyUIWrapper::setElementAttribute)
        .def("get_element_attribute", &PyUIWrapper::getElementAttribute)
        .def("set_element_property", &PyUIWrapper::setElementProperty)
        .def("get_element_property", &PyUIWrapper::getElementProperty)
        .def("add_element_class", &PyUIWrapper::addElementClass)
        .def("remove_element_class", &PyUIWrapper::removeElementClass)
        .def("register_event_callback", &PyUIWrapper::registerEventCallback);
}

} // namespace Nexus
