#include "async_physics.h"
#include "core/logging.h"

namespace action {

AsyncPhysicsSystem::AsyncPhysicsSystem(ECS* ecs, PhysicsWorld* physics, JobSystem* jobs)
    : m_ecs(ecs), m_physics(physics), m_jobs(jobs)
{
    LOG_INFO("AsyncPhysicsSystem initialized");
}

void AsyncPhysicsSystem::DispatchAsync(float dt) {
    (void)dt;
    // Capture raw pointers for the lambda — safe as long as SyncResults is called
    // before the engine destroys either system.
    PhysicsWorld*                  physics  = m_physics;
    std::mutex*                    mtx      = &m_contacts_mutex;
    std::vector<CollisionEvent>*   pending  = &m_pending_contacts;

    m_job_handle = m_jobs->Submit([physics, mtx, pending]() {
        physics->UpdateSpatialHash();
        const auto& events = physics->GetCollisionEvents();

        std::lock_guard<std::mutex> lock(*mtx);
        pending->insert(pending->end(), events.begin(), events.end());
    }, JobPriority::Normal);
}

void AsyncPhysicsSystem::SyncResults() {
    m_job_handle.Wait();

    std::vector<CollisionEvent> local;
    {
        std::lock_guard<std::mutex> lock(m_contacts_mutex);
        local.swap(m_pending_contacts);
    }

    // Dispatch events — game code can listen by polling GetCollisionEvents()
    // on PhysicsWorld or by registering callbacks; results are already in
    // m_physics->m_collision_events from UpdateSpatialHash().
    if (!local.empty())
        LOG_TRACE("AsyncPhysics flushed {} collision events", local.size());
}

bool AsyncPhysicsSystem::IsAsyncPending() const {
    return !m_job_handle.IsComplete();
}

} // namespace action
