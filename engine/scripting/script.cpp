#include "script.h"
#include "core/logging.h"
#include "gameplay/ecs/ecs.h"

namespace action {

vec3 Script::GetPosition() const {
    if (auto* transform = m_ecs->GetComponent<TransformComponent>(m_entity)) {
        return transform->position;
    }
    return vec3{0, 0, 0};
}

void Script::SetPosition(const vec3& pos) {
    if (auto* transform = m_ecs->GetComponent<TransformComponent>(m_entity)) {
        transform->position = pos;
    }
}

quat Script::GetRotation() const {
    if (auto* transform = m_ecs->GetComponent<TransformComponent>(m_entity)) {
        return transform->rotation;
    }
    return quat::identity();
}

void Script::SetRotation(const quat& rot) {
    if (auto* transform = m_ecs->GetComponent<TransformComponent>(m_entity)) {
        transform->rotation = rot;
    }
}

vec3 Script::GetScale() const {
    if (auto* transform = m_ecs->GetComponent<TransformComponent>(m_entity)) {
        return transform->scale;
    }
    return vec3{1, 1, 1};
}

void Script::SetScale(const vec3& scale) {
    if (auto* transform = m_ecs->GetComponent<TransformComponent>(m_entity)) {
        transform->scale = scale;
    }
}

vec3 Script::GetForward() const {
    quat rot = GetRotation();
    // Forward is -Z in our coordinate system
    return rot * vec3{0, 0, -1};
}

vec3 Script::GetRight() const {
    quat rot = GetRotation();
    return rot * vec3{1, 0, 0};
}

vec3 Script::GetUp() const {
    quat rot = GetRotation();
    return rot * vec3{0, 1, 0};
}

void Script::Translate(const vec3& delta) {
    SetPosition(GetPosition() + delta);
}

void Script::Rotate(const vec3& euler_degrees) {
    quat current = GetRotation();
    quat delta = quat::from_euler(
        euler_degrees.x * DEG_TO_RAD,
        euler_degrees.y * DEG_TO_RAD,
        euler_degrees.z * DEG_TO_RAD
    );
    SetRotation(current * delta);
}

void Script::LookAt(const vec3& target) {
    vec3 pos = GetPosition();
    vec3 direction = normalize(target - pos);
    
    // Calculate rotation to face direction
    vec3 forward = vec3{0, 0, -1};
    vec3 axis = cross(forward, direction);
    float dot_val = dot(forward, direction);
    
    if (length(axis) < 0.0001f) {
        if (dot_val > 0) {
            SetRotation(quat::identity());
        } else {
            SetRotation(quat::from_axis_angle(vec3{0, 1, 0}, PI));
        }
        return;
    }
    
    float angle = std::acos(std::clamp(dot_val, -1.0f, 1.0f));
    SetRotation(quat::from_axis_angle(normalize(axis), angle));
}

Entity Script::FindEntityByName(const std::string& name) {
    // Search through entities with TagComponent
    // This is a simple linear search - could be optimized with name->entity map
    Entity found = INVALID_ENTITY;
    m_ecs->ForEach<TagComponent>([&](Entity entity, TagComponent& tag) {
        if (tag.name == name) {
            found = entity;
        }
    });
    return found;
}

void Script::Log(const std::string& message) {
    LOG_INFO("[{}] {}", GetTypeName(), message);
}

void Script::LogWarning(const std::string& message) {
    LOG_WARN("[{}] {}", GetTypeName(), message);
}

void Script::LogError(const std::string& message) {
    LOG_ERROR("[{}] {}", GetTypeName(), message);
}

Entity Script::Instantiate(const std::string& prefab_name, const vec3& position) {
    // TODO: Integrate with PrefabManager
    LOG_WARN("Script::Instantiate not yet implemented");
    return INVALID_ENTITY;
}

void Script::Destroy(Entity entity, float delay) {
    if (delay <= 0.0f) {
        m_ecs->DestroyEntity(entity);
    } else {
        // TODO: Queue delayed destruction
        LOG_WARN("Delayed destruction not yet implemented");
        m_ecs->DestroyEntity(entity);
    }
}

void Script::DestroySelf(float delay) {
    Destroy(m_entity, delay);
}

} // namespace action
