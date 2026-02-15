#include "node_3d.h"
#include "core/logging.h"
#include <algorithm>
#include <cmath>

namespace action {

// ===== Node3D =====

Node3D::Node3D() : Node("Node3D") {}

Node3D::Node3D(const std::string& name) : Node(name) {}

void Node3D::SetPosition(const vec3& pos) {
    m_position = pos;
    MarkTransformDirty();
}

void Node3D::SetRotation(const vec3& euler) {
    m_rotation = euler;
    MarkTransformDirty();
}

vec3 Node3D::GetRotationRadians() const {
    constexpr float DEG_TO_RAD = 3.14159265359f / 180.0f;
    return vec3{
        m_rotation.x * DEG_TO_RAD,
        m_rotation.y * DEG_TO_RAD,
        m_rotation.z * DEG_TO_RAD
    };
}

void Node3D::SetRotationRadians(const vec3& radians) {
    constexpr float RAD_TO_DEG = 180.0f / 3.14159265359f;
    m_rotation = vec3{
        radians.x * RAD_TO_DEG,
        radians.y * RAD_TO_DEG,
        radians.z * RAD_TO_DEG
    };
    MarkTransformDirty();
}

void Node3D::SetScale(const vec3& scale) {
    m_scale = scale;
    MarkTransformDirty();
}

vec3 Node3D::GetGlobalPosition() const {
    UpdateGlobalTransform();
    return vec3{m_global_transform.columns[3].x, m_global_transform.columns[3].y, m_global_transform.columns[3].z};
}

void Node3D::SetGlobalPosition(const vec3& pos) {
    Node3D* parent = dynamic_cast<Node3D*>(GetParent());
    if (parent) {
        // Transform to local space
        vec3 local_pos = parent->ToLocal(pos);
        SetPosition(local_pos);
    } else {
        SetPosition(pos);
    }
}

vec3 Node3D::GetGlobalRotation() const {
    // Extract YXZ Euler angles from the global transform matrix.
    // Euler angles are NOT additive (adding parent angles is mathematically wrong
    // when parent axes are rotated), so we decompose from the composed matrix.
    UpdateGlobalTransform();

    // In this engine's storage convention, columns[i].xyz stores row i of the
    // TRS matrix. Each row is the corresponding rotation row scaled by scale:
    //   columns[i].xyz = scale_i * R_row_i
    // Scale is the length of each stored column's xyz.
    float sx = std::sqrt(
        m_global_transform.columns[0].x * m_global_transform.columns[0].x +
        m_global_transform.columns[0].y * m_global_transform.columns[0].y +
        m_global_transform.columns[0].z * m_global_transform.columns[0].z);
    float sy = std::sqrt(
        m_global_transform.columns[1].x * m_global_transform.columns[1].x +
        m_global_transform.columns[1].y * m_global_transform.columns[1].y +
        m_global_transform.columns[1].z * m_global_transform.columns[1].z);
    float sz = std::sqrt(
        m_global_transform.columns[2].x * m_global_transform.columns[2].x +
        m_global_transform.columns[2].y * m_global_transform.columns[2].y +
        m_global_transform.columns[2].z * m_global_transform.columns[2].z);

    constexpr float EPSILON = 0.0001f;
    constexpr float RAD_TO_DEG = 180.0f / 3.14159265359f;

    // De-scale to get pure rotation matrix rows
    // R[0][2] = columns[0].z / sx,  R[1][2] = columns[1].z / sy,  R[2][2] = columns[2].z / sz
    // R[0][0] = columns[0].x / sx,  R[0][1] = columns[0].y / sx
    float r02 = (sx > EPSILON) ? m_global_transform.columns[0].z / sx : 0.0f;
    float r12 = (sy > EPSILON) ? m_global_transform.columns[1].z / sy : 0.0f;
    float r22 = (sz > EPSILON) ? m_global_transform.columns[2].z / sz : 0.0f;
    float r00 = (sx > EPSILON) ? m_global_transform.columns[0].x / sx : 1.0f;
    float r01 = (sx > EPSILON) ? m_global_transform.columns[0].y / sx : 0.0f;

    // Extract YXZ Euler angles (same formulas as SetLocalTransform)
    vec3 euler;
    euler.x = std::atan2(-r12, r22) * RAD_TO_DEG;
    euler.y = std::asin(std::clamp(r02, -1.0f, 1.0f)) * RAD_TO_DEG;
    euler.z = std::atan2(-r01, r00) * RAD_TO_DEG;

    return euler;
}

void Node3D::SetGlobalRotation(const vec3& euler) {
    Node3D* parent = dynamic_cast<Node3D*>(GetParent());
    if (!parent) {
        SetRotation(euler);
        return;
    }

    // Build desired global transform with the target rotation, current global
    // position, and current local scale. Then compute local = parent_inv * desired
    // and decompose the local transform to extract position/rotation/scale.
    constexpr float D2R = 3.14159265359f / 180.0f;
    vec3 rad{euler.x * D2R, euler.y * D2R, euler.z * D2R};
    float cx = std::cos(rad.x), sx = std::sin(rad.x);
    float cy = std::cos(rad.y), sy = std::sin(rad.y);
    float cz = std::cos(rad.z), sz = std::sin(rad.z);

    vec3 gpos = GetGlobalPosition();
    float s_x = m_scale.x, s_y = m_scale.y, s_z = m_scale.z;

    // Build desired global TRS (rows stored in columns, matching engine convention)
    mat4 desired = mat4::Identity();
    desired.columns[0] = vec4{(cy*cz+sy*sx*sz)*s_x, (cz*sy*sx-cy*sz)*s_y, cx*sy*s_z, 0};
    desired.columns[1] = vec4{cx*sz*s_x, cx*cz*s_y, -sx*s_z, 0};
    desired.columns[2] = vec4{(cy*sx*sz-cz*sy)*s_x, (sy*sz+cy*cz*sx)*s_y, cy*cx*s_z, 0};
    desired.columns[3] = vec4{gpos.x, gpos.y, gpos.z, 1.0f};

    mat4 parent_inv = parent->GetGlobalTransform().Inverse();
    mat4 local = parent_inv * desired;
    SetLocalTransform(local);
}

vec3 Node3D::GetGlobalScale() const {
    // Extract scale from the global transform matrix.
    // In this engine's storage convention, columns[i].xyz = scale_i * R_row_i.
    // Since rotation rows have unit length, the scale is the length of each column's xyz.
    UpdateGlobalTransform();

    float sx = std::sqrt(
        m_global_transform.columns[0].x * m_global_transform.columns[0].x +
        m_global_transform.columns[0].y * m_global_transform.columns[0].y +
        m_global_transform.columns[0].z * m_global_transform.columns[0].z);
    float sy = std::sqrt(
        m_global_transform.columns[1].x * m_global_transform.columns[1].x +
        m_global_transform.columns[1].y * m_global_transform.columns[1].y +
        m_global_transform.columns[1].z * m_global_transform.columns[1].z);
    float sz = std::sqrt(
        m_global_transform.columns[2].x * m_global_transform.columns[2].x +
        m_global_transform.columns[2].y * m_global_transform.columns[2].y +
        m_global_transform.columns[2].z * m_global_transform.columns[2].z);

    return vec3{sx, sy, sz};
}

mat4 Node3D::GetLocalTransform() const {
    // Build TRS matrix
    mat4 result = mat4::Identity();
    
    // Scale
    mat4 scale_mat = mat4::Identity();
    scale_mat.columns[0].x = m_scale.x;
    scale_mat.columns[1].y = m_scale.y;
    scale_mat.columns[2].z = m_scale.z;
    
    // Rotation (YXZ order like Godot)
    vec3 rad = GetRotationRadians();
    float cx = std::cos(rad.x), sx = std::sin(rad.x);
    float cy = std::cos(rad.y), sy = std::sin(rad.y);
    float cz = std::cos(rad.z), sz = std::sin(rad.z);
    
    mat4 rot_mat = mat4::Identity();
    rot_mat.columns[0].x = cy * cz + sy * sx * sz;
    rot_mat.columns[0].y = cz * sy * sx - cy * sz;
    rot_mat.columns[0].z = cx * sy;
    rot_mat.columns[1].x = cx * sz;
    rot_mat.columns[1].y = cx * cz;
    rot_mat.columns[1].z = -sx;
    rot_mat.columns[2].x = cy * sx * sz - cz * sy;
    rot_mat.columns[2].y = sy * sz + cy * cz * sx;
    rot_mat.columns[2].z = cy * cx;
    
    // Translation
    mat4 trans_mat = mat4::Identity();
    trans_mat.columns[3].x = m_position.x;
    trans_mat.columns[3].y = m_position.y;
    trans_mat.columns[3].z = m_position.z;
    
    // TRS order: result = T * R * S
    result = trans_mat * rot_mat * scale_mat;
    
    return result;
}

mat4 Node3D::GetGlobalTransform() const {
    UpdateGlobalTransform();
    return m_global_transform;
}

void Node3D::SetLocalTransform(const mat4& transform) {
    // Extract translation
    m_position = vec3{transform.columns[3].x, transform.columns[3].y, transform.columns[3].z};
    
    // Extract scale (length of basis vectors)
    m_scale.x = std::sqrt(transform.columns[0].x * transform.columns[0].x + 
                          transform.columns[0].y * transform.columns[0].y + 
                          transform.columns[0].z * transform.columns[0].z);
    m_scale.y = std::sqrt(transform.columns[1].x * transform.columns[1].x + 
                          transform.columns[1].y * transform.columns[1].y + 
                          transform.columns[1].z * transform.columns[1].z);
    m_scale.z = std::sqrt(transform.columns[2].x * transform.columns[2].x + 
                          transform.columns[2].y * transform.columns[2].y + 
                          transform.columns[2].z * transform.columns[2].z);
    
    // Extract rotation (simplified - assumes uniform scale)
    constexpr float RAD_TO_DEG = 180.0f / 3.14159265359f;
    m_rotation.x = std::atan2(-transform.columns[1].z, transform.columns[2].z) * RAD_TO_DEG;
    m_rotation.y = std::asin(transform.columns[0].z) * RAD_TO_DEG;
    m_rotation.z = std::atan2(-transform.columns[0].y, transform.columns[0].x) * RAD_TO_DEG;
    
    MarkTransformDirty();
}

void Node3D::UpdateGlobalTransform() const {
    if (!m_global_transform_dirty) return;
    
    mat4 local = GetLocalTransform();
    
    Node3D* parent = dynamic_cast<Node3D*>(GetParent());
    if (parent) {
        m_global_transform = parent->GetGlobalTransform() * local;
    } else {
        m_global_transform = local;
    }
    
    m_global_transform_dirty = false;
}

void Node3D::MarkTransformDirty() {
    m_global_transform_dirty = true;
    
    // Mark children dirty too
    for (size_t i = 0; i < GetChildCount(); ++i) {
        if (Node3D* child = dynamic_cast<Node3D*>(GetChild(i).get())) {
            child->MarkTransformDirty();
        }
    }
}

vec3 Node3D::GetForward() const {
    // -Z direction transformed by this node's local rotation.
    // In this engine, columns[i].xyz stores scaled rotation row i.
    // columns[2].xyz / |columns[2].xyz| gives the local Z axis direction.
    mat4 local = GetLocalTransform();
    vec3 z_axis{local.columns[2].x, local.columns[2].y, local.columns[2].z};
    float len = std::sqrt(z_axis.x * z_axis.x + z_axis.y * z_axis.y + z_axis.z * z_axis.z);
    if (len > 0.0001f) {
        float inv_len = 1.0f / len;
        return vec3{-z_axis.x * inv_len, -z_axis.y * inv_len, -z_axis.z * inv_len};
    }
    return vec3{0, 0, -1};
}

vec3 Node3D::GetRight() const {
    mat4 local = GetLocalTransform();
    vec3 x_axis{local.columns[0].x, local.columns[0].y, local.columns[0].z};
    float len = std::sqrt(x_axis.x * x_axis.x + x_axis.y * x_axis.y + x_axis.z * x_axis.z);
    if (len > 0.0001f) {
        float inv_len = 1.0f / len;
        return vec3{x_axis.x * inv_len, x_axis.y * inv_len, x_axis.z * inv_len};
    }
    return vec3{1, 0, 0};
}

vec3 Node3D::GetUp() const {
    mat4 local = GetLocalTransform();
    vec3 y_axis{local.columns[1].x, local.columns[1].y, local.columns[1].z};
    float len = std::sqrt(y_axis.x * y_axis.x + y_axis.y * y_axis.y + y_axis.z * y_axis.z);
    if (len > 0.0001f) {
        float inv_len = 1.0f / len;
        return vec3{y_axis.x * inv_len, y_axis.y * inv_len, y_axis.z * inv_len};
    }
    return vec3{0, 1, 0};
}

vec3 Node3D::GetGlobalForward() const {
    // -Z direction in world space from the global transform
    mat4 transform = GetGlobalTransform();
    return vec3{-transform.columns[2].x, -transform.columns[2].y, -transform.columns[2].z}.Normalized();
}

vec3 Node3D::GetGlobalRight() const {
    mat4 transform = GetGlobalTransform();
    return vec3{transform.columns[0].x, transform.columns[0].y, transform.columns[0].z}.Normalized();
}

vec3 Node3D::GetGlobalUp() const {
    mat4 transform = GetGlobalTransform();
    return vec3{transform.columns[1].x, transform.columns[1].y, transform.columns[1].z}.Normalized();
}

void Node3D::Translate(const vec3& offset) {
    m_position.x += offset.x;
    m_position.y += offset.y;
    m_position.z += offset.z;
    MarkTransformDirty();
}

void Node3D::TranslateLocal(const vec3& offset) {
    // Transform offset by rotation
    mat4 rot = GetLocalTransform();
    vec3 world_offset = {
        rot.columns[0].x * offset.x + rot.columns[1].x * offset.y + rot.columns[2].x * offset.z,
        rot.columns[0].y * offset.x + rot.columns[1].y * offset.y + rot.columns[2].y * offset.z,
        rot.columns[0].z * offset.x + rot.columns[1].z * offset.y + rot.columns[2].z * offset.z
    };
    Translate(world_offset);
}

void Node3D::Rotate(const vec3& euler) {
    m_rotation.x += euler.x;
    m_rotation.y += euler.y;
    m_rotation.z += euler.z;
    MarkTransformDirty();
}

void Node3D::RotateX(float degrees) {
    m_rotation.x += degrees;
    MarkTransformDirty();
}

void Node3D::RotateY(float degrees) {
    m_rotation.y += degrees;
    MarkTransformDirty();
}

void Node3D::RotateZ(float degrees) {
    m_rotation.z += degrees;
    MarkTransformDirty();
}

void Node3D::LookAt(const vec3& target, const vec3& up) {
    vec3 direction = (target - m_position).Normalized();
    
    // Calculate rotation to face direction
    constexpr float RAD_TO_DEG = 180.0f / 3.14159265359f;
    
    // Yaw (Y rotation)
    m_rotation.y = std::atan2(-direction.x, -direction.z) * RAD_TO_DEG;
    
    // Pitch (X rotation)
    float horizontal_dist = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    m_rotation.x = std::atan2(direction.y, horizontal_dist) * RAD_TO_DEG;
    
    // Roll (Z rotation) - keep at 0 for standard look-at
    m_rotation.z = 0;
    
    MarkTransformDirty();
}

vec3 Node3D::ToLocal(const vec3& global_point) const {
    mat4 inv = GetGlobalTransform().Inverse();
    vec4 local = inv * vec4{global_point.x, global_point.y, global_point.z, 1.0f};
    return vec3{local.x, local.y, local.z};
}

vec3 Node3D::ToGlobal(const vec3& local_point) const {
    mat4 transform = GetGlobalTransform();
    vec4 global = transform * vec4{local_point.x, local_point.y, local_point.z, 1.0f};
    return vec3{global.x, global.y, global.z};
}

void Node3D::Serialize(Serializer& s) const {
    Node::Serialize(s);
    // Serialization implementation will be added with serialization system
}

void Node3D::Deserialize(Deserializer& d) {
    Node::Deserialize(d);
    // Deserialization implementation will be added with serialization system
}

// ===== Node2D =====

Node2D::Node2D() : Node("Node2D") {}

Node2D::Node2D(const std::string& name) : Node(name) {}

mat3 Node2D::GetLocalTransform2D() const {
    float rad = m_rotation * (3.14159265359f / 180.0f);
    float c = std::cos(rad);
    float s = std::sin(rad);
    
    // TRS matrix in 2D
    mat3 result;
    result.m[0][0] = c * m_scale.x;
    result.m[0][1] = s * m_scale.x;
    result.m[0][2] = 0;
    result.m[1][0] = -s * m_scale.y;
    result.m[1][1] = c * m_scale.y;
    result.m[1][2] = 0;
    result.m[2][0] = m_position.x;
    result.m[2][1] = m_position.y;
    result.m[2][2] = 1;
    
    return result;
}

mat3 Node2D::GetGlobalTransform2D() const {
    mat3 local = GetLocalTransform2D();
    
    Node2D* parent = dynamic_cast<Node2D*>(GetParent());
    if (parent) {
        return parent->GetGlobalTransform2D() * local;
    }
    
    return local;
}

} // namespace action
