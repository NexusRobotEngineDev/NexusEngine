#include "PybindECS.h"
#include <pybind11/stl.h>
#include "../../Core/Scene.h"
#include "../../Core/Components.h"
#include "../../Bridge/Entity.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nexus {

extern std::unique_ptr<Scene> g_scene;

void BindECS(py::module& m) {
    m.def("get_scene_entity_count", []() {
        if (!g_scene) return 0;
        int count = 0;
        auto view = g_scene->getRegistry().view<TagComponent>();
        for ([[maybe_unused]] auto ent : view) ++count;
        return count;
    });

    struct PyEntity {
        uint32_t handle;
        PyEntity(uint32_t h) : handle(h) {}

        bool is_valid() const {
            if (!g_scene) return false;
            return g_scene->getRegistry().valid(static_cast<entt::entity>(handle));
        }

        std::string get_name() const {
            if (!is_valid()) return "";
            Entity e(static_cast<entt::entity>(handle), &g_scene->getRegistry());
            if (e.hasComponent<TagComponent>()) {
                return e.getComponent<TagComponent>().name;
            }
            return "Entity_" + std::to_string(handle);
        }

        bool has_children() const {
            if (!is_valid()) return false;
            Entity e(static_cast<entt::entity>(handle), &g_scene->getRegistry());
            if (e.hasComponent<HierarchyComponent>()) {
                return !e.getComponent<HierarchyComponent>().children.empty();
            }
            return false;
        }

        std::vector<PyEntity> get_children() const {
            std::vector<PyEntity> result;
            if (!is_valid()) return result;
            Entity e(static_cast<entt::entity>(handle), &g_scene->getRegistry());
            if (e.hasComponent<HierarchyComponent>()) {
                for (auto child : e.getComponent<HierarchyComponent>().children) {
                    if (g_scene->getRegistry().valid(child)) {
                        result.push_back(PyEntity(static_cast<uint32_t>(child)));
                    }
                }
            }
            return result;
        }

        int get_parent() const {
            if (!is_valid()) return -1;
            Entity e(static_cast<entt::entity>(handle), &g_scene->getRegistry());
            if (e.hasComponent<HierarchyComponent>()) {
                auto p = e.getComponent<HierarchyComponent>().parent;
                if (g_scene->getRegistry().valid(p)) {
                    return static_cast<int>(static_cast<uint32_t>(p));
                }
            }
            return -1;
        }

        std::vector<float> get_position() const {
            if (!is_valid()) return {0,0,0};
            Entity e(static_cast<entt::entity>(handle), &g_scene->getRegistry());
            if (e.hasComponent<TransformComponent>()) {
                auto& t = e.getComponent<TransformComponent>();
                return {t.position[0], t.position[1], t.position[2]};
            }
            return {0,0,0};
        }

        void set_position(float x, float y, float z) {
            if (!is_valid()) return;
            Entity e(static_cast<entt::entity>(handle), &g_scene->getRegistry());
            if (e.hasComponent<TransformComponent>()) {
                auto& t = e.getComponent<TransformComponent>();
                t.position[0] = x;
                t.position[1] = y;
                t.position[2] = z;
            }
        }

        std::vector<float> get_rotation_euler() const {
            if (!is_valid()) return {0,0,0};
            Entity e(static_cast<entt::entity>(handle), &g_scene->getRegistry());
            if (e.hasComponent<TransformComponent>()) {
                auto& t = e.getComponent<TransformComponent>();
                glm::quat q(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]);
                glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
                return {euler.x, euler.y, euler.z};
            }
            return {0,0,0};
        }

        void set_rotation_euler(float x, float y, float z) {
            if (!is_valid()) return;
            Entity e(static_cast<entt::entity>(handle), &g_scene->getRegistry());
            if (e.hasComponent<TransformComponent>()) {
                auto& t = e.getComponent<TransformComponent>();
                glm::quat q = glm::quat(glm::radians(glm::vec3(x, y, z)));
                t.rotation[0] = q.x;
                t.rotation[1] = q.y;
                t.rotation[2] = q.z;
                t.rotation[3] = q.w;
            }
        }
    };

    py::class_<PyEntity>(m, "Entity")
        .def(py::init<uint32_t>())
        .def("is_valid", &PyEntity::is_valid)
        .def_property_readonly("id", [](const PyEntity& e) { return e.handle; })
        .def_property_readonly("name", &PyEntity::get_name)
        .def("has_children", &PyEntity::has_children)
        .def("get_children", &PyEntity::get_children)
        .def("get_parent", &PyEntity::get_parent)
        .def("get_position", &PyEntity::get_position)
        .def("set_position", &PyEntity::set_position)
        .def("get_rotation", &PyEntity::get_rotation_euler)
        .def("set_rotation", &PyEntity::set_rotation_euler);

    m.def("get_all_entities", []() {
        std::vector<PyEntity> entities;
        if (!g_scene) return entities;
        auto& reg = g_scene->getRegistry();
        auto view = reg.view<TagComponent>();
        for (auto ent : view) {
            entities.push_back(PyEntity(static_cast<uint32_t>(ent)));
        }
        return entities;
    });

    m.def("get_root_entities", []() {
        std::vector<PyEntity> roots;
        if (!g_scene) return roots;
        auto& reg = g_scene->getRegistry();
        auto view = reg.view<TagComponent>();
        for (auto ent : view) {
            Entity e(ent, &reg);
            bool isRoot = true;
            if (e.hasComponent<HierarchyComponent>()) {
                isRoot = (e.getComponent<HierarchyComponent>().parent == entt::null);
            }
            if (isRoot) roots.push_back(PyEntity(static_cast<uint32_t>(ent)));
        }
        return roots;
    });

    m.def("get_entity", [](uint32_t id) {
        return PyEntity(id);
    });
}

} // namespace Nexus
