#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NxURDF {

using Vec3  = std::array<double, 3>;
using Vec4  = std::array<double, 4>;
using Mat3  = std::array<double, 6>;

struct Pose {
    Vec3 xyz{};
    Vec3 rpy{};
};

enum class GeometryType : uint8_t {
    None, Box, Cylinder, Sphere, Mesh
};

struct Geometry {
    GeometryType type = GeometryType::None;
    Vec3         size{};
    double       radius  = 0.0;
    double       length  = 0.0;
    std::string  meshFilename;
    Vec3         meshScale{1.0, 1.0, 1.0};
};

struct Material {
    std::string name;
    Vec4 color{1.0, 1.0, 1.0, 1.0};
    std::string textureFilename;
};

struct Visual {
    std::string            name;
    Pose                   origin;
    Geometry               geometry;
    std::optional<Material> material;
};

struct Collision {
    std::string name;
    Pose        origin;
    Geometry    geometry;
};

struct Inertial {
    Pose   origin;
    double mass = 0.0;
    Mat3   inertia{};
};

struct Link {
    std::string              name;
    std::optional<Inertial>  inertial;
    std::vector<Visual>      visuals;
    std::vector<Collision>   collisions;
};

enum class JointType : uint8_t {
    Fixed, Revolute, Continuous, Prismatic, Floating, Planar
};

struct JointLimits {
    double lower    = 0.0;
    double upper    = 0.0;
    double effort   = 0.0;
    double velocity = 0.0;
};

struct JointDynamics {
    double damping  = 0.0;
    double friction = 0.0;
};

struct Joint {
    std::string                  name;
    JointType                    type = JointType::Fixed;
    std::string                  parentLink;
    std::string                  childLink;
    Pose                         origin;
    Vec3                         axis{0.0, 0.0, 1.0};
    std::optional<JointLimits>   limits;
    std::optional<JointDynamics> dynamics;
};

struct Model {
    std::string          name;
    std::vector<Link>    links;
    std::vector<Joint>   joints;

    std::unordered_map<std::string, size_t> linkIndex;
    std::unordered_map<std::string, size_t> jointIndex;

    std::unordered_map<std::string, Material> materials;

    const Link*  findLink(std::string_view name) const;
    const Joint* findJoint(std::string_view name) const;
    std::string  rootLinkName() const;
};


std::optional<Model> parseFile(std::string_view filePath);

std::optional<Model> parseString(std::string_view xmlContent);

std::string resolveMeshPath(std::string_view uri, std::string_view packageBasePath);

} // namespace NxURDF
