#pragma once
#include "core/types.h"
#include "gameplay/ecs/ecs.h"
#include "physics/physics_world.h"
#include "core/jobs/job_system.h"
#include <mutex>
#include <vector>

namespace action {

// Runs PhysicsWorld broad-phase collision on a worker thread.
// Usage:
//   1. Call DispatchAsync(dt) once per frame before other work.
//   2. Do CPU work that is independent of collision results.
//   3. Call SyncResults() to collect contacts from the worker and fire events.
class AsyncPhysicsSystem : public System {
public:
    AsyncPhysicsSystem(ECS* ecs, PhysicsWorld* physics, JobSystem* jobs);

    // No automatic ECS-driven update — call DispatchAsync / SyncResults manually.
    void Update(float /*dt*/) override {}

    void DispatchAsync(float dt);
    void SyncResults();

    bool IsAsyncPending() const;

private:
    ECS*          m_ecs     = nullptr;
    PhysicsWorld* m_physics = nullptr;
    JobSystem*    m_jobs    = nullptr;

    JobHandle              m_job_handle;
    std::mutex             m_contacts_mutex;
    std::vector<CollisionEvent> m_pending_contacts;
};

} // namespace action
