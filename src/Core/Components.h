#pragma once

#include <array>
#include <vector>
#include <string>
#include <cmath>
#include <entt/entt.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/array.hpp>

namespace Nexus {

/**
 * @brief 名称标签组件
 */
struct TagComponent {
    std::string name;

    TagComponent() = default;
    explicit TagComponent(const std::string& n) : name(n) {}

    template<class Archive>
    void serialize(Archive& ar) {
        ar(name);
    }
};

/**
 * @brief 变换组件 (TRS + 计算后的世界矩阵)
 *
 * position/rotation/scale 为局部空间，worldMatrix 由 HierarchySystem 每帧更新
 */
struct TransformComponent {
    std::array<float, 3> position  = {0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation  = {0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 3> scale     = {1.0f, 1.0f, 1.0f};
    std::array<float, 16> worldMatrix = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    /**
     * @brief 从 TRS 计算局部 4x4 矩阵 (列主序)
     */
    std::array<float, 16> computeLocalMatrix() const {
        float qx = rotation[0], qy = rotation[1], qz = rotation[2], qw = rotation[3];
        float sx = scale[0], sy = scale[1], sz = scale[2];

        float x2 = qx + qx, y2 = qy + qy, z2 = qz + qz;
        float xx = qx * x2, yy = qy * y2, zz = qz * z2;
        float xy = qx * y2, xz = qx * z2, yz = qy * z2;
        float wx = qw * x2, wy = qw * y2, wz = qw * z2;

        return {
            (1.0f - (yy + zz)) * sx, (xy + wz) * sx,          (xz - wy) * sx,          0.0f,
            (xy - wz) * sy,          (1.0f - (xx + zz)) * sy,  (yz + wx) * sy,          0.0f,
            (xz + wy) * sz,          (yz - wx) * sz,           (1.0f - (xx + yy)) * sz, 0.0f,
            position[0],             position[1],              position[2],              1.0f
        };
    }

    template<class Archive>
    void serialize(Archive& ar) {
        ar(position, rotation, scale);
    }
};

/**
 * @brief 层级关系组件
 */
struct HierarchyComponent {
    entt::entity parent = entt::null;
    std::vector<entt::entity> children;

    template<class Archive>
    void serialize(Archive& ar) {
        ar(parent, children);
    }
};

/**
 * @brief 矩阵乘法工具 (列主序 4x4)
 */
inline std::array<float, 16> multiplyMat4(const std::array<float, 16>& a, const std::array<float, 16>& b) {
    std::array<float, 16> r = {};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    return r;
}

} // namespace Nexus
