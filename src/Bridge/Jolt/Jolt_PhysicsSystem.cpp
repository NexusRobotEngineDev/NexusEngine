#include "Jolt_PhysicsSystem.h"
#include "Log.h"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include <iostream>
#include <cstdarg>

JPH_SUPPRESS_WARNINGS

namespace JPH {
#ifdef JPH_ENABLE_ASSERTS
    void TraceImpl(const char* inFMT, ...) {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);
        Nexus::Log::trace("Jolt: {}", buffer);
    }
    bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine) {
        Nexus::Log::error("Jolt Assert: {}:{}: ({}) {}", inFile, inLine, inExpression, (inMessage ? inMessage : ""));
        return true;
    }
#endif
}

namespace Nexus {

using namespace JPH;

namespace Layers {
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING = 1;
    static constexpr ObjectLayer NUM_LAYERS = 2;
};

namespace BroadPhaseLayers {
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS(2);
};

class Jolt_PhysicsSystem::MyBPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    MyBPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }
    virtual uint32_t GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }
    virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
        switch ((BroadPhaseLayer::Type)inLayer) {
        case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
        default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif
private:
    BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class Jolt_PhysicsSystem::MyObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
        case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:     return inLayer2 == BroadPhaseLayers::NON_MOVING;
        default: JPH_ASSERT(false); return false;
        }
    }
};

class Jolt_PhysicsSystem::MyObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
        switch (inObject1) {
        case Layers::NON_MOVING: return inObject2 == Layers::MOVING;
        case Layers::MOVING:     return inObject2 == Layers::NON_MOVING;
        default: JPH_ASSERT(false); return false;
        }
    }
};

class Jolt_PhysicsSystem::MyContactListenerImpl : public ContactListener {
public:
    void extractContactFromJolt(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold) {
        ContactData c;
        c.position = {inManifold.mBaseOffset.GetX(), inManifold.mBaseOffset.GetY(), inManifold.mBaseOffset.GetZ()};
        c.normal = {inManifold.mWorldSpaceNormal.GetX(), inManifold.mWorldSpaceNormal.GetY(), inManifold.mWorldSpaceNormal.GetZ()};
        c.depth = -(float)inManifold.mPenetrationDepth;

        if (inBody1.GetObjectLayer() == Layers::MOVING) {
            c.geomIdx1 = (int)inBody1.GetUserData();
            c.normal[0] = -c.normal[0];
            c.normal[1] = -c.normal[1];
            c.normal[2] = -c.normal[2];
        } else {
            c.geomIdx1 = (int)inBody2.GetUserData();
        }
        c.geomIdx2 = 0;

        std::lock_guard<std::mutex> lock(mMutex);
        mContacts.push_back(c);
    }

    virtual void OnContactAdded(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override {
        extractContactFromJolt(inBody1, inBody2, inManifold);
    }

    virtual void OnContactPersisted(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings& ioSettings) override {
        extractContactFromJolt(inBody1, inBody2, inManifold);
    }

    std::vector<ContactData> grabContacts() {
        std::lock_guard<std::mutex> lock(mMutex);
        std::vector<ContactData> res = std::move(mContacts);
        mContacts.clear();
        return res;
    }
private:
    std::mutex mMutex;
    std::vector<ContactData> mContacts;
};

Jolt_PhysicsSystem::Jolt_PhysicsSystem() {}

Jolt_PhysicsSystem::~Jolt_PhysicsSystem() {
    shutdown();
}

Status Jolt_PhysicsSystem::initialize() {
    JPH::RegisterDefaultAllocator();

#ifdef JPH_ENABLE_ASSERTS
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)
#endif

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    m_tempAllocator = new TempAllocatorImpl(10 * 1024 * 1024);
    m_jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

    const uint cMaxBodies = 10240;
    const uint cNumBodyMutexes = 0;
    const uint cMaxBodyPairs = 10240;
    const uint cMaxContactConstraints = 10240;

    m_bpLayerInterface = new MyBPLayerInterfaceImpl();
    m_objectVsBroadphaseLayerFilter = new MyObjectVsBroadPhaseLayerFilterImpl();
    m_objectVsObjectLayerFilter = new MyObjectLayerPairFilterImpl();

    m_physicsSystem = new PhysicsSystem();
    m_physicsSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
        *m_bpLayerInterface, *m_objectVsBroadphaseLayerFilter, *m_objectVsObjectLayerFilter);

    m_contactListener = new MyContactListenerImpl();
    m_physicsSystem->SetContactListener(m_contactListener);

    NX_CORE_INFO("Jolt Physics System Initialized");
    return OkStatus();
}

Status Jolt_PhysicsSystem::loadModel(const std::string& path) {
    NX_CORE_WARN("Jolt loadModel() call ignored. Jolt uses dynamic object addition instead of a static XML model.");
    return OkStatus();
}

