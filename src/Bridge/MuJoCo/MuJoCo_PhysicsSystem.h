#pragma once

#include "Base.h"
#include "Interfaces.h"
#include "thirdparty.h"

namespace Nexus {

/**
 * @brief MuJoCo 物理系统实现
 */
class MuJoCo_PhysicsSystem : public IPhysicsSystem {
public:
    MuJoCo_PhysicsSystem();
    virtual ~MuJoCo_PhysicsSystem() override;

    virtual Status initialize() override;
    virtual void update(float deltaTime) override;
    virtual void shutdown() override;

    /** TODO: 添加 Body 创建与管理接口 */

private:
    static void* allocate(size_t size) { return malloc(size); }
    static void free(void* ptr) { ::free(ptr); }
    static void* reallocate(void* ptr, size_t size) { return realloc(ptr, size); }

    mjModel* m_model = nullptr;
    mjData* m_data = nullptr;
};

} // namespace Nexus
