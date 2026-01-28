#pragma once

#include "node.h"
#include "core/math/math.h"

namespace action {

/*
 * Node3D - Base class for 3D spatial nodes (Godot-style)
 * 
 * Features:
 * - 3D transform (position, rotation, scale)
 * - Local and global transforms
 * - Basis/quaternion rotation
 */

class Node3D : public Node {
public:
    Node3D();
    explicit Node3D(const std::string& name);
    virtual ~Node3D() = default;
    
    std::string GetTypeName() const override { return "Node3D"; }
    static std::string GetStaticTypeName() { return "Node3D"; }
    
    // ===== Local Transform =====
    vec3 GetPosition() const override { return m_position; }
    void SetPosition(const vec3& pos) override;
    
    vec3 GetRotation() const { return m_rotation; }  // Euler angles (degrees)
    void SetRotation(const vec3& euler);
    
    vec3 GetRotationRadians() const;
    void SetRotationRadians(const vec3& radians);
    
    vec3 GetScale() const { return m_scale; }
    void SetScale(const vec3& scale);
    
    // ===== Global Transform =====
    vec3 GetGlobalPosition() const;
    void SetGlobalPosition(const vec3& pos);
    
    vec3 GetGlobalRotation() const;
    void SetGlobalRotation(const vec3& euler);
    
    vec3 GetGlobalScale() const;
    
    // ===== Transform Matrix =====
    mat4 GetLocalTransform() const;
    mat4 GetGlobalTransform() const;
    void SetLocalTransform(const mat4& transform);
    
    // ===== Direction Vectors =====
    vec3 GetForward() const;   // -Z in local space
    vec3 GetRight() const;     // +X in local space
    vec3 GetUp() const;        // +Y in local space
    
    vec3 GetGlobalForward() const;
    vec3 GetGlobalRight() const;
    vec3 GetGlobalUp() const;
    
    // ===== Transform Operations =====
    void Translate(const vec3& offset);
    void TranslateLocal(const vec3& offset);  // In local space
    void Rotate(const vec3& euler);           // Add to rotation
    void RotateX(float degrees);
    void RotateY(float degrees);
    void RotateZ(float degrees);
    void LookAt(const vec3& target, const vec3& up = vec3{0, 1, 0});
    
    // ===== Transform Conversion =====
    vec3 ToLocal(const vec3& global_point) const;
    vec3 ToGlobal(const vec3& local_point) const;
    
    // ===== Visibility in 3D =====
    bool IsVisibleInFrustum() const { return m_visible_in_frustum; }
    void SetVisibleInFrustum(bool visible) { m_visible_in_frustum = visible; }
    
    // ===== Serialization =====
    void Serialize(class Serializer& s) const override;
    void Deserialize(class Deserializer& d) override;
    
protected:
    void MarkTransformDirty();
    void UpdateGlobalTransform() const;
    
    vec3 m_position{0, 0, 0};
    vec3 m_rotation{0, 0, 0};  // Euler angles in degrees
    vec3 m_scale{1, 1, 1};
    
    // Cached global transform
    mutable mat4 m_global_transform;
    mutable bool m_global_transform_dirty = true;
    
    // Frustum culling result
    bool m_visible_in_frustum = true;
};

/*
 * Node2D - Base class for 2D spatial nodes
 */
class Node2D : public Node {
public:
    Node2D();
    explicit Node2D(const std::string& name);
    virtual ~Node2D() = default;
    
    std::string GetTypeName() const override { return "Node2D"; }
    static std::string GetStaticTypeName() { return "Node2D"; }
    
    // Position (z is always 0)
    vec3 GetPosition() const override { return vec3{m_position.x, m_position.y, 0}; }
    void SetPosition(const vec3& pos) override { m_position = {pos.x, pos.y}; }
    
    vec2 GetPosition2D() const { return m_position; }
    void SetPosition2D(const vec2& pos) { m_position = pos; }
    
    float GetRotation() const { return m_rotation; }  // Degrees
    void SetRotation(float degrees) { m_rotation = degrees; }
    
    vec2 GetScale() const { return m_scale; }
    void SetScale(const vec2& scale) { m_scale = scale; }
    
    // Z-index for draw order
    int GetZIndex() const { return m_z_index; }
    void SetZIndex(int z) { m_z_index = z; }
    bool IsZRelative() const { return m_z_relative; }
    void SetZRelative(bool relative) { m_z_relative = relative; }
    
    // Transform
    mat3 GetLocalTransform2D() const;
    mat3 GetGlobalTransform2D() const;
    
protected:
    vec2 m_position{0, 0};
    float m_rotation = 0;  // Degrees
    vec2 m_scale{1, 1};
    int m_z_index = 0;
    bool m_z_relative = true;
};

} // namespace action
