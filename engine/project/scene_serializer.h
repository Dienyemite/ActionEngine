#pragma once

#include "core/types.h"
#include "gameplay/ecs/ecs.h"
#include "physics/collision_shapes.h"
#include "editor/editor.h"
#include "serialization/serialization.h"
#include <string>

namespace action {

/*
 * SceneSerializer - Save and load scene data
 * 
 * Serializes:
 * - Editor node hierarchy (names, transforms, etc.)
 * - ECS entities and components
 * - Component data (Transform, Render, Collider, Rigidbody, etc.)
 */

class SceneSerializer {
public:
    SceneSerializer(ECS& ecs, Editor& editor);
    
    // Save scene to file
    bool SaveScene(const std::string& file_path);
    
    // Load scene from file
    bool LoadScene(const std::string& file_path);
    
    // Clear current scene
    void ClearScene();
    
    // Serialize to/from SerialObject (for embedding)
    SerialObject SerializeScene();
    bool DeserializeScene(const SerialObject& obj);
    
private:
    // Serialize individual components
    SerialObject SerializeTransform(const TransformComponent& transform);
    SerialObject SerializeRender(const RenderComponent& render);
    SerialObject SerializeCollider(const ColliderComponent& collider);
    SerialObject SerializeRigidbody(const RigidbodyComponent& rb);
    SerialObject SerializeTag(const TagComponent& tag);
    SerialObject SerializeVelocity(const VelocityComponent& vel);
    
    // Deserialize components
    void DeserializeTransform(Entity entity, const SerialObject& obj);
    void DeserializeRender(Entity entity, const SerialObject& obj);
    void DeserializeCollider(Entity entity, const SerialObject& obj);
    void DeserializeRigidbody(Entity entity, const SerialObject& obj);
    void DeserializeTag(Entity entity, const SerialObject& obj);
    void DeserializeVelocity(Entity entity, const SerialObject& obj);
    
    // Serialize editor node hierarchy
    SerialObject SerializeEditorNode(const EditorNode& node);
    EditorNode DeserializeEditorNode(const SerialObject& obj);
    
    // Serialize entity with all components
    SerialObject SerializeEntity(Entity entity);
    Entity DeserializeEntity(const SerialObject& obj);
    
    ECS& m_ecs;
    Editor& m_editor;
};

} // namespace action
