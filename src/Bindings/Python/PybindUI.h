#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <RmlUi/Core.h>

namespace py = pybind11;

namespace Nexus {

class PybindUIEventListener : public Rml::EventListener {
public:
    using CallbackFunc = std::function<void(const std::unordered_map<std::string, std::string>&)>;

    PybindUIEventListener(const std::string& eventType, CallbackFunc cb) 
        : m_eventType(eventType), m_callback(cb) {}

    void ProcessEvent(Rml::Event& event) override;

    void OnDetach(Rml::Element* element) override;

private:
    std::string m_eventType;
    CallbackFunc m_callback;
};

class PyUIWrapper {
public:
    PyUIWrapper() = default;

    bool floatPanel(const std::string& panelId, float x, float y);
    bool dockPanel(const std::string& panelId, const std::string& dockZoneId);
    bool loadLayout(const std::string& filePath);
    bool saveLayout(const std::string& filePath);

    bool setElementRML(const std::string& elementId, const std::string& rml);
    std::string getElementRML(const std::string& elementId);

    bool setElementAttribute(const std::string& elementId, const std::string& attr, const std::string& val);
    std::string getElementAttribute(const std::string& elementId, const std::string& attr);

    bool setElementProperty(const std::string& elementId, const std::string& prop, const std::string& val);
    std::string getElementProperty(const std::string& elementId, const std::string& prop);

    bool addElementClass(const std::string& elementId, const std::string& className);
    bool removeElementClass(const std::string& elementId, const std::string& className);

    bool registerEventCallback(const std::string& elementId, const std::string& eventType, PybindUIEventListener::CallbackFunc cb);

    static std::vector<std::unique_ptr<PybindUIEventListener>>& getGlobalListeners();
};

void BindUI(py::module& m);

} // namespace Nexus