void Jolt_PhysicsSystem::shutdown() {
    std::lock_guard<std::mutex> lock(m_physicsMutex);

    if (m_physicsSystem) {
        delete m_physicsSystem;
        m_physicsSystem = nullptr;
    }

    delete m_bpLayerInterface; m_bpLayerInterface = nullptr;
    delete m_objectVsBroadphaseLayerFilter; m_objectVsBroadphaseLayerFilter = nullptr;
    delete m_objectVsObjectLayerFilter; m_objectVsObjectLayerFilter = nullptr;
    delete m_contactListener; m_contactListener = nullptr;

    if (m_jobSystem) {
        delete m_jobSystem;
        m_jobSystem = nullptr;
    }

    if (m_tempAllocator) {
        delete m_tempAllocator;
        m_tempAllocator = nullptr;
    }

    JPH::UnregisterTypes();
    if (JPH::Factory::sInstance) {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    NX_CORE_INFO("Jolt Physics System Shutdown");
}

void Jolt_PhysicsSystem::update(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_physicsMutex);
    if (!m_physicsSystem) return;

    m_timeStepAccumulator += deltaTime;
    const float cStepFreq = 1.0f / 60.0f;
    int collisionSteps = 1;

    while (m_timeStepAccumulator >= cStepFreq) {
        m_physicsSystem->Update(cStepFreq, collisionSteps, m_tempAllocator, m_jobSystem);
        m_timeStepAccumulator -= cStepFreq;
    }
}

void Jolt_PhysicsSystem::setJointControl(const std::string& jointName, float q, float dq, float kp, float kd, float tau) {
}

bool Jolt_PhysicsSystem::getBodyTransform(const std::string& name, std::array<float, 3>& outPos, std::array<float, 4>& outRot) {
    return false;
}

std::vector<std::string> Jolt_PhysicsSystem::getActuatorNames() const {
    return {};
}

std::vector<IPhysicsSystem::ContactData> Jolt_PhysicsSystem::extractContacts() {
    if (m_contactListener) {
        return m_contactListener->grabContacts();
    }
    return {};
}

void Jolt_PhysicsSystem::syncKinematicProxies(const std::vector<GeomSyncData>& geoms) {
    std::lock_guard<std::mutex> lock(m_physicsMutex);
    if (!m_physicsSystem) return;

    static bool firstSync = true;
    static bool firstSyncLog = true;
    if (firstSync) {
        NX_CORE_INFO("[Jolt] Initializing sync with {} kinematic proxies", geoms.size());
        firstSync = false;
    }

    for (const auto& g : geoms) {
        auto it = m_geomIdToBodyId.find(g.geomId);

        RVec3 pos(g.pos[0], g.pos[1], g.pos[2]);
        Quat rot(g.rot[0], g.rot[1], g.rot[2], g.rot[3]);

        if (it != m_geomIdToBodyId.end()) {
            BodyID bid(it->second);
            m_physicsSystem->GetBodyInterface().SetPositionAndRotation(bid, pos, rot, EActivation::Activate);
        } else {

            RefConst<Shape> shape;
            try {
                if (g.type == 2) {
                    shape = new SphereShape(std::max(0.001f, g.size[0]));
                } else if (g.type == 3) {
                    shape = new CapsuleShape(std::max(0.001f, g.size[1]), std::max(0.001f, g.size[0]));
                    shape = new RotatedTranslatedShape(Vec3::sZero(), Quat::sRotation(Vec3::sAxisX(), 1.57079632679f), shape);
                } else if (g.type == 5) {
                    float halfHeight = std::max(0.001f, g.size[1]);
                    float radius = std::max(0.001f, g.size[0]);
                    float convexRadius = std::min(0.05f, std::min(halfHeight, radius) * 0.9f);
                    shape = new CylinderShape(halfHeight, radius, convexRadius);
                    shape = new RotatedTranslatedShape(Vec3::sZero(), Quat::sRotation(Vec3::sAxisX(), 1.57079632679f), shape);
                } else if (g.type == 6) {
                    float hx = std::max(0.001f, g.size[0]);
                    float hy = std::max(0.001f, g.size[1]);
                    float hz = std::max(0.001f, g.size[2]);
                    float convexRadius = std::min({0.05f, hx * 0.9f, hy * 0.9f, hz * 0.9f});
                    shape = new BoxShape(Vec3(hx, hy, hz), convexRadius);
                } else {
                    continue;
                }
            } catch (...) {
                NX_CORE_ERROR("Exception creating Jolt shape for geom {}, type {}, sizes: {}, {}, {}", g.geomId, g.type, g.size[0], g.size[1], g.size[2]);
                continue;
            }

            if (!shape) {
                NX_CORE_ERROR("Failed to create shape for geom {}, type {}", g.geomId, g.type);
                continue;
            }

            BodyCreationSettings settings(shape, pos, rot, EMotionType::Kinematic, Layers::MOVING);
            settings.mUserData = g.geomId;

            Body* body = m_physicsSystem->GetBodyInterface().CreateBody(settings);
            if (body) {
                m_physicsSystem->GetBodyInterface().AddBody(body->GetID(), EActivation::Activate);
                m_geomIdToBodyId[g.geomId] = body->GetID().GetIndexAndSequenceNumber();
            }
        }
    }

    if (firstSyncLog) {
        NX_CORE_INFO("[Jolt] Finished syncKinematicProxies execution");
        firstSyncLog = false;
    }
}

