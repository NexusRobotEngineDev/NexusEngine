
#include "URDFLoader.h"
#include <tinyxml2.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <set>
#include "../Bridge/Log.h"

namespace NxURDF {


namespace detail {

static double parseDouble(const char* s, double fallback = 0.0) {
    if (!s) return fallback;
    char* end = nullptr;
    double v = std::strtod(s, &end);
    return (end != s) ? v : fallback;
}

static Vec3 parseVec3(const char* s) {
    Vec3 v{};
    if (!s) return v;
    std::istringstream ss(s);
    ss >> v[0] >> v[1] >> v[2];
    return v;
}

static Vec4 parseVec4(const char* s) {
    Vec4 v{};
    if (!s) return v;
    std::istringstream ss(s);
    ss >> v[0] >> v[1] >> v[2] >> v[3];
    return v;
}

static Pose parsePose(const tinyxml2::XMLElement* elem) {
    Pose p;
    if (!elem) return p;
    p.xyz = parseVec3(elem->Attribute("xyz"));
    p.rpy = parseVec3(elem->Attribute("rpy"));
    return p;
}

static Geometry parseGeometry(const tinyxml2::XMLElement* elem) {
    Geometry g;
    if (!elem) return g;

    if (auto* box = elem->FirstChildElement("box")) {
        g.type = GeometryType::Box;
        g.size = parseVec3(box->Attribute("size"));
    } else if (auto* cyl = elem->FirstChildElement("cylinder")) {
        g.type = GeometryType::Cylinder;
        g.radius = parseDouble(cyl->Attribute("radius"));
        g.length = parseDouble(cyl->Attribute("length"));
    } else if (auto* sph = elem->FirstChildElement("sphere")) {
        g.type = GeometryType::Sphere;
        g.radius = parseDouble(sph->Attribute("radius"));
    } else if (auto* mesh = elem->FirstChildElement("mesh")) {
        g.type = GeometryType::Mesh;
        if (auto* fn = mesh->Attribute("filename"))
            g.meshFilename = fn;
        if (auto* sc = mesh->Attribute("scale"))
            g.meshScale = parseVec3(sc);
    }
    return g;
}

static Material parseMaterial(const tinyxml2::XMLElement* elem) {
    Material m;
    if (!elem) return m;
    if (auto* n = elem->Attribute("name"))
        m.name = n;
    if (auto* color = elem->FirstChildElement("color"))
        m.color = parseVec4(color->Attribute("rgba"));
    if (auto* tex = elem->FirstChildElement("texture")) {
        if (auto* fn = tex->Attribute("filename"))
            m.textureFilename = fn;
    }
    return m;
}

static Inertial parseInertial(const tinyxml2::XMLElement* elem) {
    Inertial in;
    if (!elem) return in;
    in.origin = parsePose(elem->FirstChildElement("origin"));
    if (auto* mass = elem->FirstChildElement("mass"))
        in.mass = parseDouble(mass->Attribute("value"));
    if (auto* inertia = elem->FirstChildElement("inertia")) {
        in.inertia[0] = parseDouble(inertia->Attribute("ixx"));
        in.inertia[1] = parseDouble(inertia->Attribute("ixy"));
        in.inertia[2] = parseDouble(inertia->Attribute("ixz"));
        in.inertia[3] = parseDouble(inertia->Attribute("iyy"));
        in.inertia[4] = parseDouble(inertia->Attribute("iyz"));
        in.inertia[5] = parseDouble(inertia->Attribute("izz"));
    }
    return in;
}

static Visual parseVisual(const tinyxml2::XMLElement* elem) {
    Visual v;
    if (!elem) return v;
    if (auto* n = elem->Attribute("name")) v.name = n;
    v.origin   = parsePose(elem->FirstChildElement("origin"));
    v.geometry = parseGeometry(elem->FirstChildElement("geometry"));
    if (auto* mat = elem->FirstChildElement("material"))
        v.material = parseMaterial(mat);

    if (v.geometry.type == GeometryType::Mesh) {
        NX_CORE_INFO("NxURDF Debug: Parsed visual mesh: {}", v.geometry.meshFilename);
    }
    return v;
}

static Collision parseCollision(const tinyxml2::XMLElement* elem) {
    Collision c;
    if (!elem) return c;
    if (auto* n = elem->Attribute("name")) c.name = n;
    c.origin   = parsePose(elem->FirstChildElement("origin"));
    c.geometry = parseGeometry(elem->FirstChildElement("geometry"));
    return c;
}

static Link parseLink(const tinyxml2::XMLElement* elem) {
    Link link;
    if (auto* n = elem->Attribute("name")) link.name = n;

    if (auto* inertial = elem->FirstChildElement("inertial"))
        link.inertial = parseInertial(inertial);

    for (auto* vis = elem->FirstChildElement("visual"); vis; vis = vis->NextSiblingElement("visual"))
        link.visuals.push_back(parseVisual(vis));

    for (auto* col = elem->FirstChildElement("collision"); col; col = col->NextSiblingElement("collision"))
        link.collisions.push_back(parseCollision(col));

    return link;
}

static JointType parseJointType(const char* s) {
    if (!s) return JointType::Fixed;
    std::string_view sv(s);
    if (sv == "revolute")   return JointType::Revolute;
    if (sv == "continuous")  return JointType::Continuous;
    if (sv == "prismatic")   return JointType::Prismatic;
    if (sv == "floating")    return JointType::Floating;
    if (sv == "planar")      return JointType::Planar;
    return JointType::Fixed;
}

static Joint parseJoint(const tinyxml2::XMLElement* elem) {
    Joint j;
    if (auto* n = elem->Attribute("name")) j.name = n;
    j.type = parseJointType(elem->Attribute("type"));
    j.origin = parsePose(elem->FirstChildElement("origin"));

    if (auto* parent = elem->FirstChildElement("parent")) {
        if (auto* l = parent->Attribute("link")) j.parentLink = l;
    }
    if (auto* child = elem->FirstChildElement("child")) {
        if (auto* l = child->Attribute("link")) j.childLink = l;
    }
    if (auto* axis = elem->FirstChildElement("axis"))
        j.axis = parseVec3(axis->Attribute("xyz"));

    if (auto* limit = elem->FirstChildElement("limit")) {
        JointLimits lim;
        lim.lower    = parseDouble(limit->Attribute("lower"));
        lim.upper    = parseDouble(limit->Attribute("upper"));
        lim.effort   = parseDouble(limit->Attribute("effort"));
        lim.velocity = parseDouble(limit->Attribute("velocity"));
        j.limits = lim;
    }
    if (auto* dyn = elem->FirstChildElement("dynamics")) {
        JointDynamics d;
        d.damping  = parseDouble(dyn->Attribute("damping"));
        d.friction = parseDouble(dyn->Attribute("friction"));
        j.dynamics = d;
    }
    return j;
}

static std::optional<Model> parseDocument(tinyxml2::XMLDocument& doc) {
    auto* robot = doc.FirstChildElement("robot");
    if (!robot) return std::nullopt;

    Model model;
    if (auto* n = robot->Attribute("name")) model.name = n;

    for (auto* elem = robot->FirstChildElement("link"); elem; elem = elem->NextSiblingElement("link")) {
        model.linkIndex[std::string(elem->Attribute("name"))] = model.links.size();
        model.links.push_back(parseLink(elem));
    }

    for (auto* elem = robot->FirstChildElement("joint"); elem; elem = elem->NextSiblingElement("joint")) {
        model.jointIndex[std::string(elem->Attribute("name"))] = model.joints.size();
        model.joints.push_back(parseJoint(elem));
    }

    return model;
}

} // namespace detail


const Link* Model::findLink(std::string_view name) const {
    auto it = linkIndex.find(std::string(name));
    return (it != linkIndex.end()) ? &links[it->second] : nullptr;
}

const Joint* Model::findJoint(std::string_view name) const {
    auto it = jointIndex.find(std::string(name));
    return (it != jointIndex.end()) ? &joints[it->second] : nullptr;
}

std::string Model::rootLinkName() const {
    std::set<std::string> childLinks;
    for (const auto& j : joints)
        childLinks.insert(j.childLink);
    for (const auto& l : links) {
        if (childLinks.find(l.name) == childLinks.end())
            return l.name;
    }
    return links.empty() ? "" : links[0].name;
}


std::optional<Model> parseFile(std::string_view filePath) {
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(std::string(filePath).c_str()) != tinyxml2::XML_SUCCESS)
        return std::nullopt;
    return detail::parseDocument(doc);
}

std::optional<Model> parseString(std::string_view xmlContent) {
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xmlContent.data(), xmlContent.size()) != tinyxml2::XML_SUCCESS)
        return std::nullopt;
    return detail::parseDocument(doc);
}

std::string resolveMeshPath(std::string_view uri, std::string_view packageBasePath) {
    constexpr std::string_view prefix = "package://";
    if (uri.substr(0, prefix.size()) == prefix) {
        auto rest = uri.substr(prefix.size());
        auto slash = rest.find('/');
        if (slash != std::string_view::npos) {
            rest = rest.substr(slash);
        }
        return std::string(packageBasePath) + std::string(rest);
    }
    return std::string(uri);
}

} // namespace NxURDF