Jolt_PhysicsSystem::RaycastResult Jolt_PhysicsSystem::raycast(const std::array<float, 3>& origin, const std::array<float, 3>& direction) {
    RaycastResult result;
    std::lock_guard<std::mutex> lock(m_physicsMutex);
    if (!m_physicsSystem) return result;

    RVec3 org(origin[0], origin[1], origin[2]);
    Vec3 dir(direction[0], direction[1], direction[2]);
    RRayCast ray{org, dir};

    RayCastResult hit;
    if (m_physicsSystem->GetNarrowPhaseQuery().CastRay(ray, hit)) {
        result.hit = true;
        result.distance = hit.mFraction * dir.Length();

        RVec3 hitPos = ray.GetPointOnRay(hit.mFraction);
        result.position = {(float)hitPos.GetX(), (float)hitPos.GetY(), (float)hitPos.GetZ()};

        BodyLockRead bodyLock(m_physicsSystem->GetBodyLockInterface(), hit.mBodyID);
        if (bodyLock.Succeeded()) {
            Vec3 normal = bodyLock.GetBody().GetShape()->GetSurfaceNormal(hit.mSubShapeID2, hitPos);
            result.normal = {normal.GetX(), normal.GetY(), normal.GetZ()};
        }
    }
    return result;
}



uint32_t Jolt_PhysicsSystem::addStaticTriangleMesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, const std::array<float, 16>& transform) {
    NX_CORE_INFO("[Jolt] addStaticTriangleMesh Start: verts={}, indices={}", vertices.size()/3, indices.size()/3);
    std::lock_guard<std::mutex> lock(m_physicsMutex);
    if (!m_physicsSystem || vertices.empty() || indices.empty()) return (uint32_t)-1;

    NX_CORE_INFO("[Jolt] Building VertexList and TriangleList");
    VertexList vertexList;
    vertexList.reserve(vertices.size() / 3);
    for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
        vertexList.push_back(Float3(vertices[i], vertices[i+1], vertices[i+2]));
    }

    IndexedTriangleList triangleList;
    triangleList.reserve(indices.size() / 3);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        triangleList.push_back(IndexedTriangle(i0, i1, i2));
    }

    NX_CORE_INFO("[Jolt] Creating MeshShapeSettings");
    Ref<MeshShapeSettings> meshSettings = new MeshShapeSettings(vertexList, triangleList);
    ShapeSettings::ShapeResult shapeResult = meshSettings->Create();
    if (shapeResult.HasError() || !shapeResult.IsValid()) {
        NX_CORE_ERROR("Failed to create MeshShape in Jolt: {}", shapeResult.GetError().c_str());
        return (uint32_t)-1;
    }
    RefConst<Shape> meshShape = shapeResult.Get();
    NX_CORE_INFO("[Jolt] MeshShape created successfully");

    Mat44 matTransform(
        Vec4(transform[0], transform[1], transform[2], transform[3]),
        Vec4(transform[4], transform[5], transform[6], transform[7]),
        Vec4(transform[8], transform[9], transform[10], transform[11]),
        Vec4(transform[12], transform[13], transform[14], transform[15])
    );

    RVec3 position = matTransform.GetTranslation();
    Quat rotation = matTransform.GetRotation().GetQuaternion();

    BodyCreationSettings settings(meshShape, position, rotation, EMotionType::Static, Layers::NON_MOVING);
    constexpr float cFriction = 0.8f;
    constexpr float cRestitution = 0.1f;
    settings.mFriction = cFriction;
    settings.mRestitution = cRestitution;

    NX_CORE_INFO("[Jolt] Creating Body...");
    Body* body = m_physicsSystem->GetBodyInterface().CreateBody(settings);
    if (body) {
        NX_CORE_INFO("[Jolt] Adding Body...");
        m_physicsSystem->GetBodyInterface().AddBody(body->GetID(), EActivation::DontActivate);
        NX_CORE_INFO("[Jolt] Body Added successfully");
        return body->GetID().GetIndexAndSequenceNumber();
    }
    NX_CORE_ERROR("[Jolt] Failed to create body!");
    return (uint32_t)-1;
}

void Jolt_PhysicsSystem::removeBody(uint32_t bodyId) {
    std::lock_guard<std::mutex> lock(m_physicsMutex);
    if (!m_physicsSystem || bodyId == (uint32_t)-1) return;

    BodyID id(bodyId);
    if (!id.IsInvalid()) {
        m_physicsSystem->GetBodyInterface().RemoveBody(id);
        m_physicsSystem->GetBodyInterface().DestroyBody(id);
    }
}

} // namespace Nexus
